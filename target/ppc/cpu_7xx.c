/*
 * CPU initialization for PowerPC 7xx CPUs
 *
 *  Copyright IBM Corp. 2022
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "sysemu/hw_accel.h"
#include "hw/ppc/ppc.h"
#include "cpu.h"
#include "spr_common.h"

static int check_pow_hid0(CPUPPCState *env)
{
    if (env->spr[SPR_HID0] & 0x00E00000) {
        return 1;
    }

    return 0;
}

static void register_745_sprs(CPUPPCState *env)
{
    /* SGPRs */
    spr_register(env, SPR_SPRG4, "SPRG4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG5, "SPRG5",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG6, "SPRG6",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_SPRG7, "SPRG7",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    /* Hardware implementation registers */
    spr_register(env, SPR_HID0, "HID0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_HID1, "HID1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_HID2, "HID2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_755_sprs(CPUPPCState *env)
{
    /* L2 cache control */
    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);

    spr_register(env, SPR_L2PMCR, "L2PMCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

/* SPR common to all 7xx PowerPC implementations */
static void register_7xx_sprs(CPUPPCState *env)
{
    /* Breakpoints */
    spr_register_kvm(env, SPR_DABR, "DABR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DABR, 0x00000000);

    spr_register(env, SPR_IABR, "IABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Cache management */
    spr_register(env, SPR_ICTC, "ICTC",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Performance monitors */
    spr_register(env, SPR_7XX_MMCR0, "MMCR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_MMCR1, "MMCR1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_PMC1, "PMC1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_PMC2, "PMC2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_PMC3, "PMC3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_PMC4, "PMC4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_7XX_SIAR, "SIAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UMMCR0, "UMMCR0",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UMMCR1, "UMMCR1",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UPMC1, "UPMC1",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UPMC2, "UPMC2",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UPMC3, "UPMC3",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_UPMC4, "UPMC4",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_7XX_USIAR, "USIAR",
                 &spr_read_ureg, SPR_NOACCESS,
                 &spr_read_ureg, SPR_NOACCESS,
                 0x00000000);
    /* External access control */
    spr_register(env, SPR_EAR, "EAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    /* Hardware implementation registers */
    spr_register(env, SPR_HID0, "HID0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_HID1, "HID1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void init_excp_7x0(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_SMI]      = 0x00001400;
    env->excp_vectors[POWERPC_EXCP_THERM]    = 0x00001700;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_750cl(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_SMI]      = 0x00001400;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_750cx(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_THERM]    = 0x00001700;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

/* XXX: Check if this is correct */
static void init_excp_7x5(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    env->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    env->excp_vectors[POWERPC_EXCP_IFTLB]    = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_DLTLB]    = 0x00001100;
    env->excp_vectors[POWERPC_EXCP_DSTLB]    = 0x00001200;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_SMI]      = 0x00001400;
    env->excp_vectors[POWERPC_EXCP_THERM]    = 0x00001700;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_proc_740(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);
    /* Thermal management */
    register_thrm_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    init_excp_7x0(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_750(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);

    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);
    /* Thermal management */
    register_thrm_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    /*
     * XXX: high BATs are also present but are known to be bugged on
     *      die version 1.x
     */
    init_excp_7x0(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_750cl(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);

    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);
    /* Thermal management */
    /* Those registers are fake on 750CL */
    spr_register(env, SPR_THRM1, "THRM1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_THRM2, "THRM2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_THRM3, "THRM3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_TDCL, "TDCL",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_750_TDCH, "TDCH",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* DMA */
    spr_register(env, SPR_750_WPAR, "WPAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_750_DMAL, "DMAL",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_750_DMAU, "DMAU",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Hardware implementation registers */
    spr_register(env, SPR_750CL_HID2, "HID2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750CL_HID4, "HID4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Quantization registers */
    spr_register(env, SPR_750_GQR0, "GQR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR1, "GQR1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR2, "GQR2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR3, "GQR3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR4, "GQR4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR5, "GQR5",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR6, "GQR6",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_750_GQR7, "GQR7",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Memory management */
    register_low_BATs(env);
    /* PowerPC 750cl has 8 DBATs and 8 IBATs */
    register_high_BATs(env);
    init_excp_750cl(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_750cx(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);

    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);
    /* Thermal management */
    register_thrm_sprs(env);

    spr_register(env, SPR_SDA, "SDA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    /* Memory management */
    register_low_BATs(env);
    /* PowerPC 750cx has 8 DBATs and 8 IBATs */
    register_high_BATs(env);
    init_excp_750cx(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_750fx(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);

    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);
    /* Thermal management */
    register_thrm_sprs(env);

    spr_register(env, SPR_750_THRM4, "THRM4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Hardware implementation registers */
    spr_register(env, SPR_750FX_HID2, "HID2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Memory management */
    register_low_BATs(env);
    /* PowerPC 750fx & 750gx has 8 DBATs and 8 IBATs */
    register_high_BATs(env);
    init_excp_7x0(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_750gx(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);

    spr_register(env, SPR_L2CR, "L2CR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, spr_access_nop,
                 0x00000000);
    /* Thermal management */
    register_thrm_sprs(env);

    spr_register(env, SPR_750_THRM4, "THRM4",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Hardware implementation registers */
    spr_register(env, SPR_750FX_HID2, "HID2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Memory management */
    register_low_BATs(env);
    /* PowerPC 750fx & 750gx has 8 DBATs and 8 IBATs */
    register_high_BATs(env);
    init_excp_7x0(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_745(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_7xx_sprs(env);
    register_745_sprs(env);
    /* Thermal management */
    register_thrm_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    register_high_BATs(env);
    register_6xx_7xx_soft_tlb(env, 64, 2);
    init_excp_7x5(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_755(CPUPPCState *env)
{
    init_proc_745(env);
    register_755_sprs(env);
}

POWERPC_FAMILY(740)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 740";
    pcc->init_proc = init_proc_740;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(750)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 750";
    pcc->init_proc = init_proc_750;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(750cl)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 750 CL";
    pcc->init_proc = init_proc_750cl;
    pcc->check_pow = check_pow_hid0;
    /*
     * XXX: not implemented:
     * cache lock instructions:
     * dcbz_l
     * floating point paired instructions
     * psq_lux
     * psq_lx
     * psq_stux
     * psq_stx
     * ps_abs
     * ps_add
     * ps_cmpo0
     * ps_cmpo1
     * ps_cmpu0
     * ps_cmpu1
     * ps_div
     * ps_madd
     * ps_madds0
     * ps_madds1
     * ps_merge00
     * ps_merge01
     * ps_merge10
     * ps_merge11
     * ps_mr
     * ps_msub
     * ps_mul
     * ps_muls0
     * ps_muls1
     * ps_nabs
     * ps_neg
     * ps_nmadd
     * ps_nmsub
     * ps_res
     * ps_rsqrte
     * ps_sel
     * ps_sub
     * ps_sum0
     * ps_sum1
     */
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(750cx)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 750CX";
    pcc->init_proc = init_proc_750cx;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(750fx)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 750FX";
    pcc->init_proc = init_proc_750fx;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(750gx)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 750GX";
    pcc->init_proc = init_proc_750gx;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_32B;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(745)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 745";
    pcc->init_proc = init_proc_745;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(755)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 755";
    pcc->init_proc = init_proc_755;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_PMM) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_7xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_750;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}
