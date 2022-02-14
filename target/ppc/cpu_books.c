/*
 * CPU initialization and exception dispatching for PowerPC BookS CPUs
 *
 *  Copyright IBM Corp. 2022
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "sysemu/hw_accel.h"
#include "sysemu/tcg.h"
#include "hw/ppc/ppc.h"
#include "fpu/softfloat.h"
#include "cpu.h"
#include "spr_common.h"
#include "power8-pmu.h"
#include "cpu-models.h"
#include "mmu-hash64.h"
#include "trace.h"
#include "helper_regs.h"

#if defined(CONFIG_USER_ONLY)
#define POWERPC970_HID5_INIT 0x00000080
#else
#define POWERPC970_HID5_INIT 0x00000000


static int powerpc_reset_wakeup(CPUState *cs, CPUPPCState *env, int excp,
                                target_ulong *msr)
{
    /* We no longer are in a PM state */
    env->resume_as_sreset = false;

    /* Pretend to be returning from doze always as we don't lose state */
    *msr |= SRR1_WS_NOLOSS;

    /* Machine checks are sent normally */
    if (excp == POWERPC_EXCP_MCHECK) {
        return excp;
    }
    switch (excp) {
    case POWERPC_EXCP_RESET:
        *msr |= SRR1_WAKERESET;
        break;
    case POWERPC_EXCP_EXTERNAL:
        *msr |= SRR1_WAKEEE;
        break;
    case POWERPC_EXCP_DECR:
        *msr |= SRR1_WAKEDEC;
        break;
    case POWERPC_EXCP_SDOOR:
        *msr |= SRR1_WAKEDBELL;
        break;
    case POWERPC_EXCP_SDOOR_HV:
        *msr |= SRR1_WAKEHDBELL;
        break;
    case POWERPC_EXCP_HV_MAINT:
        *msr |= SRR1_WAKEHMI;
        break;
    case POWERPC_EXCP_HVIRT:
        *msr |= SRR1_WAKEHVI;
        break;
    default:
        cpu_abort(cs, "Unsupported exception %d in Power Save mode\n",
                  excp);
    }
    return POWERPC_EXCP_RESET;
}

/*
 * AIL - Alternate Interrupt Location, a mode that allows interrupts to be
 * taken with the MMU on, and which uses an alternate location (e.g., so the
 * kernel/hv can map the vectors there with an effective address).
 *
 * An interrupt is considered to be taken "with AIL" or "AIL applies" if they
 * are delivered in this way. AIL requires the LPCR to be set to enable this
 * mode, and then a number of conditions have to be true for AIL to apply.
 *
 * First of all, SRESET, MCE, and HMI are always delivered without AIL, because
 * they specifically want to be in real mode (e.g., the MCE might be signaling
 * a SLB multi-hit which requires SLB flush before the MMU can be enabled).
 *
 * After that, behaviour depends on the current MSR[IR], MSR[DR], MSR[HV],
 * whether or not the interrupt changes MSR[HV] from 0 to 1, and the current
 * radix mode (LPCR[HR]).
 *
 * POWER8, POWER9 with LPCR[HR]=0
 * | LPCR[AIL] | MSR[IR||DR] | MSR[HV] | new MSR[HV] | AIL |
 * +-----------+-------------+---------+-------------+-----+
 * | a         | 00/01/10    | x       | x           | 0   |
 * | a         | 11          | 0       | 1           | 0   |
 * | a         | 11          | 1       | 1           | a   |
 * | a         | 11          | 0       | 0           | a   |
 * +-------------------------------------------------------+
 *
 * POWER9 with LPCR[HR]=1
 * | LPCR[AIL] | MSR[IR||DR] | MSR[HV] | new MSR[HV] | AIL |
 * +-----------+-------------+---------+-------------+-----+
 * | a         | 00/01/10    | x       | x           | 0   |
 * | a         | 11          | x       | x           | a   |
 * +-------------------------------------------------------+
 *
 * The difference with POWER9 being that MSR[HV] 0->1 interrupts can be sent to
 * the hypervisor in AIL mode if the guest is radix. This is good for
 * performance but allows the guest to influence the AIL of hypervisor
 * interrupts using its MSR, and also the hypervisor must disallow guest
 * interrupts (MSR[HV] 0->0) from using AIL if the hypervisor does not want to
 * use AIL for its MSR[HV] 0->1 interrupts.
 *
 * POWER10 addresses those issues with a new LPCR[HAIL] bit that is applied to
 * interrupts that begin execution with MSR[HV]=1 (so both MSR[HV] 0->1 and
 * MSR[HV] 1->1).
 *
 * HAIL=1 is equivalent to AIL=3, for interrupts delivered with MSR[HV]=1.
 *
 * POWER10 behaviour is
 * | LPCR[AIL] | LPCR[HAIL] | MSR[IR||DR] | MSR[HV] | new MSR[HV] | AIL |
 * +-----------+------------+-------------+---------+-------------+-----+
 * | a         | h          | 00/01/10    | 0       | 0           | 0   |
 * | a         | h          | 11          | 0       | 0           | a   |
 * | a         | h          | x           | 0       | 1           | h   |
 * | a         | h          | 00/01/10    | 1       | 1           | 0   |
 * | a         | h          | 11          | 1       | 1           | h   |
 * +--------------------------------------------------------------------+
 */
static void ppc_excp_apply_ail(PowerPCCPU *cpu, int excp, target_ulong msr,
                               target_ulong *new_msr, target_ulong *vector)
{
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    CPUPPCState *env = &cpu->env;
    bool mmu_all_on = ((msr >> MSR_IR) & 1) && ((msr >> MSR_DR) & 1);
    bool hv_escalation = !(msr & MSR_HVB) && (*new_msr & MSR_HVB);
    int ail = 0;

    if (excp == POWERPC_EXCP_MCHECK ||
        excp == POWERPC_EXCP_RESET ||
        excp == POWERPC_EXCP_HV_MAINT) {
        /* SRESET, MCE, HMI never apply AIL */
        return;
    }

    if (!(pcc->lpcr_mask & LPCR_AIL)) {
        /* This CPU does not have AIL */
        return;
    }

    /* P8 & P9 */
    if (!(pcc->lpcr_mask & LPCR_HAIL)) {
        if (!mmu_all_on) {
            /* AIL only works if MSR[IR] and MSR[DR] are both enabled. */
            return;
        }
        if (hv_escalation && !(env->spr[SPR_LPCR] & LPCR_HR)) {
            /*
             * AIL does not work if there is a MSR[HV] 0->1 transition and the
             * partition is in HPT mode. For radix guests, such interrupts are
             * allowed to be delivered to the hypervisor in ail mode.
             */
            return;
        }

        ail = (env->spr[SPR_LPCR] & LPCR_AIL) >> LPCR_AIL_SHIFT;
        if (ail == 0) {
            return;
        }
        if (ail == 1) {
            /* AIL=1 is reserved, treat it like AIL=0 */
            return;
        }

    /* P10 and up */
    } else {
        if (!mmu_all_on && !hv_escalation) {
            /*
             * AIL works for HV interrupts even with guest MSR[IR/DR] disabled.
             * Guest->guest and HV->HV interrupts do require MMU on.
             */
            return;
        }

        if (*new_msr & MSR_HVB) {
            if (!(env->spr[SPR_LPCR] & LPCR_HAIL)) {
                /* HV interrupts depend on LPCR[HAIL] */
                return;
            }
            ail = 3; /* HAIL=1 gives AIL=3 behaviour for HV interrupts */
        } else {
            ail = (env->spr[SPR_LPCR] & LPCR_AIL) >> LPCR_AIL_SHIFT;
        }
        if (ail == 0) {
            return;
        }
        if (ail == 1 || ail == 2) {
            /* AIL=1 and AIL=2 are reserved, treat them like AIL=0 */
            return;
        }
    }

    /*
     * AIL applies, so the new MSR gets IR and DR set, and an offset applied
     * to the new IP.
     */
    *new_msr |= (1 << MSR_IR) | (1 << MSR_DR);

    if (excp != POWERPC_EXCP_SYSCALL_VECTORED) {
        if (ail == 2) {
            *vector |= 0x0000000000018000ull;
        } else if (ail == 3) {
            *vector |= 0xc000000000004000ull;
        }
    } else {
        /*
         * scv AIL is a little different. AIL=2 does not change the address,
         * only the MSR. AIL=3 replaces the 0x17000 base with 0xc...3000.
         */
        if (ail == 3) {
            *vector &= ~0x0000000000017000ull; /* Un-apply the base offset */
            *vector |= 0xc000000000003000ull; /* Apply scv's AIL=3 offset */
        }
    }
}

/*
 * When running under vhyp, hcalls are always intercepted and sent to the
 * vhc->hypercall handler.
 */
static bool books_vhyp_handles_hcall(PowerPCCPU *cpu)
{
    if (cpu->vhyp) {
        return !vhyp_cpu_in_nested(cpu);
    }
    return false;
}

/*
 * When running a nested KVM HV guest under vhyp, HV exceptions are not
 * delivered to the guest (because there is no concept of HV support), but
 * rather they are sent tothe vhyp to exit from the L2 back to the L1 and
 * return from the H_ENTER_NESTED hypercall.
 */
static bool books_vhyp_handles_hv_excp(PowerPCCPU *cpu)
{
    if (cpu->vhyp) {
        return vhyp_cpu_in_nested(cpu);
    }
    return false;
}

static void powerpc_excp_books(PowerPCCPU *cpu, int excp)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;
    target_ulong msr, new_msr, vector;
    int srr0, srr1, lev = -1;

    /* new srr1 value excluding must-be-zero bits */
    msr = env->msr & ~0x783f0000ULL;

    /*
     * new interrupt handler msr preserves existing HV and ME unless
     * explicitly overriden
     */
    new_msr = env->msr & (((target_ulong)1 << MSR_ME) | MSR_HVB);

    /* target registers */
    srr0 = SPR_SRR0;
    srr1 = SPR_SRR1;

    /*
     * check for special resume at 0x100 from doze/nap/sleep/winkle on
     * P7/P8/P9
     */
    if (env->resume_as_sreset) {
        excp = powerpc_reset_wakeup(cs, env, excp, &msr);
    }

    /*
     * We don't want to generate a Hypervisor Emulation Assistance
     * Interrupt if we don't have HVB in msr_mask (PAPR mode).
     */
    if (excp == POWERPC_EXCP_HV_EMU && !(env->msr_mask & MSR_HVB)) {
        excp = POWERPC_EXCP_PROGRAM;
    }

    vector = env->excp_vectors[excp];
    if (vector == (target_ulong)-1ULL) {
        cpu_abort(cs, "Raised an exception without defined vector %d\n",
                  excp);
    }

    vector |= env->excp_prefix;

    switch (excp) {
    case POWERPC_EXCP_MCHECK:    /* Machine check exception                  */
        if (msr_me == 0) {
            /*
             * Machine check exception is not enabled.  Enter
             * checkstop state.
             */
            fprintf(stderr, "Machine check while not allowed. "
                    "Entering checkstop state\n");
            if (qemu_log_separate()) {
                qemu_log("Machine check while not allowed. "
                        "Entering checkstop state\n");
            }
            cs->halted = 1;
            cpu_interrupt_exittb(cs);
        }
        if (env->msr_mask & MSR_HVB) {
            /*
             * ISA specifies HV, but can be delivered to guest with HV
             * clear (e.g., see FWNMI in PAPR).
             */
            new_msr |= (target_ulong)MSR_HVB;
        }

        /* machine check exceptions don't have ME set */
        new_msr &= ~((target_ulong)1 << MSR_ME);

        break;
    case POWERPC_EXCP_DSI:       /* Data storage exception                   */
        trace_ppc_excp_dsi(env->spr[SPR_DSISR], env->spr[SPR_DAR]);
        break;
    case POWERPC_EXCP_ISI:       /* Instruction storage exception            */
        trace_ppc_excp_isi(msr, env->nip);
        msr |= env->error_code;
        break;
    case POWERPC_EXCP_EXTERNAL:  /* External input                           */
    {
        bool lpes0;

        /*
         * LPES0 is only taken into consideration if we support HV
         * mode for this CPU.
         */
        if (!env->has_hv_mode) {
            break;
        }

        lpes0 = !!(env->spr[SPR_LPCR] & LPCR_LPES0);

        if (!lpes0) {
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
        }

        break;
    }
    case POWERPC_EXCP_ALIGN:     /* Alignment exception                      */
        /* Get rS/rD and rA from faulting opcode */
        /*
         * Note: the opcode fields will not be set properly for a
         * direct store load/store, but nobody cares as nobody
         * actually uses direct store segments.
         */
        env->spr[SPR_DSISR] |= (env->error_code & 0x03FF0000) >> 16;
        break;
    case POWERPC_EXCP_PROGRAM:   /* Program exception                        */
        switch (env->error_code & ~0xF) {
        case POWERPC_EXCP_FP:
            if ((msr_fe0 == 0 && msr_fe1 == 0) || msr_fp == 0) {
                trace_ppc_excp_fp_ignore();
                powerpc_reset_excp_state(cpu);
                return;
            }

            /*
             * FP exceptions always have NIP pointing to the faulting
             * instruction, so always use store_next and claim we are
             * precise in the MSR.
             */
            msr |= 0x00100000;
            break;
        case POWERPC_EXCP_INVAL:
            trace_ppc_excp_inval(env->nip);
            msr |= 0x00080000;
            break;
        case POWERPC_EXCP_PRIV:
            msr |= 0x00040000;
            break;
        case POWERPC_EXCP_TRAP:
            msr |= 0x00020000;
            break;
        default:
            /* Should never occur */
            cpu_abort(cs, "Invalid program exception %d. Aborting\n",
                      env->error_code);
            break;
        }
        break;
    case POWERPC_EXCP_SYSCALL:   /* System call exception                    */
        lev = env->error_code;

        trace_ppc_syscall(env, lev);

        /*
         * We need to correct the NIP which in this case is supposed
         * to point to the next instruction
         */
        env->nip += 4;

        /* "PAPR mode" built-in hypercall emulation */
        if ((lev == 1) && books_vhyp_handles_hcall(cpu)) {
            PPCVirtualHypervisorClass *vhc =
                PPC_VIRTUAL_HYPERVISOR_GET_CLASS(cpu->vhyp);
            vhc->hypercall(cpu->vhyp, cpu);
            return;
        }
        if (lev == 1) {
            new_msr |= (target_ulong)MSR_HVB;
        }
        break;
    case POWERPC_EXCP_SYSCALL_VECTORED: /* scv exception                     */
        lev = env->error_code;
        trace_ppc_syscall(env, 0);
        env->nip += 4;
        new_msr |= env->msr & ((target_ulong)1 << MSR_EE);
        new_msr |= env->msr & ((target_ulong)1 << MSR_RI);

        vector += lev * 0x20;

        env->lr = env->nip;
        env->ctr = msr;
        break;
    case POWERPC_EXCP_FPU:       /* Floating-point unavailable exception     */
    case POWERPC_EXCP_DECR:      /* Decrementer exception                    */
        break;
    case POWERPC_EXCP_RESET:     /* System reset exception                   */
        /* A power-saving exception sets ME, otherwise it is unchanged */
        if (msr_pow) {
            /* indicate that we resumed from power save mode */
            msr |= 0x10000;
            new_msr |= ((target_ulong)1 << MSR_ME);
        }
        if (env->msr_mask & MSR_HVB) {
            /*
             * ISA specifies HV, but can be delivered to guest with HV
             * clear (e.g., see FWNMI in PAPR, NMI injection in QEMU).
             */
            new_msr |= (target_ulong)MSR_HVB;
        } else {
            if (msr_pow) {
                cpu_abort(cs, "Trying to deliver power-saving system reset "
                          "exception %d with no HV support\n", excp);
            }
        }
        break;
    case POWERPC_EXCP_DSEG:      /* Data segment exception                   */
    case POWERPC_EXCP_ISEG:      /* Instruction segment exception            */
    case POWERPC_EXCP_TRACE:     /* Trace exception                          */
        break;
    case POWERPC_EXCP_HISI:      /* Hypervisor instruction storage exception */
        msr |= env->error_code;
        /* fall through */
    case POWERPC_EXCP_HDECR:     /* Hypervisor decrementer exception         */
    case POWERPC_EXCP_HDSI:      /* Hypervisor data storage exception        */
    case POWERPC_EXCP_SDOOR_HV:  /* Hypervisor Doorbell interrupt            */
    case POWERPC_EXCP_HV_EMU:
    case POWERPC_EXCP_HVIRT:     /* Hypervisor virtualization                */
        srr0 = SPR_HSRR0;
        srr1 = SPR_HSRR1;
        new_msr |= (target_ulong)MSR_HVB;
        new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
        break;
    case POWERPC_EXCP_VPU:       /* Vector unavailable exception             */
    case POWERPC_EXCP_VSXU:       /* VSX unavailable exception               */
    case POWERPC_EXCP_FU:         /* Facility unavailable exception          */
        env->spr[SPR_FSCR] |= ((target_ulong)env->error_code << 56);
        break;
    case POWERPC_EXCP_HV_FU:     /* Hypervisor Facility Unavailable Exception */
        env->spr[SPR_HFSCR] |= ((target_ulong)env->error_code << FSCR_IC_POS);
        srr0 = SPR_HSRR0;
        srr1 = SPR_HSRR1;
        new_msr |= (target_ulong)MSR_HVB;
        new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
        break;
    case POWERPC_EXCP_PERFM_EBB:        /* Performance Monitor EBB Exception  */
    case POWERPC_EXCP_EXTERNAL_EBB:     /* External EBB Exception             */
        env->spr[SPR_BESCR] &= ~BESCR_GE;

        /*
         * Save NIP for rfebb insn in SPR_EBBRR. Next nip is
         * stored in the EBB Handler SPR_EBBHR.
         */
        env->spr[SPR_EBBRR] = env->nip;
        powerpc_set_excp_state(cpu, env->spr[SPR_EBBHR], env->msr);

        /*
         * This exception is handled in userspace. No need to proceed.
         */
        return;
    case POWERPC_EXCP_THERM:     /* Thermal interrupt                        */
    case POWERPC_EXCP_PERFM:     /* Embedded performance monitor interrupt   */
    case POWERPC_EXCP_VPUA:      /* Vector assist exception                  */
    case POWERPC_EXCP_MAINT:     /* Maintenance exception                    */
    case POWERPC_EXCP_SDOOR:     /* Doorbell interrupt                       */
    case POWERPC_EXCP_HV_MAINT:  /* Hypervisor Maintenance exception         */
        cpu_abort(cs, "%s exception not implemented\n",
                  powerpc_excp_name(excp));
        break;
    default:
        cpu_abort(cs, "Invalid PowerPC exception %d. Aborting\n", excp);
        break;
    }

    /*
     * Sort out endianness of interrupt, this differs depending on the
     * CPU, the HV mode, etc...
     */
    if (ppc_interrupts_little_endian(cpu, !!(new_msr & MSR_HVB))) {
        new_msr |= (target_ulong)1 << MSR_LE;
    }

    new_msr |= (target_ulong)1 << MSR_SF;

    if (excp != POWERPC_EXCP_SYSCALL_VECTORED) {
        /* Save PC */
        env->spr[srr0] = env->nip;

        /* Save MSR */
        env->spr[srr1] = msr;
    }

    if ((new_msr & MSR_HVB) && books_vhyp_handles_hv_excp(cpu)) {
        PPCVirtualHypervisorClass *vhc =
            PPC_VIRTUAL_HYPERVISOR_GET_CLASS(cpu->vhyp);
        /* Deliver interrupt to L1 by returning from the H_ENTER_NESTED call */
        vhc->deliver_hv_excp(cpu, excp);

        powerpc_reset_excp_state(cpu);

    } else {
        /* Sanity check */
        if (!(env->msr_mask & MSR_HVB) && srr0 == SPR_HSRR0) {
            cpu_abort(cs, "Trying to deliver HV exception (HSRR) %d with "
                      "no HV support\n", excp);
        }

        /* This can update new_msr and vector if AIL applies */
        ppc_excp_apply_ail(cpu, excp, msr, &new_msr, &vector);

        powerpc_set_excp_state(cpu, vector, new_msr);
    }
}

static inline void vscr_init(CPUPPCState *env, uint32_t val)
{
    /* Altivec always uses round-to-nearest */
    set_float_rounding_mode(float_round_nearest_even, &env->vec_status);
    ppc_store_vscr(env, val);
}

#ifdef CONFIG_SOFTMMU
/*
 * Radix pg sizes and AP encodings for dt node ibm,processor-radix-AP-encodings
 * Encoded as array of int_32s in the form:
 *  0bxxxyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
 *  x -> AP encoding
 *  y -> radix mode supported page size (encoded as a shift)
 */
static struct ppc_radix_page_info POWER9_radix_page_info = {
    .count = 4,
    .entries = {
        0x0000000c, /*  4K - enc: 0x0 */
        0xa0000010, /* 64K - enc: 0x5 */
        0x20000015, /*  2M - enc: 0x1 */
        0x4000001e  /*  1G - enc: 0x2 */
    }
};

/*
 * Radix pg sizes and AP encodings for dt node ibm,processor-radix-AP-encodings
 * Encoded as array of int_32s in the form:
 *  0bxxxyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
 *  x -> AP encoding
 *  y -> radix mode supported page size (encoded as a shift)
 */
static struct ppc_radix_page_info POWER10_radix_page_info = {
    .count = 4,
    .entries = {
        0x0000000c, /*  4K - enc: 0x0 */
        0xa0000010, /* 64K - enc: 0x5 */
        0x20000015, /*  2M - enc: 0x1 */
        0x4000001e  /*  1G - enc: 0x2 */
    }
};
#endif /* CONFIG_SOFTMMU */

static bool ppc_pvr_match_power7(PowerPCCPUClass *pcc, uint32_t pvr)
{
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER7P_BASE) {
        return true;
    }
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER7_BASE) {
        return true;
    }
    return false;
}

static bool cpu_has_work_POWER7(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if (cs->halted) {
        if (!(cs->interrupt_request & CPU_INTERRUPT_HARD)) {
            return false;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_EXT)) &&
            (env->spr[SPR_LPCR] & LPCR_P7_PECE0)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DECR)) &&
            (env->spr[SPR_LPCR] & LPCR_P7_PECE1)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_MCK)) &&
            (env->spr[SPR_LPCR] & LPCR_P7_PECE2)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HMI)) &&
            (env->spr[SPR_LPCR] & LPCR_P7_PECE2)) {
            return true;
        }
        if (env->pending_interrupts & (1u << PPC_INTERRUPT_RESET)) {
            return true;
        }
        return false;
    } else {
        return msr_ee && (cs->interrupt_request & CPU_INTERRUPT_HARD);
    }
}

static bool ppc_pvr_match_power8(PowerPCCPUClass *pcc, uint32_t pvr)
{
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER8NVL_BASE) {
        return true;
    }
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER8E_BASE) {
        return true;
    }
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER8_BASE) {
        return true;
    }
    return false;
}

static bool cpu_has_work_POWER8(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if (cs->halted) {
        if (!(cs->interrupt_request & CPU_INTERRUPT_HARD)) {
            return false;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_EXT)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE2)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DECR)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE3)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_MCK)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE4)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HMI)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE4)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE0)) {
            return true;
        }
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HDOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_P8_PECE1)) {
            return true;
        }
        if (env->pending_interrupts & (1u << PPC_INTERRUPT_RESET)) {
            return true;
        }
        return false;
    } else {
        return msr_ee && (cs->interrupt_request & CPU_INTERRUPT_HARD);
    }
}

static bool ppc_pvr_match_power9(PowerPCCPUClass *pcc, uint32_t pvr)
{
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER9_BASE) {
        return true;
    }
    return false;
}

static bool cpu_has_work_POWER9(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if (cs->halted) {
        uint64_t psscr = env->spr[SPR_PSSCR];

        if (!(cs->interrupt_request & CPU_INTERRUPT_HARD)) {
            return false;
        }

        /* If EC is clear, just return true on any pending interrupt */
        if (!(psscr & PSSCR_EC)) {
            return true;
        }
        /* External Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_EXT)) &&
            (env->spr[SPR_LPCR] & LPCR_EEE)) {
            bool heic = !!(env->spr[SPR_LPCR] & LPCR_HEIC);
            if (heic == 0 || !msr_hv || msr_pr) {
                return true;
            }
        }
        /* Decrementer Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DECR)) &&
            (env->spr[SPR_LPCR] & LPCR_DEE)) {
            return true;
        }
        /* Machine Check or Hypervisor Maintenance Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_MCK |
            1u << PPC_INTERRUPT_HMI)) && (env->spr[SPR_LPCR] & LPCR_OEE)) {
            return true;
        }
        /* Privileged Doorbell Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_PDEE)) {
            return true;
        }
        /* Hypervisor Doorbell Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HDOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_HDEE)) {
            return true;
        }
        /* Hypervisor virtualization exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HVIRT)) &&
            (env->spr[SPR_LPCR] & LPCR_HVEE)) {
            return true;
        }
        if (env->pending_interrupts & (1u << PPC_INTERRUPT_RESET)) {
            return true;
        }
        return false;
    } else {
        return msr_ee && (cs->interrupt_request & CPU_INTERRUPT_HARD);
    }
}

static bool ppc_pvr_match_power10(PowerPCCPUClass *pcc, uint32_t pvr)
{
    if ((pvr & CPU_POWERPC_POWER_SERVER_MASK) == CPU_POWERPC_POWER10_BASE) {
        return true;
    }
    return false;
}

static bool cpu_has_work_POWER10(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if (cs->halted) {
        uint64_t psscr = env->spr[SPR_PSSCR];

        if (!(cs->interrupt_request & CPU_INTERRUPT_HARD)) {
            return false;
        }

        /* If EC is clear, just return true on any pending interrupt */
        if (!(psscr & PSSCR_EC)) {
            return true;
        }
        /* External Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_EXT)) &&
            (env->spr[SPR_LPCR] & LPCR_EEE)) {
            bool heic = !!(env->spr[SPR_LPCR] & LPCR_HEIC);
            if (heic == 0 || !msr_hv || msr_pr) {
                return true;
            }
        }
        /* Decrementer Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DECR)) &&
            (env->spr[SPR_LPCR] & LPCR_DEE)) {
            return true;
        }
        /* Machine Check or Hypervisor Maintenance Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_MCK |
            1u << PPC_INTERRUPT_HMI)) && (env->spr[SPR_LPCR] & LPCR_OEE)) {
            return true;
        }
        /* Privileged Doorbell Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_DOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_PDEE)) {
            return true;
        }
        /* Hypervisor Doorbell Exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HDOORBELL)) &&
            (env->spr[SPR_LPCR] & LPCR_HDEE)) {
            return true;
        }
        /* Hypervisor virtualization exception */
        if ((env->pending_interrupts & (1u << PPC_INTERRUPT_HVIRT)) &&
            (env->spr[SPR_LPCR] & LPCR_HVEE)) {
            return true;
        }
        if (env->pending_interrupts & (1u << PPC_INTERRUPT_RESET)) {
            return true;
        }
        return false;
    } else {
        return msr_ee && (cs->interrupt_request & CPU_INTERRUPT_HARD);
    }
}

static void init_excp_970(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_DSEG]     = 0x00000380;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_ISEG]     = 0x00000480;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_HDECR]    = 0x00000980;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_VPU]      = 0x00000F20;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_MAINT]    = 0x00001600;
    env->excp_vectors[POWERPC_EXCP_VPUA]     = 0x00001700;
    env->excp_vectors[POWERPC_EXCP_THERM]    = 0x00001800;
    /* Hardware reset vector */
    env->hreset_vector = 0x0000000000000100ULL;
#endif
}

static void init_excp_POWER7(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_DSEG]     = 0x00000380;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_ISEG]     = 0x00000480;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_HDECR]    = 0x00000980;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_HDSI]     = 0x00000E00;
    env->excp_vectors[POWERPC_EXCP_HISI]     = 0x00000E20;
    env->excp_vectors[POWERPC_EXCP_HV_EMU]   = 0x00000E40;
    env->excp_vectors[POWERPC_EXCP_HV_MAINT] = 0x00000E60;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_VPU]      = 0x00000F20;
    env->excp_vectors[POWERPC_EXCP_VSXU]     = 0x00000F40;
    /* Hardware reset vector */
    env->hreset_vector = 0x0000000000000100ULL;
#endif
}

static void init_excp_POWER8(CPUPPCState *env)
{
    init_excp_POWER7(env);

#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_SDOOR]    = 0x00000A00;
    env->excp_vectors[POWERPC_EXCP_FU]       = 0x00000F60;
    env->excp_vectors[POWERPC_EXCP_HV_FU]    = 0x00000F80;
    env->excp_vectors[POWERPC_EXCP_SDOOR_HV] = 0x00000E80;

    /* Userland exceptions without vector value in PowerISA v3.1 */
    env->excp_vectors[POWERPC_EXCP_PERFM_EBB] = 0x0;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL_EBB] = 0x0;
#endif
}

static void init_excp_POWER9(CPUPPCState *env)
{
    init_excp_POWER8(env);

#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_HVIRT]    = 0x00000EA0;
    env->excp_vectors[POWERPC_EXCP_SYSCALL_VECTORED] = 0x00017000;
#endif
}

static void init_excp_POWER10(CPUPPCState *env)
{
    init_excp_POWER9(env);
}

#endif

static int check_pow_970(CPUPPCState *env)
{
    if (env->spr[SPR_HID0] & (HID0_DEEPNAP | HID0_DOZE | HID0_NAP)) {
        return 1;
    }

    return 0;
}

static void register_amr_sprs(CPUPPCState *env)
{
#ifndef CONFIG_USER_ONLY
    /*
     * Virtual Page Class Key protection
     *
     * The AMR is accessible either via SPR 13 or SPR 29.  13 is
     * userspace accessible, 29 is privileged.  So we only need to set
     * the kvm ONE_REG id on one of them, we use 29
     */
    spr_register(env, SPR_UAMR, "UAMR",
                 &spr_read_generic, &spr_write_amr,
                 &spr_read_generic, &spr_write_amr,
                 0);
    spr_register_kvm_hv(env, SPR_AMR, "AMR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_amr,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_AMR, 0);
    spr_register_kvm_hv(env, SPR_UAMOR, "UAMOR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_uamor,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_UAMOR, 0);
    spr_register_hv(env, SPR_AMOR, "AMOR",
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, SPR_NOACCESS,
                    &spr_read_generic, &spr_write_generic,
                    0);
#endif /* !CONFIG_USER_ONLY */
}

static void register_iamr_sprs(CPUPPCState *env)
{
#ifndef CONFIG_USER_ONLY
    spr_register_kvm_hv(env, SPR_IAMR, "IAMR",
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_iamr,
                        &spr_read_generic, &spr_write_generic,
                        KVM_REG_PPC_IAMR, 0);
#endif /* !CONFIG_USER_ONLY */
}

static void register_970_hid_sprs(CPUPPCState *env)
{
    /* Hardware implementation registers */
    spr_register(env, SPR_HID0, "HID0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_clear,
                 0x60000000);
    spr_register(env, SPR_HID1, "HID1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_970_HID5, "HID5",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 POWERPC970_HID5_INIT);
}

static void register_970_hior_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_HIOR, "SPR_HIOR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_hior, &spr_write_hior,
                 0x00000000);
}

static void register_book3s_ctrl_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_CTRL, "SPR_CTRL",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, &spr_write_CTRL,
                 0x00000000);
    spr_register(env, SPR_UCTRL, "SPR_UCTRL",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);
}

static void register_book3s_altivec_sprs(CPUPPCState *env)
{
    if (!(env->insns_flags & PPC_ALTIVEC)) {
        return;
    }

    spr_register_kvm(env, SPR_VRSAVE, "VRSAVE",
                     &spr_read_generic, &spr_write_generic,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_VRSAVE, 0x00000000);

}

static void register_book3s_dbg_sprs(CPUPPCState *env)
{
    /*
     * TODO: different specs define different scopes for these,
     * will have to address this:
     * 970: super/write and super/read
     * powerisa 2.03..2.04: hypv/write and super/read.
     * powerisa 2.05 and newer: hypv/write and hypv/read.
     */
    spr_register_kvm(env, SPR_DABR, "DABR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DABR, 0x00000000);
    spr_register_kvm(env, SPR_DABRX, "DABRX",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DABRX, 0x00000000);
}

static void register_book3s_207_dbg_sprs(CPUPPCState *env)
{
    spr_register_kvm_hv(env, SPR_DAWR0, "DAWR0",
                        SPR_NOACCESS, SPR_NOACCESS,
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_generic,
                        KVM_REG_PPC_DAWR, 0x00000000);
    spr_register_kvm_hv(env, SPR_DAWRX0, "DAWRX0",
                        SPR_NOACCESS, SPR_NOACCESS,
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_generic,
                        KVM_REG_PPC_DAWRX, 0x00000000);
    spr_register_kvm_hv(env, SPR_CIABR, "CIABR",
                        SPR_NOACCESS, SPR_NOACCESS,
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_generic,
                        KVM_REG_PPC_CIABR, 0x00000000);
}

static void register_970_dbg_sprs(CPUPPCState *env)
{
    /* Breakpoints */
    spr_register(env, SPR_IABR, "IABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_book3s_pmu_sup_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_POWER_MMCR0, "MMCR0",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_MMCR0,
                     KVM_REG_PPC_MMCR0, 0x80000000);
    spr_register_kvm(env, SPR_POWER_MMCR1, "MMCR1",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_MMCR1,
                     KVM_REG_PPC_MMCR1, 0x00000000);
    spr_register_kvm(env, SPR_POWER_MMCRA, "MMCRA",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_MMCRA, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC1, "PMC1",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC1, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC2, "PMC2",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC2, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC3, "PMC3",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC3, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC4, "PMC4",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC4, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC5, "PMC5",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC5, 0x00000000);
    spr_register_kvm(env, SPR_POWER_PMC6, "PMC6",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_PMC, &spr_write_PMC,
                     KVM_REG_PPC_PMC6, 0x00000000);
    spr_register_kvm(env, SPR_POWER_SIAR, "SIAR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_SIAR, 0x00000000);
    spr_register_kvm(env, SPR_POWER_SDAR, "SDAR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_SDAR, 0x00000000);
}

static void register_book3s_pmu_user_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_POWER_UMMCR0, "UMMCR0",
                 &spr_read_MMCR0_ureg, &spr_write_MMCR0_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x80000000);
    spr_register(env, SPR_POWER_UMMCR1, "UMMCR1",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UMMCRA, "UMMCRA",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC1, "UPMC1",
                 &spr_read_PMC14_ureg, &spr_write_PMC14_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC2, "UPMC2",
                 &spr_read_PMC14_ureg, &spr_write_PMC14_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC3, "UPMC3",
                 &spr_read_PMC14_ureg, &spr_write_PMC14_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC4, "UPMC4",
                 &spr_read_PMC14_ureg, &spr_write_PMC14_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC5, "UPMC5",
                 &spr_read_PMC56_ureg, &spr_write_PMC56_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_UPMC6, "UPMC6",
                 &spr_read_PMC56_ureg, &spr_write_PMC56_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_USIAR, "USIAR",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_USDAR, "USDAR",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
}

static void register_970_pmu_sup_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_970_PMC7, "PMC7",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_PMC7, 0x00000000);
    spr_register_kvm(env, SPR_970_PMC8, "PMC8",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_PMC8, 0x00000000);
}

static void register_970_pmu_user_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_970_UPMC7, "UPMC7",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_970_UPMC8, "UPMC8",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
}

static void register_power8_pmu_sup_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_POWER_MMCR2, "MMCR2",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_MMCR2, 0x00000000);
    spr_register_kvm(env, SPR_POWER_MMCRS, "MMCRS",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_MMCRS, 0x00000000);
    spr_register_kvm(env, SPR_POWER_SIER, "SIER",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_SIER, 0x00000000);
    spr_register_kvm(env, SPR_POWER_SPMC1, "SPMC1",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_SPMC1, 0x00000000);
    spr_register_kvm(env, SPR_POWER_SPMC2, "SPMC2",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_SPMC2, 0x00000000);
    spr_register_kvm(env, SPR_TACR, "TACR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_TACR, 0x00000000);
    spr_register_kvm(env, SPR_TCSCR, "TCSCR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_TCSCR, 0x00000000);
    spr_register_kvm(env, SPR_CSIGR, "CSIGR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_CSIGR, 0x00000000);
}

static void register_power8_pmu_user_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_POWER_UMMCR2, "UMMCR2",
                 &spr_read_MMCR2_ureg, &spr_write_MMCR2_ureg,
                 &spr_read_ureg, &spr_write_ureg,
                 0x00000000);
    spr_register(env, SPR_POWER_USIER, "USIER",
                 &spr_read_generic, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_power5p_ear_sprs(CPUPPCState *env)
{
    /* External access control */
    spr_register(env, SPR_EAR, "EAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_power5p_tb_sprs(CPUPPCState *env)
{
    /* TBU40 (High 40 bits of the Timebase register */
    spr_register_hv(env, SPR_TBU40, "TBU40",
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, &spr_write_tbu40,
                    0x00000000);
}

static void register_970_lpar_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    /*
     * PPC970: HID4 covers things later controlled by the LPCR and
     * RMOR in later CPUs, but with a different encoding.  We only
     * support the 970 in "Apple mode" which has all hypervisor
     * facilities disabled by strapping, so we can basically just
     * ignore it
     */
    spr_register(env, SPR_970_HID4, "HID4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
#endif
}

static void register_power5p_lpar_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    /* Logical partitionning */
    spr_register_kvm_hv(env, SPR_LPCR, "LPCR",
                        SPR_NOACCESS, SPR_NOACCESS,
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_lpcr,
                        KVM_REG_PPC_LPCR, LPCR_LPES0 | LPCR_LPES1);
    spr_register_hv(env, SPR_HDEC, "HDEC",
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, SPR_NOACCESS,
                    &spr_read_hdecr, &spr_write_hdecr, 0);
#endif
}

static void register_book3s_ids_sprs(CPUPPCState *env)
{
    /* FIXME: Will need to deal with thread vs core only SPRs */

    /* Processor identification */
    spr_register_hv(env, SPR_PIR, "PIR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
                 &spr_read_generic, NULL,
                 0x00000000);
    spr_register_hv(env, SPR_HID0, "HID0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_TSCR, "TSCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HMER, "HMER",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_hmer,
                 0x00000000);
    spr_register_hv(env, SPR_HMEER, "HMEER",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_TFMR, "TFMR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_LPIDR, "LPIDR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_lpidr,
                 0x00000000);
    spr_register_hv(env, SPR_HFSCR, "HFSCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_MMCRC, "MMCRC",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_MMCRH, "MMCRH",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HSPRG0, "HSPRG0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HSPRG1, "HSPRG1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HSRR0, "HSRR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HSRR1, "HSRR1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HDAR, "HDAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HDSISR, "HDSISR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register_hv(env, SPR_HRMOR, "HRMOR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_rmor_sprs(CPUPPCState *env)
{
    spr_register_hv(env, SPR_RMOR, "RMOR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_power8_ids_sprs(CPUPPCState *env)
{
    /* Thread identification */
    spr_register(env, SPR_TIR, "TIR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
                 0x00000000);
}

static void register_book3s_purr_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    /* PURR & SPURR: Hack - treat these as aliases for the TB for now */
    spr_register_kvm_hv(env, SPR_PURR,   "PURR",
                        &spr_read_purr, SPR_NOACCESS,
                        &spr_read_purr, SPR_NOACCESS,
                        &spr_read_purr, &spr_write_purr,
                        KVM_REG_PPC_PURR, 0x00000000);
    spr_register_kvm_hv(env, SPR_SPURR,   "SPURR",
                        &spr_read_purr, SPR_NOACCESS,
                        &spr_read_purr, SPR_NOACCESS,
                        &spr_read_purr, &spr_write_purr,
                        KVM_REG_PPC_SPURR, 0x00000000);
#endif
}

static void register_power6_dbg_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    spr_register(env, SPR_CFAR, "SPR_CFAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_cfar, &spr_write_cfar,
                 0x00000000);
#endif
}

static void register_power5p_common_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_PPR, "PPR",
                     &spr_read_generic, &spr_write_generic,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_PPR, 0x00000000);
}

static void register_power6_common_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    spr_register_kvm(env, SPR_DSCR, "SPR_DSCR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DSCR, 0x00000000);
#endif
    /*
     * Register PCR to report POWERPC_EXCP_PRIV_REG instead of
     * POWERPC_EXCP_INVAL_SPR in userspace. Permit hypervisor access.
     */
    spr_register_hv(env, SPR_PCR, "PCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_pcr,
                 0x00000000);
}

static void register_power8_tce_address_control_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_TAR, "TAR",
                     &spr_read_tar, &spr_write_tar,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_TAR, 0x00000000);
}

static void register_power8_tm_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_TFHAR, "TFHAR",
                     &spr_read_tm, &spr_write_tm,
                     &spr_read_tm, &spr_write_tm,
                     KVM_REG_PPC_TFHAR, 0x00000000);
    spr_register_kvm(env, SPR_TFIAR, "TFIAR",
                     &spr_read_tm, &spr_write_tm,
                     &spr_read_tm, &spr_write_tm,
                     KVM_REG_PPC_TFIAR, 0x00000000);
    spr_register_kvm(env, SPR_TEXASR, "TEXASR",
                     &spr_read_tm, &spr_write_tm,
                     &spr_read_tm, &spr_write_tm,
                     KVM_REG_PPC_TEXASR, 0x00000000);
    spr_register(env, SPR_TEXASRU, "TEXASRU",
                 &spr_read_tm_upper32, &spr_write_tm_upper32,
                 &spr_read_tm_upper32, &spr_write_tm_upper32,
                 0x00000000);
}

static void register_power8_ebb_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_BESCRS, "BESCRS",
                 &spr_read_ebb, &spr_write_ebb,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_BESCRSU, "BESCRSU",
                 &spr_read_ebb_upper32, &spr_write_ebb_upper32,
                 &spr_read_prev_upper32, &spr_write_prev_upper32,
                 0x00000000);
    spr_register(env, SPR_BESCRR, "BESCRR",
                 &spr_read_ebb, &spr_write_ebb,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_BESCRRU, "BESCRRU",
                 &spr_read_ebb_upper32, &spr_write_ebb_upper32,
                 &spr_read_prev_upper32, &spr_write_prev_upper32,
                 0x00000000);
    spr_register_kvm(env, SPR_EBBHR, "EBBHR",
                     &spr_read_ebb, &spr_write_ebb,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_EBBHR, 0x00000000);
    spr_register_kvm(env, SPR_EBBRR, "EBBRR",
                     &spr_read_ebb, &spr_write_ebb,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_EBBRR, 0x00000000);
    spr_register_kvm(env, SPR_BESCR, "BESCR",
                     &spr_read_ebb, &spr_write_ebb,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_BESCR, 0x00000000);
}

/* Virtual Time Base */
static void register_vtb_sprs(CPUPPCState *env)
{
    spr_register_kvm_hv(env, SPR_VTB, "VTB",
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_vtb, SPR_NOACCESS,
                        &spr_read_vtb, &spr_write_vtb,
                        KVM_REG_PPC_VTB, 0x00000000);
}

static void register_power8_fscr_sprs(CPUPPCState *env)
{
#if defined(CONFIG_USER_ONLY)
    target_ulong initval = 1ULL << FSCR_TAR;
#else
    target_ulong initval = 0;
#endif
    spr_register_kvm(env, SPR_FSCR, "FSCR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_FSCR, initval);
}

static void register_power8_pspb_sprs(CPUPPCState *env)
{
    spr_register_kvm(env, SPR_PSPB, "PSPB",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic32,
                     KVM_REG_PPC_PSPB, 0);
}

static void register_power8_dpdes_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    /* Directed Privileged Door-bell Exception State, used for IPI */
    spr_register_kvm_hv(env, SPR_DPDES, "DPDES",
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_dpdes, SPR_NOACCESS,
                        &spr_read_dpdes, &spr_write_dpdes,
                        KVM_REG_PPC_DPDES, 0x00000000);
#endif
}

static void register_power8_ic_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    spr_register_hv(env, SPR_IC, "IC",
                    SPR_NOACCESS, SPR_NOACCESS,
                    &spr_read_generic, SPR_NOACCESS,
                    &spr_read_generic, &spr_write_generic,
                    0);
#endif
}

static void register_power8_book4_sprs(CPUPPCState *env)
{
    /* Add a number of P8 book4 registers */
#if !defined(CONFIG_USER_ONLY)
    spr_register_kvm(env, SPR_ACOP, "ACOP",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_ACOP, 0);
    spr_register_kvm(env, SPR_BOOKS_PID, "PID",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_pidr,
                     KVM_REG_PPC_PID, 0);
    spr_register_kvm(env, SPR_WORT, "WORT",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_WORT, 0);
#endif
}

static void register_power7_book4_sprs(CPUPPCState *env)
{
    /* Add a number of P7 book4 registers */
#if !defined(CONFIG_USER_ONLY)
    spr_register_kvm(env, SPR_ACOP, "ACOP",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_ACOP, 0);
    spr_register_kvm(env, SPR_BOOKS_PID, "PID",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_PID, 0);
#endif
}

static void register_power8_rpr_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    spr_register_hv(env, SPR_RPR, "RPR",
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, SPR_NOACCESS,
                    &spr_read_generic, &spr_write_generic,
                    0x00000103070F1F3F);
#endif
}

static void register_power9_mmu_sprs(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    /* Partition Table Control */
    spr_register_kvm_hv(env, SPR_PTCR, "PTCR",
                        SPR_NOACCESS, SPR_NOACCESS,
                        SPR_NOACCESS, SPR_NOACCESS,
                        &spr_read_generic, &spr_write_ptcr,
                        KVM_REG_PPC_PTCR, 0x00000000);
    /* Address Segment Descriptor Register */
    spr_register_hv(env, SPR_ASDR, "ASDR",
                    SPR_NOACCESS, SPR_NOACCESS,
                    SPR_NOACCESS, SPR_NOACCESS,
                    &spr_read_generic, &spr_write_generic,
                    0x0000000000000000);
#endif
}

/*
 * Initialize PMU counter overflow timers for Power8 and
 * newer Power chips when using TCG.
 */
static void init_tcg_pmu_power8(CPUPPCState *env)
{
    /* Init PMU overflow timers */
    if (tcg_enabled()) {
        cpu_ppc_pmu_init(env);
    }
}

static void init_proc_book3s_common(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_book3s_altivec_sprs(env);
    register_book3s_pmu_sup_sprs(env);
    register_book3s_pmu_user_sprs(env);
    register_book3s_ctrl_sprs(env);
    /*
     * Can't find information on what this should be on reset.  This
     * value is the one used by 74xx processors.
     */
    vscr_init(env, 0x00010000);

    spr_register(env, SPR_USPRG3, "USPRG3",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);
}

static void init_proc_970(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_sdr1_sprs(env);
    register_book3s_dbg_sprs(env);

    /* 970 Specific Registers */
    register_970_hid_sprs(env);
    register_970_hior_sprs(env);
    register_low_BATs(env);
    register_970_pmu_sup_sprs(env);
    register_970_pmu_user_sprs(env);
    register_970_lpar_sprs(env);
    register_970_dbg_sprs(env);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_970(env);
    ppc970_irq_init(env_archcpu(env));
}

static void init_proc_power5plus(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_sdr1_sprs(env);
    register_book3s_dbg_sprs(env);

    /* POWER5+ Specific Registers */
    register_970_hid_sprs(env);
    register_970_hior_sprs(env);
    register_low_BATs(env);
    register_970_pmu_sup_sprs(env);
    register_970_pmu_user_sprs(env);
    register_power5p_common_sprs(env);
    register_power5p_lpar_sprs(env);
    register_power5p_ear_sprs(env);
    register_power5p_tb_sprs(env);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_970(env);
    ppc970_irq_init(env_archcpu(env));
}

static void init_proc_POWER7(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_sdr1_sprs(env);
    register_book3s_dbg_sprs(env);

    /* POWER7 Specific Registers */
    register_book3s_ids_sprs(env);
    register_rmor_sprs(env);
    register_amr_sprs(env);
    register_book3s_purr_sprs(env);
    register_power5p_common_sprs(env);
    register_power5p_lpar_sprs(env);
    register_power5p_ear_sprs(env);
    register_power5p_tb_sprs(env);
    register_power6_common_sprs(env);
    register_power6_dbg_sprs(env);
    register_power7_book4_sprs(env);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_POWER7(env);
    ppcPOWER7_irq_init(env_archcpu(env));
}

static void init_proc_POWER8(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_sdr1_sprs(env);
    register_book3s_207_dbg_sprs(env);

    /* Common TCG PMU */
    init_tcg_pmu_power8(env);

    /* POWER8 Specific Registers */
    register_book3s_ids_sprs(env);
    register_rmor_sprs(env);
    register_amr_sprs(env);
    register_iamr_sprs(env);
    register_book3s_purr_sprs(env);
    register_power5p_common_sprs(env);
    register_power5p_lpar_sprs(env);
    register_power5p_ear_sprs(env);
    register_power5p_tb_sprs(env);
    register_power6_common_sprs(env);
    register_power6_dbg_sprs(env);
    register_power8_tce_address_control_sprs(env);
    register_power8_ids_sprs(env);
    register_power8_ebb_sprs(env);
    register_power8_fscr_sprs(env);
    register_power8_pmu_sup_sprs(env);
    register_power8_pmu_user_sprs(env);
    register_power8_tm_sprs(env);
    register_power8_pspb_sprs(env);
    register_power8_dpdes_sprs(env);
    register_vtb_sprs(env);
    register_power8_ic_sprs(env);
    register_power8_book4_sprs(env);
    register_power8_rpr_sprs(env);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_POWER8(env);
    ppcPOWER7_irq_init(env_archcpu(env));
}

static void init_proc_POWER9(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_book3s_207_dbg_sprs(env);

    /* Common TCG PMU */
    init_tcg_pmu_power8(env);

    /* POWER8 Specific Registers */
    register_book3s_ids_sprs(env);
    register_amr_sprs(env);
    register_iamr_sprs(env);
    register_book3s_purr_sprs(env);
    register_power5p_common_sprs(env);
    register_power5p_lpar_sprs(env);
    register_power5p_ear_sprs(env);
    register_power5p_tb_sprs(env);
    register_power6_common_sprs(env);
    register_power6_dbg_sprs(env);
    register_power8_tce_address_control_sprs(env);
    register_power8_ids_sprs(env);
    register_power8_ebb_sprs(env);
    register_power8_fscr_sprs(env);
    register_power8_pmu_sup_sprs(env);
    register_power8_pmu_user_sprs(env);
    register_power8_tm_sprs(env);
    register_power8_pspb_sprs(env);
    register_power8_dpdes_sprs(env);
    register_vtb_sprs(env);
    register_power8_ic_sprs(env);
    register_power8_book4_sprs(env);
    register_power8_rpr_sprs(env);
    register_power9_mmu_sprs(env);

    /* POWER9 Specific registers */
    spr_register_kvm(env, SPR_TIDR, "TIDR", NULL, NULL,
                     spr_read_generic, spr_write_generic,
                     KVM_REG_PPC_TIDR, 0);

    /* FIXME: Filter fields properly based on privilege level */
    spr_register_kvm_hv(env, SPR_PSSCR, "PSSCR", NULL, NULL, NULL, NULL,
                        spr_read_generic, spr_write_generic,
                        KVM_REG_PPC_PSSCR, 0);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_POWER9(env);
    ppcPOWER9_irq_init(env_archcpu(env));
}

static void init_proc_POWER10(CPUPPCState *env)
{
    /* Common Registers */
    init_proc_book3s_common(env);
    register_book3s_207_dbg_sprs(env);

    /* Common TCG PMU */
    init_tcg_pmu_power8(env);

    /* POWER8 Specific Registers */
    register_book3s_ids_sprs(env);
    register_amr_sprs(env);
    register_iamr_sprs(env);
    register_book3s_purr_sprs(env);
    register_power5p_common_sprs(env);
    register_power5p_lpar_sprs(env);
    register_power5p_ear_sprs(env);
    register_power6_common_sprs(env);
    register_power6_dbg_sprs(env);
    register_power8_tce_address_control_sprs(env);
    register_power8_ids_sprs(env);
    register_power8_ebb_sprs(env);
    register_power8_fscr_sprs(env);
    register_power8_pmu_sup_sprs(env);
    register_power8_pmu_user_sprs(env);
    register_power8_tm_sprs(env);
    register_power8_pspb_sprs(env);
    register_vtb_sprs(env);
    register_power8_ic_sprs(env);
    register_power8_book4_sprs(env);
    register_power8_rpr_sprs(env);
    register_power9_mmu_sprs(env);

    /* FIXME: Filter fields properly based on privilege level */
    spr_register_kvm_hv(env, SPR_PSSCR, "PSSCR", NULL, NULL, NULL, NULL,
                        spr_read_generic, spr_write_generic,
                        KVM_REG_PPC_PSSCR, 0);

    /* env variables */
    env->dcache_line_size = 128;
    env->icache_line_size = 128;

    /* Allocate hardware IRQ controller */
    init_excp_POWER10(env);
    ppcPOWER9_irq_init(env_archcpu(env));
}

POWERPC_FAMILY(970)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 970";
    pcc->init_proc = init_proc_970;
    pcc->check_pow = check_pow_970;
    pcc->dispatch_excp = powerpc_excp_books;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_64B | PPC_ALTIVEC |
                       PPC_SEGMENT_64B | PPC_SLBI;
    pcc->insns_flags2 = PPC2_FP_CVT_S64;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_POW) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI);
    pcc->mmu_model = POWERPC_MMU_64B;
#if defined(CONFIG_SOFTMMU)
    pcc->hash64_opts = &ppc_hash64_opts_basic;
#endif
    pcc->excp_model = POWERPC_EXCP_970;
    pcc->bus_model = PPC_FLAGS_INPUT_970;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x10000;
}

POWERPC_FAMILY(POWER5P)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->fw_name = "PowerPC,POWER5";
    dc->desc = "POWER5+";
    pcc->init_proc = init_proc_power5plus;
    pcc->check_pow = check_pow_970;
    pcc->dispatch_excp = powerpc_excp_books;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_STFIWX |
                       PPC_FLOAT_EXT |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_64B |
                       PPC_POPCNTB |
                       PPC_SEGMENT_64B | PPC_SLBI;
    pcc->insns_flags2 = PPC2_FP_CVT_S64;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_POW) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI);
    pcc->lpcr_mask = LPCR_RMLS | LPCR_ILE | LPCR_LPES0 | LPCR_LPES1 |
        LPCR_RMI | LPCR_HDICE;
    pcc->mmu_model = POWERPC_MMU_2_03;
#if defined(CONFIG_SOFTMMU)
    pcc->hash64_opts = &ppc_hash64_opts_basic;
    pcc->lrg_decr_bits = 32;
#endif
    pcc->excp_model = POWERPC_EXCP_970;
    pcc->bus_model = PPC_FLAGS_INPUT_970;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x10000;
}

POWERPC_FAMILY(POWER7)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    dc->fw_name = "PowerPC,POWER7";
    dc->desc = "POWER7";
    pcc->pvr_match = ppc_pvr_match_power7;
    pcc->pcr_mask = PCR_VEC_DIS | PCR_VSX_DIS | PCR_COMPAT_2_05;
    pcc->pcr_supported = PCR_COMPAT_2_06 | PCR_COMPAT_2_05;
    pcc->init_proc = init_proc_POWER7;
    pcc->check_pow = check_pow_nocheck;
    pcc->dispatch_excp = powerpc_excp_books;
    cc->has_work = cpu_has_work_POWER7;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_ISEL | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_FRSQRTES |
                       PPC_FLOAT_STFIWX |
                       PPC_FLOAT_EXT |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_64B | PPC_64H | PPC_64BX | PPC_ALTIVEC |
                       PPC_SEGMENT_64B | PPC_SLBI |
                       PPC_POPCNTB | PPC_POPCNTWD |
                       PPC_CILDST;
    pcc->insns_flags2 = PPC2_VSX | PPC2_DFP | PPC2_DBRX | PPC2_ISA205 |
                        PPC2_PERM_ISA206 | PPC2_DIVE_ISA206 |
                        PPC2_ATOMIC_ISA206 | PPC2_FP_CVT_ISA206 |
                        PPC2_FP_TST_ISA206 | PPC2_FP_CVT_S64 |
                        PPC2_PM_ISA206;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_VSX) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->lpcr_mask = LPCR_VPM0 | LPCR_VPM1 | LPCR_ISL | LPCR_DPFD |
        LPCR_VRMASD | LPCR_RMLS | LPCR_ILE |
        LPCR_P7_PECE0 | LPCR_P7_PECE1 | LPCR_P7_PECE2 |
        LPCR_MER | LPCR_TC |
        LPCR_LPES0 | LPCR_LPES1 | LPCR_HDICE;
    pcc->lpcr_pm = LPCR_P7_PECE0 | LPCR_P7_PECE1 | LPCR_P7_PECE2;
    pcc->mmu_model = POWERPC_MMU_2_06;
#if defined(CONFIG_SOFTMMU)
    pcc->hash64_opts = &ppc_hash64_opts_POWER7;
    pcc->lrg_decr_bits = 32;
#endif
    pcc->excp_model = POWERPC_EXCP_POWER7;
    pcc->bus_model = PPC_FLAGS_INPUT_POWER7;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK | POWERPC_FLAG_CFAR |
                 POWERPC_FLAG_VSX;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x8000;
}

POWERPC_FAMILY(POWER8)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    dc->fw_name = "PowerPC,POWER8";
    dc->desc = "POWER8";
    pcc->pvr_match = ppc_pvr_match_power8;
    pcc->pcr_mask = PCR_TM_DIS | PCR_COMPAT_2_06 | PCR_COMPAT_2_05;
    pcc->pcr_supported = PCR_COMPAT_2_07 | PCR_COMPAT_2_06 | PCR_COMPAT_2_05;
    pcc->init_proc = init_proc_POWER8;
    pcc->check_pow = check_pow_nocheck;
    pcc->dispatch_excp = powerpc_excp_books;
    cc->has_work = cpu_has_work_POWER8;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_ISEL | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_FRSQRTES |
                       PPC_FLOAT_STFIWX |
                       PPC_FLOAT_EXT |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_64B | PPC_64H | PPC_64BX | PPC_ALTIVEC |
                       PPC_SEGMENT_64B | PPC_SLBI |
                       PPC_POPCNTB | PPC_POPCNTWD |
                       PPC_CILDST;
    pcc->insns_flags2 = PPC2_VSX | PPC2_VSX207 | PPC2_DFP | PPC2_DBRX |
                        PPC2_PERM_ISA206 | PPC2_DIVE_ISA206 |
                        PPC2_ATOMIC_ISA206 | PPC2_FP_CVT_ISA206 |
                        PPC2_FP_TST_ISA206 | PPC2_BCTAR_ISA207 |
                        PPC2_LSQ_ISA207 | PPC2_ALTIVEC_207 |
                        PPC2_ISA205 | PPC2_ISA207S | PPC2_FP_CVT_S64 |
                        PPC2_TM | PPC2_PM_ISA206;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_HV) |
                    (1ull << MSR_TM) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_VSX) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_TS0) |
                    (1ull << MSR_TS1) |
                    (1ull << MSR_LE);
    pcc->lpcr_mask = LPCR_VPM0 | LPCR_VPM1 | LPCR_ISL | LPCR_KBV |
        LPCR_DPFD | LPCR_VRMASD | LPCR_RMLS | LPCR_ILE |
        LPCR_AIL | LPCR_ONL | LPCR_P8_PECE0 | LPCR_P8_PECE1 |
        LPCR_P8_PECE2 | LPCR_P8_PECE3 | LPCR_P8_PECE4 |
        LPCR_MER | LPCR_TC | LPCR_LPES0 | LPCR_HDICE;
    pcc->lpcr_pm = LPCR_P8_PECE0 | LPCR_P8_PECE1 | LPCR_P8_PECE2 |
                   LPCR_P8_PECE3 | LPCR_P8_PECE4;
    pcc->mmu_model = POWERPC_MMU_2_07;
#if defined(CONFIG_SOFTMMU)
    pcc->hash64_opts = &ppc_hash64_opts_POWER7;
    pcc->lrg_decr_bits = 32;
    pcc->n_host_threads = 8;
#endif
    pcc->excp_model = POWERPC_EXCP_POWER8;
    pcc->bus_model = PPC_FLAGS_INPUT_POWER7;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK | POWERPC_FLAG_CFAR |
                 POWERPC_FLAG_VSX | POWERPC_FLAG_TM;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x8000;
}

POWERPC_FAMILY(POWER9)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    dc->fw_name = "PowerPC,POWER9";
    dc->desc = "POWER9";
    pcc->pvr_match = ppc_pvr_match_power9;
    pcc->pcr_mask = PCR_COMPAT_2_05 | PCR_COMPAT_2_06 | PCR_COMPAT_2_07;
    pcc->pcr_supported = PCR_COMPAT_3_00 | PCR_COMPAT_2_07 | PCR_COMPAT_2_06 |
                         PCR_COMPAT_2_05;
    pcc->init_proc = init_proc_POWER9;
    pcc->check_pow = check_pow_nocheck;
    pcc->dispatch_excp = powerpc_excp_books;
    cc->has_work = cpu_has_work_POWER9;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_ISEL | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_FRSQRTES |
                       PPC_FLOAT_STFIWX |
                       PPC_FLOAT_EXT |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBSYNC |
                       PPC_64B | PPC_64H | PPC_64BX | PPC_ALTIVEC |
                       PPC_SEGMENT_64B | PPC_SLBI |
                       PPC_POPCNTB | PPC_POPCNTWD |
                       PPC_CILDST;
    pcc->insns_flags2 = PPC2_VSX | PPC2_VSX207 | PPC2_DFP | PPC2_DBRX |
                        PPC2_PERM_ISA206 | PPC2_DIVE_ISA206 |
                        PPC2_ATOMIC_ISA206 | PPC2_FP_CVT_ISA206 |
                        PPC2_FP_TST_ISA206 | PPC2_BCTAR_ISA207 |
                        PPC2_LSQ_ISA207 | PPC2_ALTIVEC_207 |
                        PPC2_ISA205 | PPC2_ISA207S | PPC2_FP_CVT_S64 |
                        PPC2_TM | PPC2_ISA300 | PPC2_PRCNTL;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_HV) |
                    (1ull << MSR_TM) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_VSX) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->lpcr_mask = LPCR_VPM1 | LPCR_ISL | LPCR_KBV | LPCR_DPFD |
        (LPCR_PECE_U_MASK & LPCR_HVEE) | LPCR_ILE | LPCR_AIL |
        LPCR_UPRT | LPCR_EVIRT | LPCR_ONL | LPCR_HR | LPCR_LD |
        (LPCR_PECE_L_MASK & (LPCR_PDEE | LPCR_HDEE | LPCR_EEE |
                             LPCR_DEE | LPCR_OEE))
        | LPCR_MER | LPCR_GTSE | LPCR_TC |
        LPCR_HEIC | LPCR_LPES0 | LPCR_HVICE | LPCR_HDICE;
    pcc->lpcr_pm = LPCR_PDEE | LPCR_HDEE | LPCR_EEE | LPCR_DEE | LPCR_OEE;
    pcc->mmu_model = POWERPC_MMU_3_00;
#if defined(CONFIG_SOFTMMU)
    /* segment page size remain the same */
    pcc->hash64_opts = &ppc_hash64_opts_POWER7;
    pcc->radix_page_info = &POWER9_radix_page_info;
    pcc->lrg_decr_bits = 56;
    pcc->n_host_threads = 4;
#endif
    pcc->excp_model = POWERPC_EXCP_POWER9;
    pcc->bus_model = PPC_FLAGS_INPUT_POWER9;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK | POWERPC_FLAG_CFAR |
                 POWERPC_FLAG_VSX | POWERPC_FLAG_TM | POWERPC_FLAG_SCV;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x8000;
}

POWERPC_FAMILY(POWER10)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    dc->fw_name = "PowerPC,POWER10";
    dc->desc = "POWER10";
    pcc->pvr_match = ppc_pvr_match_power10;
    pcc->pcr_mask = PCR_COMPAT_2_05 | PCR_COMPAT_2_06 | PCR_COMPAT_2_07 |
                    PCR_COMPAT_3_00;
    pcc->pcr_supported = PCR_COMPAT_3_10 | PCR_COMPAT_3_00 | PCR_COMPAT_2_07 |
                         PCR_COMPAT_2_06 | PCR_COMPAT_2_05;
    pcc->init_proc = init_proc_POWER10;
    pcc->check_pow = check_pow_nocheck;
    pcc->dispatch_excp = powerpc_excp_books;
    cc->has_work = cpu_has_work_POWER10;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_ISEL | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FSQRT | PPC_FLOAT_FRSQRTE |
                       PPC_FLOAT_FRSQRTES |
                       PPC_FLOAT_STFIWX |
                       PPC_FLOAT_EXT |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBSYNC |
                       PPC_64B | PPC_64H | PPC_64BX | PPC_ALTIVEC |
                       PPC_SEGMENT_64B | PPC_SLBI |
                       PPC_POPCNTB | PPC_POPCNTWD |
                       PPC_CILDST;
    pcc->insns_flags2 = PPC2_VSX | PPC2_VSX207 | PPC2_DFP | PPC2_DBRX |
                        PPC2_PERM_ISA206 | PPC2_DIVE_ISA206 |
                        PPC2_ATOMIC_ISA206 | PPC2_FP_CVT_ISA206 |
                        PPC2_FP_TST_ISA206 | PPC2_BCTAR_ISA207 |
                        PPC2_LSQ_ISA207 | PPC2_ALTIVEC_207 |
                        PPC2_ISA205 | PPC2_ISA207S | PPC2_FP_CVT_S64 |
                        PPC2_TM | PPC2_ISA300 | PPC2_PRCNTL | PPC2_ISA310;
    pcc->msr_mask = (1ull << MSR_SF) |
                    (1ull << MSR_HV) |
                    (1ull << MSR_TM) |
                    (1ull << MSR_VR) |
                    (1ull << MSR_VSX) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->lpcr_mask = LPCR_VPM1 | LPCR_ISL | LPCR_KBV | LPCR_DPFD |
        (LPCR_PECE_U_MASK & LPCR_HVEE) | LPCR_ILE | LPCR_AIL |
        LPCR_UPRT | LPCR_EVIRT | LPCR_ONL | LPCR_HR | LPCR_LD |
        (LPCR_PECE_L_MASK & (LPCR_PDEE | LPCR_HDEE | LPCR_EEE |
                             LPCR_DEE | LPCR_OEE))
        | LPCR_MER | LPCR_GTSE | LPCR_TC |
        LPCR_HEIC | LPCR_LPES0 | LPCR_HVICE | LPCR_HDICE;
    /* DD2 adds an extra HAIL bit */
    pcc->lpcr_mask |= LPCR_HAIL;

    pcc->lpcr_pm = LPCR_PDEE | LPCR_HDEE | LPCR_EEE | LPCR_DEE | LPCR_OEE;
    pcc->mmu_model = POWERPC_MMU_3_00;
#if defined(CONFIG_SOFTMMU)
    /* segment page size remain the same */
    pcc->hash64_opts = &ppc_hash64_opts_POWER7;
    pcc->radix_page_info = &POWER10_radix_page_info;
    pcc->lrg_decr_bits = 56;
#endif
    pcc->excp_model = POWERPC_EXCP_POWER10;
    pcc->bus_model = PPC_FLAGS_INPUT_POWER9;
    pcc->bfd_mach = bfd_mach_ppc64;
    pcc->flags = POWERPC_FLAG_VRE | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_PMM |
                 POWERPC_FLAG_BUS_CLK | POWERPC_FLAG_CFAR |
                 POWERPC_FLAG_VSX | POWERPC_FLAG_TM | POWERPC_FLAG_SCV;
    pcc->l1_dcache_size = 0x8000;
    pcc->l1_icache_size = 0x8000;
}
