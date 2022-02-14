/*
 *  PowerPC exception emulation helpers for QEMU.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "internal.h"
#include "helper_regs.h"

#include "trace.h"

#ifdef CONFIG_TCG
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#endif

/*****************************************************************************/
/* Exception processing */
#if !defined(CONFIG_USER_ONLY)

const char *powerpc_excp_name(int excp)
{
    switch (excp) {
    case POWERPC_EXCP_CRITICAL: return "CRITICAL";
    case POWERPC_EXCP_MCHECK:   return "MCHECK";
    case POWERPC_EXCP_DSI:      return "DSI";
    case POWERPC_EXCP_ISI:      return "ISI";
    case POWERPC_EXCP_EXTERNAL: return "EXTERNAL";
    case POWERPC_EXCP_ALIGN:    return "ALIGN";
    case POWERPC_EXCP_PROGRAM:  return "PROGRAM";
    case POWERPC_EXCP_FPU:      return "FPU";
    case POWERPC_EXCP_SYSCALL:  return "SYSCALL";
    case POWERPC_EXCP_APU:      return "APU";
    case POWERPC_EXCP_DECR:     return "DECR";
    case POWERPC_EXCP_FIT:      return "FIT";
    case POWERPC_EXCP_WDT:      return "WDT";
    case POWERPC_EXCP_DTLB:     return "DTLB";
    case POWERPC_EXCP_ITLB:     return "ITLB";
    case POWERPC_EXCP_DEBUG:    return "DEBUG";
    case POWERPC_EXCP_SPEU:     return "SPEU";
    case POWERPC_EXCP_EFPDI:    return "EFPDI";
    case POWERPC_EXCP_EFPRI:    return "EFPRI";
    case POWERPC_EXCP_EPERFM:   return "EPERFM";
    case POWERPC_EXCP_DOORI:    return "DOORI";
    case POWERPC_EXCP_DOORCI:   return "DOORCI";
    case POWERPC_EXCP_GDOORI:   return "GDOORI";
    case POWERPC_EXCP_GDOORCI:  return "GDOORCI";
    case POWERPC_EXCP_HYPPRIV:  return "HYPPRIV";
    case POWERPC_EXCP_RESET:    return "RESET";
    case POWERPC_EXCP_DSEG:     return "DSEG";
    case POWERPC_EXCP_ISEG:     return "ISEG";
    case POWERPC_EXCP_HDECR:    return "HDECR";
    case POWERPC_EXCP_TRACE:    return "TRACE";
    case POWERPC_EXCP_HDSI:     return "HDSI";
    case POWERPC_EXCP_HISI:     return "HISI";
    case POWERPC_EXCP_HDSEG:    return "HDSEG";
    case POWERPC_EXCP_HISEG:    return "HISEG";
    case POWERPC_EXCP_VPU:      return "VPU";
    case POWERPC_EXCP_PIT:      return "PIT";
    case POWERPC_EXCP_EMUL:     return "EMUL";
    case POWERPC_EXCP_IFTLB:    return "IFTLB";
    case POWERPC_EXCP_DLTLB:    return "DLTLB";
    case POWERPC_EXCP_DSTLB:    return "DSTLB";
    case POWERPC_EXCP_FPA:      return "FPA";
    case POWERPC_EXCP_DABR:     return "DABR";
    case POWERPC_EXCP_IABR:     return "IABR";
    case POWERPC_EXCP_SMI:      return "SMI";
    case POWERPC_EXCP_PERFM:    return "PERFM";
    case POWERPC_EXCP_THERM:    return "THERM";
    case POWERPC_EXCP_VPUA:     return "VPUA";
    case POWERPC_EXCP_SOFTP:    return "SOFTP";
    case POWERPC_EXCP_MAINT:    return "MAINT";
    case POWERPC_EXCP_MEXTBR:   return "MEXTBR";
    case POWERPC_EXCP_NMEXTBR:  return "NMEXTBR";
    case POWERPC_EXCP_ITLBE:    return "ITLBE";
    case POWERPC_EXCP_DTLBE:    return "DTLBE";
    case POWERPC_EXCP_VSXU:     return "VSXU";
    case POWERPC_EXCP_FU:       return "FU";
    case POWERPC_EXCP_HV_EMU:   return "HV_EMU";
    case POWERPC_EXCP_HV_MAINT: return "HV_MAINT";
    case POWERPC_EXCP_HV_FU:    return "HV_FU";
    case POWERPC_EXCP_SDOOR:    return "SDOOR";
    case POWERPC_EXCP_SDOOR_HV: return "SDOOR_HV";
    case POWERPC_EXCP_HVIRT:    return "HVIRT";
    case POWERPC_EXCP_SYSCALL_VECTORED: return "SYSCALL_VECTORED";
    default:
        g_assert_not_reached();
    }
}

void ppc_excp_debug_sw_tlb(CPUPPCState *env, int excp)
{
    const char *es;
    target_ulong *miss, *cmp;
    int en;

    if (!qemu_loglevel_mask(CPU_LOG_MMU)) {
        return;
    }

    if (excp == POWERPC_EXCP_IFTLB) {
        es = "I";
        en = 'I';
        miss = &env->spr[SPR_IMISS];
        cmp = &env->spr[SPR_ICMP];
    } else {
        if (excp == POWERPC_EXCP_DLTLB) {
            es = "DL";
        } else {
            es = "DS";
        }
        en = 'D';
        miss = &env->spr[SPR_DMISS];
        cmp = &env->spr[SPR_DCMP];
    }
    qemu_log("6xx %sTLB miss: %cM " TARGET_FMT_lx " %cC "
             TARGET_FMT_lx " H1 " TARGET_FMT_lx " H2 "
             TARGET_FMT_lx " %08x\n", es, en, *miss, en, *cmp,
             env->spr[SPR_HASH1], env->spr[SPR_HASH2],
             env->error_code);
}

void powerpc_reset_excp_state(PowerPCCPU *cpu)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    /* Reset exception state */
    cs->exception_index = POWERPC_EXCP_NONE;
    env->error_code = 0;
}

void powerpc_set_excp_state(PowerPCCPU *cpu, target_ulong vector,
                            target_ulong msr)
{
    CPUPPCState *env = &cpu->env;

    assert((msr & env->msr_mask) == msr);

    /*
     * We don't use hreg_store_msr here as already have treated any
     * special case that could occur. Just store MSR and update hflags
     *
     * Note: We *MUST* not use hreg_store_msr() as-is anyway because it
     * will prevent setting of the HV bit which some exceptions might need
     * to do.
     */
    env->nip = vector;
    env->msr = msr;
    hreg_compute_hflags(env);

    powerpc_reset_excp_state(cpu);

    /*
     * Any interrupt is context synchronizing, check if TCG TLB needs
     * a delayed flush on ppc64
     */
    check_tlb_flush(env, false);

    /* Reset the reservation */
    env->reserve_addr = -1;
}

/*
 * When running a nested HV guest under vhyp, external interrupts are
 * delivered as HVIRT.
 */
static bool books_vhyp_promotes_external_to_hvirt(PowerPCCPU *cpu)
{
    if (cpu->vhyp) {
        return vhyp_cpu_in_nested(cpu);
    }
    return false;
}

static void powerpc_excp(PowerPCCPU *cpu, int excp)
{
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    if (excp <= POWERPC_EXCP_NONE || excp >= POWERPC_EXCP_NB) {
        cpu_abort(cs, "Invalid PowerPC exception %d. Aborting\n", excp);
    }

    if (qemu_loglevel_mask(CPU_LOG_INT)) {
        warn_report_once("use -trace ppc_excp* instead of -d int\n");
    }

    trace_ppc_excp(env->nip, powerpc_excp_name(excp), excp, env->error_code);

    (*pcc->dispatch_excp)(cpu, excp);
}

void ppc_cpu_do_interrupt(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);

    powerpc_excp(cpu, cs->exception_index);
}

static void ppc_hw_interrupt(CPUPPCState *env)
{
    PowerPCCPU *cpu = env_archcpu(env);
    bool async_deliver;

    /* External reset */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_RESET)) {
        env->pending_interrupts &= ~(1 << PPC_INTERRUPT_RESET);
        powerpc_excp(cpu, POWERPC_EXCP_RESET);
        return;
    }
    /* Machine check exception */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_MCK)) {
        env->pending_interrupts &= ~(1 << PPC_INTERRUPT_MCK);
        powerpc_excp(cpu, POWERPC_EXCP_MCHECK);
        return;
    }
#if 0 /* TODO */
    /* External debug exception */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_DEBUG)) {
        env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DEBUG);
        powerpc_excp(cpu, POWERPC_EXCP_DEBUG);
        return;
    }
#endif

    /*
     * For interrupts that gate on MSR:EE, we need to do something a
     * bit more subtle, as we need to let them through even when EE is
     * clear when coming out of some power management states (in order
     * for them to become a 0x100).
     */
    async_deliver = (msr_ee != 0) || env->resume_as_sreset;

    /* Hypervisor decrementer exception */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_HDECR)) {
        /* LPCR will be clear when not supported so this will work */
        bool hdice = !!(env->spr[SPR_LPCR] & LPCR_HDICE);
        if ((async_deliver || msr_hv == 0) && hdice) {
            /* HDEC clears on delivery */
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_HDECR);
            powerpc_excp(cpu, POWERPC_EXCP_HDECR);
            return;
        }
    }

    /* Hypervisor virtualization interrupt */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_HVIRT)) {
        /* LPCR will be clear when not supported so this will work */
        bool hvice = !!(env->spr[SPR_LPCR] & LPCR_HVICE);
        if ((async_deliver || msr_hv == 0) && hvice) {
            powerpc_excp(cpu, POWERPC_EXCP_HVIRT);
            return;
        }
    }

    /* External interrupt can ignore MSR:EE under some circumstances */
    if (env->pending_interrupts & (1 << PPC_INTERRUPT_EXT)) {
        bool lpes0 = !!(env->spr[SPR_LPCR] & LPCR_LPES0);
        bool heic = !!(env->spr[SPR_LPCR] & LPCR_HEIC);
        /* HEIC blocks delivery to the hypervisor */
        if ((async_deliver && !(heic && msr_hv && !msr_pr)) ||
            (env->has_hv_mode && msr_hv == 0 && !lpes0)) {
            if (books_vhyp_promotes_external_to_hvirt(cpu)) {
                powerpc_excp(cpu, POWERPC_EXCP_HVIRT);
            } else {
                powerpc_excp(cpu, POWERPC_EXCP_EXTERNAL);
            }
            return;
        }
    }
    if (msr_ce != 0) {
        /* External critical interrupt */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_CEXT)) {
            powerpc_excp(cpu, POWERPC_EXCP_CRITICAL);
            return;
        }
    }
    if (async_deliver != 0) {
        /* Watchdog timer on embedded PowerPC */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_WDT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_WDT);
            powerpc_excp(cpu, POWERPC_EXCP_WDT);
            return;
        }
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_CDOORBELL)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_CDOORBELL);
            powerpc_excp(cpu, POWERPC_EXCP_DOORCI);
            return;
        }
        /* Fixed interval timer on embedded PowerPC */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_FIT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_FIT);
            powerpc_excp(cpu, POWERPC_EXCP_FIT);
            return;
        }
        /* Programmable interval timer on embedded PowerPC */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_PIT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_PIT);
            powerpc_excp(cpu, POWERPC_EXCP_PIT);
            return;
        }
        /* Decrementer exception */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_DECR)) {
            if (ppc_decr_clear_on_delivery(env)) {
                env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DECR);
            }
            powerpc_excp(cpu, POWERPC_EXCP_DECR);
            return;
        }
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_DOORBELL)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DOORBELL);
            if (is_book3s_arch2x(env)) {
                powerpc_excp(cpu, POWERPC_EXCP_SDOOR);
            } else {
                powerpc_excp(cpu, POWERPC_EXCP_DOORI);
            }
            return;
        }
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_HDOORBELL)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_HDOORBELL);
            powerpc_excp(cpu, POWERPC_EXCP_SDOOR_HV);
            return;
        }
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_PERFM)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_PERFM);
            powerpc_excp(cpu, POWERPC_EXCP_PERFM);
            return;
        }
        /* Thermal interrupt */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_THERM)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_THERM);
            powerpc_excp(cpu, POWERPC_EXCP_THERM);
            return;
        }
        /* EBB exception */
        if (env->pending_interrupts & (1 << PPC_INTERRUPT_EBB)) {
            /*
             * EBB exception must be taken in problem state and
             * with BESCR_GE set.
             */
            if (msr_pr == 1 && env->spr[SPR_BESCR] & BESCR_GE) {
                env->pending_interrupts &= ~(1 << PPC_INTERRUPT_EBB);

                if (env->spr[SPR_BESCR] & BESCR_PMEO) {
                    powerpc_excp(cpu, POWERPC_EXCP_PERFM_EBB);
                } else if (env->spr[SPR_BESCR] & BESCR_EEO) {
                    powerpc_excp(cpu, POWERPC_EXCP_EXTERNAL_EBB);
                }

                return;
            }
        }
    }

    if (env->resume_as_sreset) {
        /*
         * This is a bug ! It means that has_work took us out of halt without
         * anything to deliver while in a PM state that requires getting
         * out via a 0x100
         *
         * This means we will incorrectly execute past the power management
         * instruction instead of triggering a reset.
         *
         * It generally means a discrepancy between the wakeup conditions in the
         * processor has_work implementation and the logic in this function.
         */
        cpu_abort(env_cpu(env),
                  "Wakeup from PM state but interrupt Undelivered");
    }
}

void ppc_cpu_do_system_reset(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);

    powerpc_excp(cpu, POWERPC_EXCP_RESET);
}

void ppc_cpu_do_fwnmi_machine_check(CPUState *cs, target_ulong vector)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;
    target_ulong msr = 0;

    /*
     * Set MSR and NIP for the handler, SRR0/1, DAR and DSISR have already
     * been set by KVM.
     */
    msr = (1ULL << MSR_ME);
    msr |= env->msr & (1ULL << MSR_SF);
    if (ppc_interrupts_little_endian(cpu, false)) {
        msr |= (1ULL << MSR_LE);
    }

    /* Anything for nested required here? MSR[HV] bit? */

    powerpc_set_excp_state(cpu, vector, msr);
}

bool ppc_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if (interrupt_request & CPU_INTERRUPT_HARD) {
        ppc_hw_interrupt(env);
        if (env->pending_interrupts == 0) {
            cs->interrupt_request &= ~CPU_INTERRUPT_HARD;
        }
        return true;
    }
    return false;
}

#endif /* !CONFIG_USER_ONLY */

/*****************************************************************************/
/* Exceptions processing helpers */

void raise_exception_err_ra(CPUPPCState *env, uint32_t exception,
                            uint32_t error_code, uintptr_t raddr)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = exception;
    env->error_code = error_code;
    cpu_loop_exit_restore(cs, raddr);
}

void raise_exception_err(CPUPPCState *env, uint32_t exception,
                         uint32_t error_code)
{
    raise_exception_err_ra(env, exception, error_code, 0);
}

void raise_exception(CPUPPCState *env, uint32_t exception)
{
    raise_exception_err_ra(env, exception, 0, 0);
}

void raise_exception_ra(CPUPPCState *env, uint32_t exception,
                        uintptr_t raddr)
{
    raise_exception_err_ra(env, exception, 0, raddr);
}

#ifdef CONFIG_TCG
void helper_raise_exception_err(CPUPPCState *env, uint32_t exception,
                                uint32_t error_code)
{
    raise_exception_err_ra(env, exception, error_code, 0);
}

void helper_raise_exception(CPUPPCState *env, uint32_t exception)
{
    raise_exception_err_ra(env, exception, 0, 0);
}
#endif

#if !defined(CONFIG_USER_ONLY)
#ifdef CONFIG_TCG
void helper_store_msr(CPUPPCState *env, target_ulong val)
{
    uint32_t excp = hreg_store_msr(env, val, 0);

    if (excp != 0) {
        CPUState *cs = env_cpu(env);
        cpu_interrupt_exittb(cs);
        raise_exception(env, excp);
    }
}

#if defined(TARGET_PPC64)
void helper_scv(CPUPPCState *env, uint32_t lev)
{
    if (env->spr[SPR_FSCR] & (1ull << FSCR_SCV)) {
        raise_exception_err(env, POWERPC_EXCP_SYSCALL_VECTORED, lev);
    } else {
        raise_exception_err(env, POWERPC_EXCP_FU, FSCR_IC_SCV);
    }
}

void helper_pminsn(CPUPPCState *env, powerpc_pm_insn_t insn)
{
    CPUState *cs;

    cs = env_cpu(env);
    cs->halted = 1;

    /* Condition for waking up at 0x100 */
    env->resume_as_sreset = (insn != PPC_PM_STOP) ||
        (env->spr[SPR_PSSCR] & PSSCR_EC);
}
#endif /* defined(TARGET_PPC64) */

static void do_rfi(CPUPPCState *env, target_ulong nip, target_ulong msr)
{
    CPUState *cs = env_cpu(env);

    /* MSR:POW cannot be set by any form of rfi */
    msr &= ~(1ULL << MSR_POW);

    /* MSR:TGPR cannot be set by any form of rfi */
    if (env->flags & POWERPC_FLAG_TGPR)
        msr &= ~(1ULL << MSR_TGPR);

#if defined(TARGET_PPC64)
    /* Switching to 32-bit ? Crop the nip */
    if (!msr_is_64bit(env, msr)) {
        nip = (uint32_t)nip;
    }
#else
    nip = (uint32_t)nip;
#endif
    /* XXX: beware: this is false if VLE is supported */
    env->nip = nip & ~((target_ulong)0x00000003);
    hreg_store_msr(env, msr, 1);
    trace_ppc_excp_rfi(env->nip, env->msr);
    /*
     * No need to raise an exception here, as rfi is always the last
     * insn of a TB
     */
    cpu_interrupt_exittb(cs);
    /* Reset the reservation */
    env->reserve_addr = -1;

    /* Context synchronizing: check if TCG TLB needs flush */
    check_tlb_flush(env, false);
}

void helper_rfi(CPUPPCState *env)
{
    do_rfi(env, env->spr[SPR_SRR0], env->spr[SPR_SRR1] & 0xfffffffful);
}

#define MSR_BOOK3S_MASK
#if defined(TARGET_PPC64)
void helper_rfid(CPUPPCState *env)
{
    /*
     * The architecture defines a number of rules for which bits can
     * change but in practice, we handle this in hreg_store_msr()
     * which will be called by do_rfi(), so there is no need to filter
     * here
     */
    do_rfi(env, env->spr[SPR_SRR0], env->spr[SPR_SRR1]);
}

void helper_rfscv(CPUPPCState *env)
{
    do_rfi(env, env->lr, env->ctr);
}

void helper_hrfid(CPUPPCState *env)
{
    do_rfi(env, env->spr[SPR_HSRR0], env->spr[SPR_HSRR1]);
}
#endif

#if defined(TARGET_PPC64) && !defined(CONFIG_USER_ONLY)
void helper_rfebb(CPUPPCState *env, target_ulong s)
{
    target_ulong msr = env->msr;

    /*
     * Handling of BESCR bits 32:33 according to PowerISA v3.1:
     *
     * "If BESCR 32:33 != 0b00 the instruction is treated as if
     *  the instruction form were invalid."
     */
    if (env->spr[SPR_BESCR] & BESCR_INVALID) {
        raise_exception_err(env, POWERPC_EXCP_PROGRAM,
                            POWERPC_EXCP_INVAL | POWERPC_EXCP_INVAL_INVAL);
    }

    env->nip = env->spr[SPR_EBBRR];

    /* Switching to 32-bit ? Crop the nip */
    if (!msr_is_64bit(env, msr)) {
        env->nip = (uint32_t)env->spr[SPR_EBBRR];
    }

    if (s) {
        env->spr[SPR_BESCR] |= BESCR_GE;
    } else {
        env->spr[SPR_BESCR] &= ~BESCR_GE;
    }
}

/*
 * Triggers or queues an 'ebb_excp' EBB exception. All checks
 * but FSCR, HFSCR and msr_pr must be done beforehand.
 *
 * PowerISA v3.1 isn't clear about whether an EBB should be
 * postponed or cancelled if the EBB facility is unavailable.
 * Our assumption here is that the EBB is cancelled if both
 * FSCR and HFSCR EBB facilities aren't available.
 */
static void do_ebb(CPUPPCState *env, int ebb_excp)
{
    PowerPCCPU *cpu = env_archcpu(env);
    CPUState *cs = CPU(cpu);

    /*
     * FSCR_EBB and FSCR_IC_EBB are the same bits used with
     * HFSCR.
     */
    helper_fscr_facility_check(env, FSCR_EBB, 0, FSCR_IC_EBB);
    helper_hfscr_facility_check(env, FSCR_EBB, "EBB", FSCR_IC_EBB);

    if (ebb_excp == POWERPC_EXCP_PERFM_EBB) {
        env->spr[SPR_BESCR] |= BESCR_PMEO;
    } else if (ebb_excp == POWERPC_EXCP_EXTERNAL_EBB) {
        env->spr[SPR_BESCR] |= BESCR_EEO;
    }

    if (msr_pr == 1) {
        powerpc_excp(cpu, ebb_excp);
    } else {
        env->pending_interrupts |= 1 << PPC_INTERRUPT_EBB;
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    }
}

void raise_ebb_perfm_exception(CPUPPCState *env)
{
    bool perfm_ebb_enabled = env->spr[SPR_POWER_MMCR0] & MMCR0_EBE &&
                             env->spr[SPR_BESCR] & BESCR_PME &&
                             env->spr[SPR_BESCR] & BESCR_GE;

    if (!perfm_ebb_enabled) {
        return;
    }

    do_ebb(env, POWERPC_EXCP_PERFM_EBB);
}
#endif

/*****************************************************************************/
/* Embedded PowerPC specific helpers */
void helper_40x_rfci(CPUPPCState *env)
{
    do_rfi(env, env->spr[SPR_40x_SRR2], env->spr[SPR_40x_SRR3]);
}

void helper_rfci(CPUPPCState *env)
{
    do_rfi(env, env->spr[SPR_BOOKE_CSRR0], env->spr[SPR_BOOKE_CSRR1]);
}

void helper_rfdi(CPUPPCState *env)
{
    /* FIXME: choose CSRR1 or DSRR1 based on cpu type */
    do_rfi(env, env->spr[SPR_BOOKE_DSRR0], env->spr[SPR_BOOKE_DSRR1]);
}

void helper_rfmci(CPUPPCState *env)
{
    /* FIXME: choose CSRR1 or MCSRR1 based on cpu type */
    do_rfi(env, env->spr[SPR_BOOKE_MCSRR0], env->spr[SPR_BOOKE_MCSRR1]);
}
#endif /* CONFIG_TCG */
#endif /* !defined(CONFIG_USER_ONLY) */

#ifdef CONFIG_TCG
void helper_tw(CPUPPCState *env, target_ulong arg1, target_ulong arg2,
               uint32_t flags)
{
    if (!likely(!(((int32_t)arg1 < (int32_t)arg2 && (flags & 0x10)) ||
                  ((int32_t)arg1 > (int32_t)arg2 && (flags & 0x08)) ||
                  ((int32_t)arg1 == (int32_t)arg2 && (flags & 0x04)) ||
                  ((uint32_t)arg1 < (uint32_t)arg2 && (flags & 0x02)) ||
                  ((uint32_t)arg1 > (uint32_t)arg2 && (flags & 0x01))))) {
        raise_exception_err_ra(env, POWERPC_EXCP_PROGRAM,
                               POWERPC_EXCP_TRAP, GETPC());
    }
}

#if defined(TARGET_PPC64)
void helper_td(CPUPPCState *env, target_ulong arg1, target_ulong arg2,
               uint32_t flags)
{
    if (!likely(!(((int64_t)arg1 < (int64_t)arg2 && (flags & 0x10)) ||
                  ((int64_t)arg1 > (int64_t)arg2 && (flags & 0x08)) ||
                  ((int64_t)arg1 == (int64_t)arg2 && (flags & 0x04)) ||
                  ((uint64_t)arg1 < (uint64_t)arg2 && (flags & 0x02)) ||
                  ((uint64_t)arg1 > (uint64_t)arg2 && (flags & 0x01))))) {
        raise_exception_err_ra(env, POWERPC_EXCP_PROGRAM,
                               POWERPC_EXCP_TRAP, GETPC());
    }
}
#endif
#endif

#if !defined(CONFIG_USER_ONLY)

#ifdef CONFIG_TCG

/* Embedded.Processor Control */
static int dbell2irq(target_ulong rb)
{
    int msg = rb & DBELL_TYPE_MASK;
    int irq = -1;

    switch (msg) {
    case DBELL_TYPE_DBELL:
        irq = PPC_INTERRUPT_DOORBELL;
        break;
    case DBELL_TYPE_DBELL_CRIT:
        irq = PPC_INTERRUPT_CDOORBELL;
        break;
    case DBELL_TYPE_G_DBELL:
    case DBELL_TYPE_G_DBELL_CRIT:
    case DBELL_TYPE_G_DBELL_MC:
        /* XXX implement */
    default:
        break;
    }

    return irq;
}

void helper_msgclr(CPUPPCState *env, target_ulong rb)
{
    int irq = dbell2irq(rb);

    if (irq < 0) {
        return;
    }

    env->pending_interrupts &= ~(1 << irq);
}

void helper_msgsnd(target_ulong rb)
{
    int irq = dbell2irq(rb);
    int pir = rb & DBELL_PIRTAG_MASK;
    CPUState *cs;

    if (irq < 0) {
        return;
    }

    qemu_mutex_lock_iothread();
    CPU_FOREACH(cs) {
        PowerPCCPU *cpu = POWERPC_CPU(cs);
        CPUPPCState *cenv = &cpu->env;

        if ((rb & DBELL_BRDCAST) || (cenv->spr[SPR_BOOKE_PIR] == pir)) {
            cenv->pending_interrupts |= 1 << irq;
            cpu_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
    qemu_mutex_unlock_iothread();
}

/* Server Processor Control */

static bool dbell_type_server(target_ulong rb)
{
    /*
     * A Directed Hypervisor Doorbell message is sent only if the
     * message type is 5. All other types are reserved and the
     * instruction is a no-op
     */
    return (rb & DBELL_TYPE_MASK) == DBELL_TYPE_DBELL_SERVER;
}

void helper_book3s_msgclr(CPUPPCState *env, target_ulong rb)
{
    if (!dbell_type_server(rb)) {
        return;
    }

    env->pending_interrupts &= ~(1 << PPC_INTERRUPT_HDOORBELL);
}

static void book3s_msgsnd_common(int pir, int irq)
{
    CPUState *cs;

    qemu_mutex_lock_iothread();
    CPU_FOREACH(cs) {
        PowerPCCPU *cpu = POWERPC_CPU(cs);
        CPUPPCState *cenv = &cpu->env;

        /* TODO: broadcast message to all threads of the same  processor */
        if (cenv->spr_cb[SPR_PIR].default_value == pir) {
            cenv->pending_interrupts |= 1 << irq;
            cpu_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
    qemu_mutex_unlock_iothread();
}

void helper_book3s_msgsnd(target_ulong rb)
{
    int pir = rb & DBELL_PROCIDTAG_MASK;

    if (!dbell_type_server(rb)) {
        return;
    }

    book3s_msgsnd_common(pir, PPC_INTERRUPT_HDOORBELL);
}

#if defined(TARGET_PPC64)
void helper_book3s_msgclrp(CPUPPCState *env, target_ulong rb)
{
    helper_hfscr_facility_check(env, HFSCR_MSGP, "msgclrp", HFSCR_IC_MSGP);

    if (!dbell_type_server(rb)) {
        return;
    }

    env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DOORBELL);
}

/*
 * sends a message to other threads that are on the same
 * multi-threaded processor
 */
void helper_book3s_msgsndp(CPUPPCState *env, target_ulong rb)
{
    int pir = env->spr_cb[SPR_PIR].default_value;

    helper_hfscr_facility_check(env, HFSCR_MSGP, "msgsndp", HFSCR_IC_MSGP);

    if (!dbell_type_server(rb)) {
        return;
    }

    /* TODO: TCG supports only one thread */

    book3s_msgsnd_common(pir, PPC_INTERRUPT_DOORBELL);
}
#endif /* TARGET_PPC64 */

void ppc_cpu_do_unaligned_access(CPUState *cs, vaddr vaddr,
                                 MMUAccessType access_type,
                                 int mmu_idx, uintptr_t retaddr)
{
    CPUPPCState *env = cs->env_ptr;
    uint32_t insn;

    /* Restore state and reload the insn we executed, for filling in DSISR.  */
    cpu_restore_state(cs, retaddr, true);
    insn = cpu_ldl_code(env, env->nip);

    switch (env->mmu_model) {
    case POWERPC_MMU_SOFT_4xx:
        env->spr[SPR_40x_DEAR] = vaddr;
        break;
    case POWERPC_MMU_BOOKE:
    case POWERPC_MMU_BOOKE206:
        env->spr[SPR_BOOKE_DEAR] = vaddr;
        break;
    default:
        env->spr[SPR_DAR] = vaddr;
        break;
    }

    cs->exception_index = POWERPC_EXCP_ALIGN;
    env->error_code = insn & 0x03FF0000;
    cpu_loop_exit(cs);
}
#endif /* CONFIG_TCG */
#endif /* !CONFIG_USER_ONLY */
