#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "ppc_intr.h"

/* for hreg_swap_gpr_tgpr */
#include "helper_regs.h"

/* #define DEBUG_SOFTWARE_TLB */
/* #define DEBUG_EXCEPTIONS */

#ifdef DEBUG_EXCEPTIONS
#  define LOG_EXCP(...) qemu_log(__VA_ARGS__)
#else
#  define LOG_EXCP(...) do { } while (0)
#endif

void __ppc_intr_add(CPUPPCState *env, target_ulong addr, int id,
                   const char *intr_name)
{
    PPCInterrupt *intr = &env->entry_points[id];

    object_initialize(intr, sizeof(*intr), intr_name);
    intr->addr = addr;
}

static const TypeInfo ppc_interrupt_info = {
    .name = TYPE_PPC_INTERRUPT,
    .parent = TYPE_OBJECT,
    .abstract = true,
};

static void ppc_interrupt_register_types(void)
{
    type_register_static(&ppc_interrupt_info);
}
type_init(ppc_interrupt_register_types);



static void ppc_intr_def_log(PowerPCCPU *cpu, PPCInterrupt *intr,
                             int excp_model, ppc_intr_args *regs, bool *ignore)
{
    LOG_EXCP("%s interrupt\n", intr->name);
}

static void ppc_intr_def_not_impl(PowerPCCPU *cpu, PPCInterrupt *intr,
                                  int excp_model, ppc_intr_args *regs,
                                  bool *ignore)
{
    CPUState *cs = CPU(cpu);

    cpu_abort(cs, "%s interrupt not implemented yet.\n", intr->name);
}

static void ppc_intr_def_no_op(PowerPCCPU *cpu, PPCInterrupt *intr,
                               int excp_model, ppc_intr_args *regs,
                               bool *ignore)
{
}

/* Default implementation for interrupts that set the MSR_HV bit */
static void ppc_intr_def_hv(PowerPCCPU *cpu, PPCInterrupt *intr, int excp_model,
                            ppc_intr_args *regs, bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    regs->sprn_srr0 = SPR_HSRR0;
    regs->sprn_srr1 = SPR_HSRR1;
    regs->new_msr |= (target_ulong)MSR_HVB;
    regs->new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
}

/* Default implementation for Facility Unavailable interrupts */
static void ppc_intr_def_fac_unavail_64(PowerPCCPU *cpu, PPCInterrupt *intr,
                                        int excp_model, ppc_intr_args *regs,
                                        bool *ignore)
{
#ifdef TARGET_PPC64
    CPUPPCState *env = &cpu->env;
    env->spr[SPR_FSCR] |= ((target_ulong)env->error_code << FSCR_IC_POS);
#endif
}

static void ppc_debug_software_tlb(CPUPPCState *env, int excp, int excp_model,
                                   int imiss_sprn, int icmp_sprn,
                                   int dmiss_sprn, int dcmp_sprn)
{
#if defined(DEBUG_SOFTWARE_TLB)
    if (qemu_log_enabled()) {
        const char *es;
        target_ulong *miss, *cmp;
        int en;

        if (excp == POWERPC_EXCP_IFTLB) {
            es = "I";
            en = 'I';
            miss = &env->spr[imiss_sprn];
            cmp = &env->spr[icmp_sprn];
        } else {
            if (excp == POWERPC_EXCP_DLTLB) {
                es = "DL";
            } else {
                es = "DS";
            }
            en = 'D';
            miss = &env->spr[dmiss_sprn];
            cmp = &env->spr[dcmp_srpn];
        }

        if (excp_model == POWERPC_EXCP_74xx) {
            qemu_log("74xx %sTLB miss: %cM " TARGET_FMT_lx " %cC "
                     TARGET_FMT_lx " %08x\n", es, en, *miss, en, *cmp,
                     env->error_code);
        } else {
            qemu_log("6xx %sTLB miss: %cM " TARGET_FMT_lx " %cC "
                     TARGET_FMT_lx " H1 " TARGET_FMT_lx " H2 "
                     TARGET_FMT_lx " %08x\n", es, en, *miss, en, *cmp,
                     env->spr[SPR_HASH1], env->spr[SPR_HASH2],
                     env->error_code);
        }
    }
#endif
}

static void ppc_intr_def_tlb_miss(PowerPCCPU *cpu, PPCInterrupt *intr,
                                  int excp_model, ppc_intr_args *regs,
                                  bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    switch (excp_model) {
    case POWERPC_EXCP_602:
    case POWERPC_EXCP_603:
    case POWERPC_EXCP_603E:
    case POWERPC_EXCP_G2:
        /* Swap temporary saved registers with GPRs */
        if (!(regs->new_msr & ((target_ulong)1 << MSR_TGPR))) {
            regs->new_msr |= (target_ulong)1 << MSR_TGPR;
            hreg_swap_gpr_tgpr(env);
        }

        ppc_debug_software_tlb(env, intr->id, excp_model,
                               SPR_IMISS, SPR_ICMP,
                               SPR_DMISS, SPR_DCMP);

        /* fall through */
    case POWERPC_EXCP_7x5:
        regs->msr |= env->crf[0] << 28;
        regs->msr |= env->error_code; /* key, D/I, S/L bits */

        /* Set way using a LRU mechanism */
        regs->msr |= ((env->last_way + 1) & (env->nb_ways - 1)) << 17;

        break;
    case POWERPC_EXCP_74xx:
        ppc_debug_software_tlb(env, intr->id, excp_model,
                               SPR_TLBMISS, SPR_PTEHI,
                               SPR_TLBMISS, SPR_PTEHI);

        regs->msr |= env->error_code; /* key bit */
        break;
    default:
        cpu_abort(cs, "Invalid instruction TLB miss exception\n");
        break;
    }
}

static void ppc_intr_critical(PowerPCCPU *cpu, PPCInterrupt *intr,
                              int excp_model, ppc_intr_args *regs, bool *ignore)
{
    CPUState *cs = CPU(cpu);

    switch (excp_model) {
    case POWERPC_EXCP_40x:
        regs->sprn_srr0 = SPR_40x_SRR2;
        regs->sprn_srr1 = SPR_40x_SRR3;
        break;
    case POWERPC_EXCP_BOOKE:
        regs->sprn_srr0 = SPR_BOOKE_CSRR0;
        regs->sprn_srr1 = SPR_BOOKE_CSRR1;
        break;
    case POWERPC_EXCP_G2:
        break;
    default:
        cpu_abort(cs, "Invalid Critical interrupt for model %d. Aborting\n",
                  excp_model);
        break;
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_CRITICAL, critical, "Critical input");

static void ppc_intr_machine_check(PowerPCCPU *cpu, PPCInterrupt *intr,
                                   int excp_model, ppc_intr_args *regs,
                                   bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

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
        regs->new_msr |= (target_ulong)MSR_HVB;
    }

    /* machine check exceptions don't have ME set */
    regs->new_msr &= ~((target_ulong)1 << MSR_ME);

    /* XXX: should also have something loaded in DAR / DSISR */
    switch (excp_model) {
    case POWERPC_EXCP_40x:
        regs->sprn_srr0 = SPR_40x_SRR2;
        regs->sprn_srr1 = SPR_40x_SRR3;
        break;
    case POWERPC_EXCP_BOOKE:
        /* FIXME: choose one or the other based on CPU type */
        regs->sprn_srr0 = SPR_BOOKE_MCSRR0;
        regs->sprn_srr1 = SPR_BOOKE_MCSRR1;
        regs->sprn_asrr0 = SPR_BOOKE_CSRR0;
        regs->sprn_asrr1 = SPR_BOOKE_CSRR1;
        break;
    default:
        break;
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_MCHECK, machine_check, "Machine check");

static void ppc_intr_data_storage(PowerPCCPU *cpu, PPCInterrupt *intr,
                                  int excp_model, ppc_intr_args *regs,
                                  bool *ignore)
{
    LOG_EXCP("DSI exception: DSISR=" TARGET_FMT_lx" DAR=" TARGET_FMT_lx
             "\n", env->spr[SPR_DSISR], env->spr[SPR_DAR]);
}
PPC_DEFINE_INTR(POWERPC_EXCP_DSI, data_storage, "Data storage");

static void ppc_intr_insn_storage(PowerPCCPU *cpu, PPCInterrupt *intr,
                                  int excp_model, ppc_intr_args *regs,
                                  bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    LOG_EXCP("ISI exception: msr=" TARGET_FMT_lx ", nip=" TARGET_FMT_lx
             "\n", regs->msr, regs->nip);
    regs->msr |= env->error_code;
}
PPC_DEFINE_INTR(POWERPC_EXCP_ISI, insn_storage, "Instruction storage");

static void ppc_intr_external(PowerPCCPU *cpu, PPCInterrupt *intr,
                              int excp_model, ppc_intr_args *regs, bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;
    bool lpes0;

    /*
     * Exception targeting modifiers
     *
     * LPES0 is supported on POWER7/8/9
     * LPES1 is not supported (old iSeries mode)
     *
     * On anything else, we behave as if LPES0 is 1
     * (externals don't alter MSR:HV)
     */
#if defined(TARGET_PPC64)
    if (excp_model == POWERPC_EXCP_POWER7 ||
        excp_model == POWERPC_EXCP_POWER8 ||
        excp_model == POWERPC_EXCP_POWER9 ||
        excp_model == POWERPC_EXCP_POWER10) {
        lpes0 = !!(env->spr[SPR_LPCR] & LPCR_LPES0);
    } else
#endif /* defined(TARGET_PPC64) */
    {
        lpes0 = true;
    }

    if (!lpes0) {
        regs->new_msr |= (target_ulong)MSR_HVB;
        regs->new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
        regs->sprn_srr0 = SPR_HSRR0;
        regs->sprn_srr1 = SPR_HSRR1;
    }
    if (env->mpic_proxy) {
        /* IACK the IRQ on delivery */
        env->spr[SPR_BOOKE_EPR] = ldl_phys(cs->as, env->mpic_iack);
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_EXTERNAL, external, "External");

static void ppc_intr_alignment(PowerPCCPU *cpu, PPCInterrupt *intr,
                               int excp_model, ppc_intr_args *regs,
                               bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    /* Get rS/rD and rA from faulting opcode */
    /*
     * Note: the opcode fields will not be set properly for a
     * direct store load/store, but nobody cares as nobody
     * actually uses direct store segments.
     */
    env->spr[SPR_DSISR] |= (env->error_code & 0x03FF0000) >> 16;
}
PPC_DEFINE_INTR(POWERPC_EXCP_ALIGN, alignment, "Alignment");

static void ppc_intr_program(PowerPCCPU *cpu, PPCInterrupt *intr,
                             int excp_model, ppc_intr_args *regs, bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    switch (env->error_code & ~0xF) {
    case POWERPC_EXCP_FP:
        if ((msr_fe0 == 0 && msr_fe1 == 0) || msr_fp == 0) {
            LOG_EXCP("Ignore floating point exception\n");
            cs->exception_index = POWERPC_EXCP_NONE;
            env->error_code = 0;

            *ignore = true;
            return;
        }

        /*
         * FP exceptions always have NIP pointing to the faulting
         * instruction, so always use store_next and claim we are
         * precise in the MSR.
         */
        regs->msr |= 0x00100000;
        env->spr[SPR_BOOKE_ESR] = ESR_FP;
        break;
    case POWERPC_EXCP_INVAL:
        LOG_EXCP("Invalid instruction at " TARGET_FMT_lx "\n", regs->nip);
        regs->msr |= 0x00080000;
        env->spr[SPR_BOOKE_ESR] = ESR_PIL;
        break;
    case POWERPC_EXCP_PRIV:
        regs->msr |= 0x00040000;
        env->spr[SPR_BOOKE_ESR] = ESR_PPR;
        break;
    case POWERPC_EXCP_TRAP:
        regs->msr |= 0x00020000;
        env->spr[SPR_BOOKE_ESR] = ESR_PTR;
        break;
    default:
        /* Should never occur */
        cpu_abort(cs, "Invalid program exception %d. Aborting\n",
                  env->error_code);
        break;
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_PROGRAM, program, "Program");


static inline void dump_syscall(CPUPPCState *env)
{
    qemu_log_mask(CPU_LOG_INT, "syscall r0=%016" PRIx64
                  " r3=%016" PRIx64 " r4=%016" PRIx64 " r5=%016" PRIx64
                  " r6=%016" PRIx64 " r7=%016" PRIx64 " r8=%016" PRIx64
                  " nip=" TARGET_FMT_lx "\n",
                  ppc_dump_gpr(env, 0), ppc_dump_gpr(env, 3),
                  ppc_dump_gpr(env, 4), ppc_dump_gpr(env, 5),
                  ppc_dump_gpr(env, 6), ppc_dump_gpr(env, 7),
                  ppc_dump_gpr(env, 8), env->nip);
}

static inline void dump_hcall(CPUPPCState *env)
{
    qemu_log_mask(CPU_LOG_INT, "hypercall r3=%016" PRIx64
                  " r4=%016" PRIx64 " r5=%016" PRIx64 " r6=%016" PRIx64
                  " r7=%016" PRIx64 " r8=%016" PRIx64 " r9=%016" PRIx64
                  " r10=%016" PRIx64 " r11=%016" PRIx64 " r12=%016" PRIx64
                  " nip=" TARGET_FMT_lx "\n",
                  ppc_dump_gpr(env, 3), ppc_dump_gpr(env, 4),
                  ppc_dump_gpr(env, 5), ppc_dump_gpr(env, 6),
                  ppc_dump_gpr(env, 7), ppc_dump_gpr(env, 8),
                  ppc_dump_gpr(env, 9), ppc_dump_gpr(env, 10),
                  ppc_dump_gpr(env, 11), ppc_dump_gpr(env, 12),
                  env->nip);
}


static void ppc_intr_system_call(PowerPCCPU *cpu, PPCInterrupt *intr,
                                 int excp_model, ppc_intr_args *regs,
                                 bool *ignore)
{
    CPUPPCState *env = &cpu->env;
    int lev;

    lev = env->error_code;

    if ((lev == 1) && cpu->vhyp) {
        dump_hcall(env);
    } else {
        dump_syscall(env);
    }

    /*
     * We need to correct the NIP which in this case is supposed
     * to point to the next instruction. We also set env->nip here
     * because the modification needs to be accessible by the
     * virtual hypervisor code below.
     */
    regs->nip += 4;
    env->nip = regs->nip;

    /* "PAPR mode" built-in hypercall emulation */
    if ((lev == 1) && cpu->vhyp) {
        PPCVirtualHypervisorClass *vhc =
            PPC_VIRTUAL_HYPERVISOR_GET_CLASS(cpu->vhyp);
        vhc->hypercall(cpu->vhyp, cpu);

        *ignore = true;
        return;
    }

    if (lev == 1) {
        regs->new_msr |= (target_ulong)MSR_HVB;
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_SYSCALL, system_call, "System call");

static void ppc_intr_system_call_vectored(PowerPCCPU *cpu, PPCInterrupt *intr,
                                          int excp_model, ppc_intr_args *regs,
                                          bool *ignore)
{
    CPUPPCState *env = &cpu->env;
    int lev;

    lev = env->error_code;
    dump_syscall(env);

    regs->nip += 4;
    regs->new_msr |= env->msr & ((target_ulong)1 << MSR_EE);
    regs->new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
    regs->new_nip += lev * 0x20;

    env->lr = regs->nip;
    env->ctr = regs->msr;
}
PPC_DEFINE_INTR(POWERPC_EXCP_SYSCALL_VECTORED, system_call_vectored,
                     "System call vectored");

static void ppc_intr_watchdog(PowerPCCPU *cpu, PPCInterrupt *intr,
                              int excp_model, ppc_intr_args *regs, bool *ignore)
{
    LOG_EXCP("WDT exception\n");
    switch (excp_model) {
    case POWERPC_EXCP_BOOKE:
        regs->sprn_srr0 = SPR_BOOKE_CSRR0;
        regs->sprn_srr1 = SPR_BOOKE_CSRR1;
        break;
    default:
        break;
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_WDT, watchdog, "Watchdog timer");

static void ppc_intr_debug(PowerPCCPU *cpu, PPCInterrupt *intr, int excp_model,
                           ppc_intr_args *regs, bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    if (env->flags & POWERPC_FLAG_DE) {
        /* FIXME: choose one or the other based on CPU type */
        regs->sprn_srr0 = SPR_BOOKE_DSRR0;
        regs->sprn_srr1 = SPR_BOOKE_DSRR1;
        regs->sprn_asrr0 = SPR_BOOKE_CSRR0;
        regs->sprn_asrr1 = SPR_BOOKE_CSRR1;
        /* DBSR already modified by caller */
    } else {
        cpu_abort(cs, "Debug exception triggered on unsupported model\n");
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_DEBUG, debug, "Debug");

static void ppc_intr_spe_unavailable(PowerPCCPU *cpu, PPCInterrupt *intr,
                                     int excp_model, ppc_intr_args *regs,
                                     bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    env->spr[SPR_BOOKE_ESR] = ESR_SPV;
}
PPC_DEFINE_INTR(POWERPC_EXCP_SPEU, spe_unavailable,
                     "SPE/embedded floating-point unavailable");

static void ppc_intr_embedded_fp_data(PowerPCCPU *cpu, PPCInterrupt *intr,
                                      int excp_model, ppc_intr_args *regs,
                                      bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    ppc_intr_def_not_impl(cpu, intr, excp_model, regs, ignore);
    env->spr[SPR_BOOKE_ESR] = ESR_SPV;
}
PPC_DEFINE_INTR(POWERPC_EXCP_EFPDI, embedded_fp_data,
                     "Embedded floating-point data");

static void ppc_intr_embedded_fp_round(PowerPCCPU *cpu, PPCInterrupt *intr,
                                       int excp_model, ppc_intr_args *regs,
                                       bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    ppc_intr_def_not_impl(cpu, intr, excp_model, regs, ignore);
    env->spr[SPR_BOOKE_ESR] = ESR_SPV;
}
PPC_DEFINE_INTR(POWERPC_EXCP_EFPRI, embedded_fp_round,
                     "Embedded floating-point round");

static void ppc_intr_embedded_doorbell_crit(PowerPCCPU *cpu, PPCInterrupt *intr,
                                            int excp_model, ppc_intr_args *regs,
                                            bool *ignore)
{
    regs->sprn_srr0 = SPR_BOOKE_CSRR0;
    regs->sprn_srr1 = SPR_BOOKE_CSRR1;
}
PPC_DEFINE_INTR(POWERPC_EXCP_DOORCI, embedded_doorbell_crit,
                     "Embedded doorbell critical");

static void ppc_intr_system_reset(PowerPCCPU *cpu, PPCInterrupt *intr,
                                  int excp_model, ppc_intr_args *regs,
                                  bool *ignore)
{
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    /* A power-saving exception sets ME, otherwise it is unchanged */
    if (msr_pow) {
        /* indicate that we resumed from power save mode */
        regs->msr |= 0x10000;
        regs->new_msr |= ((target_ulong)1 << MSR_ME);
    }
    if (env->msr_mask & MSR_HVB) {
        /*
         * ISA specifies HV, but can be delivered to guest with HV
         * clear (e.g., see FWNMI in PAPR, NMI injection in QEMU).
         */
        regs->new_msr |= (target_ulong)MSR_HVB;
    } else {
        if (msr_pow) {
            cpu_abort(cs, "Trying to deliver power-saving system reset "
                      "exception with no HV support\n");
        }
    }
}
PPC_DEFINE_INTR(POWERPC_EXCP_RESET, system_reset, "System reset");

static void ppc_intr_hv_insn_storage(PowerPCCPU *cpu, PPCInterrupt *intr,
                                     int excp_model, ppc_intr_args *regs,
                                     bool *ignore)
{
    CPUPPCState *env = &cpu->env;

    regs->msr |= env->error_code;
    ppc_intr_def_hv(cpu, intr, excp_model, regs, ignore);
}
PPC_DEFINE_INTR(POWERPC_EXCP_HISI, hv_insn_storage,
                     "Hypervisor instruction storage");

static void ppc_intr_hv_facility_unavail(PowerPCCPU *cpu, PPCInterrupt *intr,
                                         int excp_model, ppc_intr_args *regs,
                                         bool *ignore)
{
#ifdef TARGET_PPC64
    ppc_intr_def_fac_unavail_64(cpu, intr, excp_model, regs, ignore);
    ppc_intr_def_hv(cpu, intr, excp_model, regs, ignore);
#endif
}
PPC_DEFINE_INTR(POWERPC_EXCP_HV_FU, hv_facility_unavail,
                "Hypervisor facility unavailable");

PPC_DEFINE_INTR(POWERPC_EXCP_HDECR,    def_hv, "Hypervisor decrementer");
PPC_DEFINE_INTR(POWERPC_EXCP_HDSI,     def_hv, "Hypervisor data storage");
PPC_DEFINE_INTR(POWERPC_EXCP_HDSEG,    def_hv, "Hypervisor data segment");
PPC_DEFINE_INTR(POWERPC_EXCP_HISEG,    def_hv, "Hypervisor insn segment");
PPC_DEFINE_INTR(POWERPC_EXCP_SDOOR_HV, def_hv, "Hypervisor doorbell");
PPC_DEFINE_INTR(POWERPC_EXCP_HV_EMU,   def_hv, "Hypervisor emulation assist");
PPC_DEFINE_INTR(POWERPC_EXCP_HVIRT,    def_hv, "Hypervisor virtualization");

PPC_DEFINE_INTR(POWERPC_EXCP_VPU,  def_fac_unavail_64, "Vector unavailable");
PPC_DEFINE_INTR(POWERPC_EXCP_VSXU, def_fac_unavail_64, "VSX unavailable");
PPC_DEFINE_INTR(POWERPC_EXCP_FU,   def_fac_unavail_64, "Facility unavailable");

PPC_DEFINE_INTR(POWERPC_EXCP_IFTLB, def_tlb_miss, "Insn fetch TLB error");
PPC_DEFINE_INTR(POWERPC_EXCP_DLTLB, def_tlb_miss, "Data load TLB error");
PPC_DEFINE_INTR(POWERPC_EXCP_DSTLB, def_tlb_miss, "Data store TLB error");

PPC_DEFINE_INTR(POWERPC_EXCP_FIT, def_log, "Fixed-interval timer");
PPC_DEFINE_INTR(POWERPC_EXCP_PIT, def_log, "Programmable interval timer");

PPC_DEFINE_INTR(POWERPC_EXCP_FPU,   def_no_op, "Floating-point unavailable");
PPC_DEFINE_INTR(POWERPC_EXCP_APU,   def_no_op, "Aux. processor unavailable");
PPC_DEFINE_INTR(POWERPC_EXCP_DECR,  def_no_op, "Decrementer");
PPC_DEFINE_INTR(POWERPC_EXCP_DTLB,  def_no_op, "Data TLB error");
PPC_DEFINE_INTR(POWERPC_EXCP_ITLB,  def_no_op, "Instruction TLB error");
PPC_DEFINE_INTR(POWERPC_EXCP_DOORI, def_no_op, "Embedded doorbell");
PPC_DEFINE_INTR(POWERPC_EXCP_DSEG,  def_no_op, "Data segment");
PPC_DEFINE_INTR(POWERPC_EXCP_ISEG,  def_no_op, "Instruction segment");
PPC_DEFINE_INTR(POWERPC_EXCP_TRACE, def_no_op, "Trace");

PPC_DEFINE_INTR(POWERPC_EXCP_EPERFM,  def_not_impl, "Embedded perf. monitor");
PPC_DEFINE_INTR(POWERPC_EXCP_IO,      def_not_impl, "IO error");
PPC_DEFINE_INTR(POWERPC_EXCP_RUNM,    def_not_impl, "Run mode");
PPC_DEFINE_INTR(POWERPC_EXCP_EMUL,    def_not_impl, "Emulation trap");
PPC_DEFINE_INTR(POWERPC_EXCP_FPA,     def_not_impl, "Floating-point assist");
PPC_DEFINE_INTR(POWERPC_EXCP_DABR,    def_not_impl, "Data address breakpoint");
PPC_DEFINE_INTR(POWERPC_EXCP_IABR,    def_not_impl, "Insn address breakpoint");
PPC_DEFINE_INTR(POWERPC_EXCP_SMI,     def_not_impl, "System management");
PPC_DEFINE_INTR(POWERPC_EXCP_THERM,   def_not_impl, "Thermal management");
PPC_DEFINE_INTR(POWERPC_EXCP_PERFM,   def_not_impl, "Performance counter");
PPC_DEFINE_INTR(POWERPC_EXCP_VPUA,    def_not_impl, "Vector assist");
PPC_DEFINE_INTR(POWERPC_EXCP_SOFTP,   def_not_impl, "Soft patch");
PPC_DEFINE_INTR(POWERPC_EXCP_MAINT,   def_not_impl, "Maintenance");
PPC_DEFINE_INTR(POWERPC_EXCP_MEXTBR,  def_not_impl, "Maskable external");
PPC_DEFINE_INTR(POWERPC_EXCP_NMEXTBR, def_not_impl, "Non-maskable external");

/* These are used by P7 and P8 but were never implemented */
PPC_DEFINE_INTR(POWERPC_EXCP_SDOOR, def_not_impl, "Server doorbell");
PPC_DEFINE_INTR(POWERPC_EXCP_HV_MAINT, def_not_impl, "Hypervisor maintenance");
