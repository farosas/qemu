/*
 * CPU initialization for PowerPC 6xx CPUs
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

static void register_5xx_8xx_sprs(CPUPPCState *env)
{
    /* Exception processing */
    spr_register_kvm(env, SPR_DSISR, "DSISR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DSISR, 0x00000000);
    spr_register_kvm(env, SPR_DAR, "DAR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DAR, 0x00000000);
    /* Timer */
    spr_register(env, SPR_DECR, "DECR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_decr, &spr_write_decr,
                 0x00000000);

    spr_register(env, SPR_MPC_EIE, "EIE",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_EID, "EID",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_NRI, "NRI",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPA, "CMPA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPB, "CMPB",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPC, "CMPC",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPD, "CMPD",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_ECR, "ECR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_DER, "DER",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_COUNTA, "COUNTA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_COUNTB, "COUNTB",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPE, "CMPE",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPF, "CMPF",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPG, "CMPG",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_CMPH, "CMPH",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_LCTRL1, "LCTRL1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_LCTRL2, "LCTRL2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_BAR, "BAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_DPDR, "DPDR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_IMMR, "IMMR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_5xx_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_RCPU_MI_GRA, "MI_GRA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_GRA, "L2U_GRA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RPCU_BBCMCR, "L2U_BBCMCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_MCR, "L2U_MCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RBA0, "MI_RBA0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RBA1, "MI_RBA1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RBA2, "MI_RBA2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RBA3, "MI_RBA3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RBA0, "L2U_RBA0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RBA1, "L2U_RBA1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RBA2, "L2U_RBA2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RBA3, "L2U_RBA3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RA0, "MI_RA0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RA1, "MI_RA1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RA2, "MI_RA2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_MI_RA3, "MI_RA3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RA0, "L2U_RA0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RA1, "L2U_RA1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RA2, "L2U_RA2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_L2U_RA3, "L2U_RA3",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_RCPU_FPECR, "FPECR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_8xx_sprs(CPUPPCState *env)
{

    spr_register(env, SPR_MPC_IC_CST, "IC_CST",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_IC_ADR, "IC_ADR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_IC_DAT, "IC_DAT",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_DC_CST, "DC_CST",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_DC_ADR, "DC_ADR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_DC_DAT, "DC_DAT",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_CTR, "MI_CTR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_AP, "MI_AP",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_EPN, "MI_EPN",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_TWC, "MI_TWC",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_RPN, "MI_RPN",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_DBCAM, "MI_DBCAM",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_DBRAM0, "MI_DBRAM0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MI_DBRAM1, "MI_DBRAM1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_CTR, "MD_CTR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_CASID, "MD_CASID",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_AP, "MD_AP",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_EPN, "MD_EPN",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_TWB, "MD_TWB",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_TWC, "MD_TWC",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_RPN, "MD_RPN",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_TW, "MD_TW",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_DBCAM, "MD_DBCAM",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_DBRAM0, "MD_DBRAM0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_MPC_MD_DBRAM1, "MD_DBRAM1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_G2_sprs(CPUPPCState *env)
{
    /* Memory base address */
    /* MBAR */
    spr_register(env, SPR_MBAR, "MBAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Exception processing */
    spr_register(env, SPR_BOOKE_CSRR0, "CSRR0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    spr_register(env, SPR_BOOKE_CSRR1, "CSRR1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Breakpoints */
    spr_register(env, SPR_DABR, "DABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_DABR2, "DABR2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_IABR, "IABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_IABR2, "IABR2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_IBCR, "IBCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_DBCR, "DBCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    /* External access control */
    spr_register(env, SPR_EAR, "EAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Hardware implementation register */
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
}

static void register_603_sprs(CPUPPCState *env)
{
    /* External access control */
    spr_register(env, SPR_EAR, "EAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Breakpoints */
    spr_register(env, SPR_IABR, "IABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_HID0, "HID0",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_HID1, "HID1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_e300_sprs(CPUPPCState *env)
{
    /* hardware implementation registers */
    spr_register(env, SPR_HID2, "HID2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
    /* Breakpoints */
    spr_register(env, SPR_DABR, "DABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_DABR2, "DABR2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_IABR2, "IABR2",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_IBCR, "IBCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register(env, SPR_DBCR, "DBCR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void register_604_sprs(CPUPPCState *env)
{
    /* Processor identification */
    spr_register(env, SPR_PIR, "PIR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_pir,
                 0x00000000);
    /* Breakpoints */
    spr_register(env, SPR_IABR, "IABR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);

    spr_register_kvm(env, SPR_DABR, "DABR",
                     SPR_NOACCESS, SPR_NOACCESS,
                     &spr_read_generic, &spr_write_generic,
                     KVM_REG_PPC_DABR, 0x00000000);
    /* Performance counters */
    spr_register(env, SPR_7XX_MMCR0, "MMCR0",
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

    spr_register(env, SPR_7XX_SIAR, "SIAR",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
                 0x00000000);

    spr_register(env, SPR_SDA, "SDA",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, SPR_NOACCESS,
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
}

static void register_604e_sprs(CPUPPCState *env)
{
    spr_register(env, SPR_7XX_MMCR1, "MMCR1",
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
    /* Hardware implementation registers */
    spr_register(env, SPR_HID1, "HID1",
                 SPR_NOACCESS, SPR_NOACCESS,
                 &spr_read_generic, &spr_write_generic,
                 0x00000000);
}

static void init_excp_MPC5xx(CPUPPCState *env)
{
#if !defined(CONFIG_USER_ONLY)
    env->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    env->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    env->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    env->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    env->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    env->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    env->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_FPA]      = 0x00000E00;
    env->excp_vectors[POWERPC_EXCP_EMUL]     = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_DABR]     = 0x00001C00;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001C00;
    env->excp_vectors[POWERPC_EXCP_MEXTBR]   = 0x00001E00;
    env->excp_vectors[POWERPC_EXCP_NMEXTBR]  = 0x00001F00;
    env->ivor_mask = 0x0000FFF0UL;
    env->ivpr_mask = 0xFFFF0000UL;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_MPC8xx(CPUPPCState *env)
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
    env->excp_vectors[POWERPC_EXCP_FPA]      = 0x00000E00;
    env->excp_vectors[POWERPC_EXCP_EMUL]     = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_ITLB]     = 0x00001100;
    env->excp_vectors[POWERPC_EXCP_DTLB]     = 0x00001200;
    env->excp_vectors[POWERPC_EXCP_ITLBE]    = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_DTLBE]    = 0x00001400;
    env->excp_vectors[POWERPC_EXCP_DABR]     = 0x00001C00;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001C00;
    env->excp_vectors[POWERPC_EXCP_MEXTBR]   = 0x00001E00;
    env->excp_vectors[POWERPC_EXCP_NMEXTBR]  = 0x00001F00;
    env->ivor_mask = 0x0000FFF0UL;
    env->ivpr_mask = 0xFFFF0000UL;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_G2(CPUPPCState *env)
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
    env->excp_vectors[POWERPC_EXCP_CRITICAL] = 0x00000A00;
    env->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    env->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    env->excp_vectors[POWERPC_EXCP_IFTLB]    = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_DLTLB]    = 0x00001100;
    env->excp_vectors[POWERPC_EXCP_DSTLB]    = 0x00001200;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_SMI]      = 0x00001400;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_603(CPUPPCState *env)
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
    env->excp_vectors[POWERPC_EXCP_IFTLB]    = 0x00001000;
    env->excp_vectors[POWERPC_EXCP_DLTLB]    = 0x00001100;
    env->excp_vectors[POWERPC_EXCP_DSTLB]    = 0x00001200;
    env->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    env->excp_vectors[POWERPC_EXCP_SMI]      = 0x00001400;
    /* Hardware reset vector */
    env->hreset_vector = 0x00000100UL;
#endif
}

static void init_excp_604(CPUPPCState *env)
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

static void init_proc_MPC5xx(CPUPPCState *env)
{
    register_5xx_8xx_sprs(env);
    register_5xx_sprs(env);
    init_excp_MPC5xx(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* XXX: TODO: allocate internal IRQ controller */
}

static void init_proc_MPC8xx(CPUPPCState *env)
{
    register_5xx_8xx_sprs(env);
    register_8xx_sprs(env);
    init_excp_MPC8xx(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* XXX: TODO: allocate internal IRQ controller */
}

static void init_proc_G2(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_G2_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    register_high_BATs(env);
    register_6xx_7xx_soft_tlb(env, 64, 2);
    init_excp_G2(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_603(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_603_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    register_6xx_7xx_soft_tlb(env, 64, 2);
    init_excp_603(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_e300(CPUPPCState *env)
{
    init_proc_603(env);
    register_e300_sprs(env);
}

static void init_proc_604(CPUPPCState *env)
{
    register_non_embedded_sprs(env);
    register_sdr1_sprs(env);
    register_604_sprs(env);

    /* Memory management */
    register_low_BATs(env);
    init_excp_604(env);
    env->dcache_line_size = 32;
    env->icache_line_size = 32;
    /* Allocate hardware IRQ controller */
    ppc6xx_irq_init(env_archcpu(env));
}

static void init_proc_604E(CPUPPCState *env)
{
    init_proc_604(env);
    register_604e_sprs(env);
}

POWERPC_FAMILY(MPC5xx)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "Freescale 5xx cores (aka RCPU)";
    pcc->init_proc = init_proc_MPC5xx;
    pcc->check_pow = check_pow_none;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING |
                       PPC_MEM_EIEIO | PPC_MEM_SYNC |
                       PPC_CACHE_ICBI | PPC_FLOAT | PPC_FLOAT_STFIWX |
                       PPC_MFTB;
    pcc->msr_mask = (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_REAL;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_RCPU;
    pcc->bfd_mach = bfd_mach_ppc_505;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(MPC8xx)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "Freescale 8xx cores (aka PowerQUICC)";
    pcc->init_proc = init_proc_MPC8xx;
    pcc->check_pow = check_pow_none;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING  |
                       PPC_MEM_EIEIO | PPC_MEM_SYNC |
                       PPC_CACHE_ICBI | PPC_MFTB;
    pcc->msr_mask = (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_MPC8xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_RCPU;
    pcc->bfd_mach = bfd_mach_ppc_860;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(G2)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC G2";
    pcc->init_proc = init_proc_G2;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_TGPR) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_AL) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_RI);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_ec603e;
    pcc->flags = POWERPC_FLAG_TGPR | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(G2LE)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC G2LE";
    pcc->init_proc = init_proc_G2;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_TGPR) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_AL) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_ec603e;
    pcc->flags = POWERPC_FLAG_TGPR | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(603)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 603";
    pcc->init_proc = init_proc_603;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_TGPR) |
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
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_603;
    pcc->flags = POWERPC_FLAG_TGPR | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(603E)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 603e";
    pcc->init_proc = init_proc_603;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_FRSQRTE | PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_TGPR) |
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
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_ec603e;
    pcc->flags = POWERPC_FLAG_TGPR | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(e300)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "e300 core";
    pcc->init_proc = init_proc_e300;
    pcc->check_pow = check_pow_hid0;
    pcc->insns_flags = PPC_INSNS_BASE | PPC_STRING | PPC_MFTB |
                       PPC_FLOAT | PPC_FLOAT_FSEL | PPC_FLOAT_FRES |
                       PPC_FLOAT_STFIWX |
                       PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ |
                       PPC_MEM_SYNC | PPC_MEM_EIEIO |
                       PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_6xx_TLB |
                       PPC_SEGMENT | PPC_EXTERN;
    pcc->msr_mask = (1ull << MSR_POW) |
                    (1ull << MSR_TGPR) |
                    (1ull << MSR_ILE) |
                    (1ull << MSR_EE) |
                    (1ull << MSR_PR) |
                    (1ull << MSR_FP) |
                    (1ull << MSR_ME) |
                    (1ull << MSR_FE0) |
                    (1ull << MSR_SE) |
                    (1ull << MSR_DE) |
                    (1ull << MSR_FE1) |
                    (1ull << MSR_AL) |
                    (1ull << MSR_EP) |
                    (1ull << MSR_IR) |
                    (1ull << MSR_DR) |
                    (1ull << MSR_RI) |
                    (1ull << MSR_LE);
    pcc->mmu_model = POWERPC_MMU_SOFT_6xx;
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_603;
    pcc->flags = POWERPC_FLAG_TGPR | POWERPC_FLAG_SE |
                 POWERPC_FLAG_BE | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(604)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 604";
    pcc->init_proc = init_proc_604;
    pcc->check_pow = check_pow_nocheck;
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
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_604;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}

POWERPC_FAMILY(604E)(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);

    dc->desc = "PowerPC 604E";
    pcc->init_proc = init_proc_604E;
    pcc->check_pow = check_pow_nocheck;
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
    pcc->excp_model = POWERPC_EXCP_6xx;
    pcc->bus_model = PPC_FLAGS_INPUT_6xx;
    pcc->bfd_mach = bfd_mach_ppc_604;
    pcc->flags = POWERPC_FLAG_SE | POWERPC_FLAG_BE |
                 POWERPC_FLAG_PMM | POWERPC_FLAG_BUS_CLK;
}
