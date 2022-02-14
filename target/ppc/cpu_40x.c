/*
 * CPU initialization for PowerPC 40x CPUs
 *
 *  Copyright IBM Corp. 2022
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/ppc/ppc.h"
#include "cpu.h"
#include "spr_common.h"

/* SPR shared between PowerPC 40x implementations */
static void register_40x_sprs(CPUPPCState *env)
{
    /* Cache */
    /* not emulated, as QEMU do not emulate caches */
    spr_register(env, SPR_40x_DCCR, "DCCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* not emulated, as QEMU do not emulate caches */
    spr_register(env, SPR_40x_ICCR, "ICCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* not emulated, as QEMU do not emulate caches */
    spr_register(env, SPR_BOOKE_ICDBDR, "ICDBDR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
                 0x00000000);
    /* Exception */
    spr_register(env, SPR_40x_DEAR, "DEAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_40x_ESR, "ESR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_40x_EVPR, "EVPR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_excp_prefix,
                 0x00000000);
    spr_register(env, SPR_40x_SRR2, "SRR2",
                 &spr_read_generic, &spr_write_generic,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_40x_SRR3, "SRR3",
                 &spr_read_generic, &spr_write_generic,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Timers */
    spr_register(env, SPR_40x_PIT, "PIT",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_40x_pit, &spr_write_40x_pit,
                 0x00000000);
    spr_register(env, SPR_40x_TCR, "TCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_40x_tcr,
                 0x00000000);
    spr_register(env, SPR_40x_TSR, "TSR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_40x_tsr,
                 0x00000000);
}

/* SPR specific to PowerPC 405 implementation */
static void register_405_sprs(CPUPPCState *env)
{
    /* MMU */
    spr_register(env, SPR_40x_PID, "PID",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_40x_pid,
                 0x00000000);
    spr_register(env, SPR_4xx_CCR0, "CCR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00700000);
    /* Debug interface */
    spr_register(env, SPR_40x_DBCR0, "DBCR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_40x_dbcr0,
                 0x00000000);

    spr_register(env, SPR_405_DBCR1, "DBCR1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_40x_DBSR, "DBSR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_clear,
                 /* Last reset was system reset */
                 0x00000300);

    spr_register(env, SPR_40x_DAC1, "DAC1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_40x_DAC2, "DAC2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_405_DVC1, "DVC1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_405_DVC2, "DVC2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_40x_IAC1, "IAC1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_40x_IAC2, "IAC2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_405_IAC3, "IAC3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_405_IAC4, "IAC4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Storage control */
    spr_register(env, SPR_405_SLER, "SLER",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_40x_sler,
                 0x00000000);
    spr_register(env, SPR_40x_ZPR, "ZPR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_405_SU0R, "SU0R",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* SPRG */
    spr_register(env, SPR_USPRG0, "USPRG0",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);
    spr_register(env, SPR_SPRG4, "SPRG4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG5, "SPRG5",
                 SPR_NOACCESS, SPR_NOACCESS,
                 spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG6, "SPRG6",
                 SPR_NOACCESS, SPR_NOACCESS,
                 spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG7, "SPRG7",
                 SPR_NOACCESS, SPR_NOACCESS,
                 spr_read_generic, &spr_write_generic,
                 0x00000000);

    /* Bus access control */
    /* not emulated, as QEMU never does speculative access */
    spr_register(env, SPR_40x_SGR, "SGR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0xFFFFFFFF);
    /* not emulated, as QEMU do not emulate caches */
    spr_register(env, SPR_40x_DCWR, "DCWR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void init_excp_4xx_softmmu(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_CRITICAL] = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_PIT]      = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_FIT]      = 0x00001010;
    env->excp_vectors[POWERPC_EXCP_WDT]      = 0x00001020;
    env->excp_vectors[POWERPC_EXCP_DTLB]     = 0x00001100;
    env->excp_vectors[POWERPC_EXCP_ITLB]     = 0x00001200;
    env->excp_vectors[POWERPC_EXCP_DEBUG]    = 0x00002000;
    env->ivor_mask = 0x0000FFF0UL;
    env->ivpr_mask = 0xFFFF0000UL;
    /* Hardware reset vector */
    env->hreset_vector = 0xFFFFFFFCUL;
#endif
}

static void init_proc_405(CPUPPCState *env)
{
    register_40x_sprs(env);
    register_405_sprs(env);
    register_usprgh_sprs(env);

    /* Memory management */
#if !defined(CONFIG_USER_ONLY)
    env->nb_tlb = 64;
    env->nb_ways = 1;
    env->id_tlbs = 0;
    env->tlb_type = TLB_EMB;
#endif
    init_excp_4xx_softmmu(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc40x_irq_init(env_archcpu(env));

    SET_FIT_PERIOD(8, 12, 16, 20);
    SET_WDT_PERIOD(16, 20, 24, 28);
}

POWERPC_FAMILY(405)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 405";
    pcc->init_proc = init_proc_405;
    pcc->check_pow = check_pow_nocheck;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_DCR | PPC_WRTEE |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_40x_ICBT |
                       PPC_CACHE_DCBZ | PPC_CACHE_DCBA |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_40x_TLB | PPC_MEM_TLBIA | PPC_MEM_TLBSYNC |
                       PPC_4xx_COMMON | PPC_405_MAC | PPC_40x_EXCP;
    pcc->msr_mask = (1ull << MSR_WE) |
                    (1ull << MSR_CE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_DWE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR);
    pcc->mmu_model = POWERPC_MMU_SOFT_4xx;
    pcc->excp_model = POWERPC_EXCP_40x;
    pcc->bus_model = PPC_FLAGS_INPUT_405;
    pcc->bfd_mach = bfd_mach_ppc_403;
    pcc->flags = POWERPC_FLAG_CE | POWERPC_FLAG_DWE |
                 POWERPC_FLAG_DE | POWERPC_FLAG_BUS_CLK;
}
