/*
 *  PowerPC CPU initialization for qemu.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
 *  Copyright 2011 Freescale Semiconductor, Inc.
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
#include "disas/dis-asm.h"
#include "exec/gdbstub.h"
#include "kvm_ppc.h"
#include "sysemu/cpus.h"
#include "sysemu/hw_accel.h"
#include "sysemu/tcg.h"
#include "cpu-models.h"
#include "mmu-hash32.h"
#include "mmu-hash64.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/qemu-print.h"
#include "qapi/error.h"
#include "qapi/qmp/qnull.h"
#include "qapi/visitor.h"
#include "hw/qdev-properties.h"
#include "hw/ppc/ppc.h"
#include "qemu/cutils.h"
#include "disas/capstone.h"
#include "fpu/softfloat.h"
#include "qapi/qapi-commands-machine-target.h"

#include "helper_regs.h"
#include "internal.h"
#include "spr_common.h"
#include "power8-pmu.h"

#if !defined(CONFIG_USER_ONLY)
void cpu_ppc_set_vhyp(PowerPCCPU *cpu, PPCVirtualHypervisor *vhyp)
{
    CPUPPCState *env = &cpu->env;

    cpu->vhyp = vhyp;

    /*
     * With a virtual hypervisor mode we never allow the CPU to go
     * hypervisor mode itself
     */
    env->msr_mask &= ~MSR_HVB;
}
#endif /* !defined(CONFIG_USER_ONLY) */

/*****************************************************************************/
/* Generic CPU instantiation routine                                         */
static void init_ppc_proc(PowerPCCPU *cpu)
{
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    CPUPPCState *env = &cpu->env;
#if !defined(CONFIG_USER_ONLY)
    int i;

    env->irq_inputs = NULL;
    /* Set all exception vectors to an invalid address */
    for (i = 0; i < POWERPC_EXCP_NB; i++) {
        env->excp_vectors[i] = (target_ulong)(-1ULL);
    }
    env->ivor_mask = 0x00000000;
    env->ivpr_mask = 0x00000000;
    /* Default MMU definitions */
    env->nb_BATs = 0;
    env->nb_tlb = 0;
    env->nb_ways = 0;
    env->tlb_type = TLB_NONE;
#endif
    /* Register SPR common to all PowerPC implementations */
    register_generic_sprs(cpu);

    /* PowerPC implementation specific initialisations (SPRs, timers, ...) */
    (*pcc->init_proc)(env);

#if !defined(CONFIG_USER_ONLY)
    ppc_gdb_gen_spr_xml(cpu);
#endif

    /* MSR bits & flags consistency checks */
    if (env->msr_mask & (1 << 25)) {
        switch (env->flags & (POWERPC_FLAG_SPE | POWERPC_FLAG_VRE)) {
        case POWERPC_FLAG_SPE:
        case POWERPC_FLAG_VRE:
            break;
        default:
            fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                    "Should define POWERPC_FLAG_SPE or POWERPC_FLAG_VRE\n");
            exit(1);
        }
    } else if (env->flags & (POWERPC_FLAG_SPE | POWERPC_FLAG_VRE)) {
        fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                "Should not define POWERPC_FLAG_SPE nor POWERPC_FLAG_VRE\n");
        exit(1);
    }
    if (env->msr_mask & (1 << 17)) {
        switch (env->flags & (POWERPC_FLAG_TGPR | POWERPC_FLAG_CE)) {
        case POWERPC_FLAG_TGPR:
        case POWERPC_FLAG_CE:
            break;
        default:
            fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                    "Should define POWERPC_FLAG_TGPR or POWERPC_FLAG_CE\n");
            exit(1);
        }
    } else if (env->flags & (POWERPC_FLAG_TGPR | POWERPC_FLAG_CE)) {
        fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                "Should not define POWERPC_FLAG_TGPR nor POWERPC_FLAG_CE\n");
        exit(1);
    }
    if (env->msr_mask & (1 << 10)) {
        switch (env->flags & (POWERPC_FLAG_SE | POWERPC_FLAG_DWE |
                              POWERPC_FLAG_UBLE)) {
        case POWERPC_FLAG_SE:
        case POWERPC_FLAG_DWE:
        case POWERPC_FLAG_UBLE:
            break;
        default:
            fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                    "Should define POWERPC_FLAG_SE or POWERPC_FLAG_DWE or "
                    "POWERPC_FLAG_UBLE\n");
            exit(1);
        }
    } else if (env->flags & (POWERPC_FLAG_SE | POWERPC_FLAG_DWE |
                             POWERPC_FLAG_UBLE)) {
        fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                "Should not define POWERPC_FLAG_SE nor POWERPC_FLAG_DWE nor "
                "POWERPC_FLAG_UBLE\n");
            exit(1);
    }
    if (env->msr_mask & (1 << 9)) {
        switch (env->flags & (POWERPC_FLAG_BE | POWERPC_FLAG_DE)) {
        case POWERPC_FLAG_BE:
        case POWERPC_FLAG_DE:
            break;
        default:
            fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                    "Should define POWERPC_FLAG_BE or POWERPC_FLAG_DE\n");
            exit(1);
        }
    } else if (env->flags & (POWERPC_FLAG_BE | POWERPC_FLAG_DE)) {
        fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                "Should not define POWERPC_FLAG_BE nor POWERPC_FLAG_DE\n");
        exit(1);
    }
    if (env->msr_mask & (1 << 2)) {
        switch (env->flags & (POWERPC_FLAG_PX | POWERPC_FLAG_PMM)) {
        case POWERPC_FLAG_PX:
        case POWERPC_FLAG_PMM:
            break;
        default:
            fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                    "Should define POWERPC_FLAG_PX or POWERPC_FLAG_PMM\n");
            exit(1);
        }
    } else if (env->flags & (POWERPC_FLAG_PX | POWERPC_FLAG_PMM)) {
        fprintf(stderr, "PowerPC MSR definition inconsistency\n"
                "Should not define POWERPC_FLAG_PX nor POWERPC_FLAG_PMM\n");
        exit(1);
    }
    if ((env->flags & POWERPC_FLAG_BUS_CLK) == 0) {
        fprintf(stderr, "PowerPC flags inconsistency\n"
                "Should define the time-base and decrementer clock source\n");
        exit(1);
    }
    /* Allocate TLBs buffer when needed */
#if !defined(CONFIG_USER_ONLY)
    if (env->nb_tlb != 0) {
        int nb_tlb = env->nb_tlb;
        if (env->id_tlbs != 0) {
            nb_tlb *= 2;
        }
        switch (env->tlb_type) {
        case TLB_6XX:
            env->tlb.tlb6 = g_new0(ppc6xx_tlb_t, nb_tlb);
            break;
        case TLB_EMB:
            env->tlb.tlbe = g_new0(ppcemb_tlb_t, nb_tlb);
            break;
        case TLB_MAS:
            env->tlb.tlbm = g_new0(ppcmas_tlb_t, nb_tlb);
            break;
        }
        /* Pre-compute some useful values */
        env->tlb_per_way = env->nb_tlb / env->nb_ways;
    }
    if (env->irq_inputs == NULL) {
        warn_report("no internal IRQ controller registered."
                    " Attempt QEMU to crash very soon !");
    }
#endif
    if (env->check_pow == NULL) {
        warn_report("no power management check handler registered."
                    " Attempt QEMU to crash very soon !");
    }
}


static void ppc_cpu_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    PowerPCCPU *cpu = POWERPC_CPU(dev);
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    if (cpu->vcpu_id == UNASSIGNED_CPU_INDEX) {
        cpu->vcpu_id = cs->cpu_index;
    }

    if (tcg_enabled()) {
        if (ppc_fixup_cpu(cpu) != 0) {
            error_setg(errp, "Unable to emulate selected CPU with TCG");
            goto unrealize;
        }
    }

    create_ppc_opcodes(cpu, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        goto unrealize;
    }
    init_ppc_proc(cpu);

    ppc_gdb_init(cs, pcc);
    qemu_init_vcpu(cs);

    pcc->parent_realize(dev, errp);

    return;

unrealize:
    cpu_exec_unrealizefn(cs);
}

static void ppc_cpu_unrealize(DeviceState *dev)
{
    PowerPCCPU *cpu = POWERPC_CPU(dev);
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);

    pcc->parent_unrealize(dev);

    cpu_remove_sync(CPU(cpu));

    destroy_ppc_opcodes(cpu);
}

static gint ppc_cpu_compare_class_pvr(gconstpointer a, gconstpointer b)
{
    ObjectClass *oc = (ObjectClass *)a;
    uint32_t pvr = *(uint32_t *)b;
    PowerPCCPUClass *pcc = (PowerPCCPUClass *)a;

    /* -cpu host does a PVR lookup during construction */
    if (unlikely(strcmp(object_class_get_name(oc),
                        TYPE_HOST_POWERPC_CPU) == 0)) {
        return -1;
    }

    return pcc->pvr == pvr ? 0 : -1;
}

PowerPCCPUClass *ppc_cpu_class_by_pvr(uint32_t pvr)
{
    GSList *list, *item;
    PowerPCCPUClass *pcc = NULL;

    list = object_class_get_list(TYPE_POWERPC_CPU, false);
    item = g_slist_find_custom(list, &pvr, ppc_cpu_compare_class_pvr);
    if (item != NULL) {
        pcc = POWERPC_CPU_CLASS(item->data);
    }
    g_slist_free(list);

    return pcc;
}

static gint ppc_cpu_compare_class_pvr_mask(gconstpointer a, gconstpointer b)
{
    ObjectClass *oc = (ObjectClass *)a;
    uint32_t pvr = *(uint32_t *)b;
    PowerPCCPUClass *pcc = (PowerPCCPUClass *)a;

    /* -cpu host does a PVR lookup during construction */
    if (unlikely(strcmp(object_class_get_name(oc),
                        TYPE_HOST_POWERPC_CPU) == 0)) {
        return -1;
    }

    if (pcc->pvr_match(pcc, pvr)) {
        return 0;
    }

    return -1;
}

PowerPCCPUClass *ppc_cpu_class_by_pvr_mask(uint32_t pvr)
{
    GSList *list, *item;
    PowerPCCPUClass *pcc = NULL;

    list = object_class_get_list(TYPE_POWERPC_CPU, true);
    item = g_slist_find_custom(list, &pvr, ppc_cpu_compare_class_pvr_mask);
    if (item != NULL) {
        pcc = POWERPC_CPU_CLASS(item->data);
    }
    g_slist_free(list);

    return pcc;
}

static const char *ppc_cpu_lookup_alias(const char *alias)
{
    int ai;

    for (ai = 0; ppc_cpu_aliases[ai].alias != NULL; ai++) {
        if (strcmp(ppc_cpu_aliases[ai].alias, alias) == 0) {
            return ppc_cpu_aliases[ai].model;
        }
    }

    return NULL;
}

static ObjectClass *ppc_cpu_class_by_name(const char *name)
{
    char *cpu_model, *typename;
    ObjectClass *oc;
    const char *p;
    unsigned long pvr;

    /*
     * Lookup by PVR if cpu_model is valid 8 digit hex number (excl:
     * 0x prefix if present)
     */
    if (!qemu_strtoul(name, &p, 16, &pvr)) {
        int len = p - name;
        len = (len == 10) && (name[1] == 'x') ? len - 2 : len;
        if ((len == 8) && (*p == '\0')) {
            return OBJECT_CLASS(ppc_cpu_class_by_pvr(pvr));
        }
    }

    cpu_model = g_ascii_strdown(name, -1);
    p = ppc_cpu_lookup_alias(cpu_model);
    if (p) {
        g_free(cpu_model);
        cpu_model = g_strdup(p);
    }

    typename = g_strdup_printf("%s" POWERPC_CPU_TYPE_SUFFIX, cpu_model);
    oc = object_class_by_name(typename);
    g_free(typename);
    g_free(cpu_model);

    return oc;
}

PowerPCCPUClass *ppc_cpu_get_family_class(PowerPCCPUClass *pcc)
{
    ObjectClass *oc = OBJECT_CLASS(pcc);

    while (oc && !object_class_is_abstract(oc)) {
        oc = object_class_get_parent(oc);
    }
    assert(oc);

    return POWERPC_CPU_CLASS(oc);
}

/* Sort by PVR, ordering special case "host" last. */
static gint ppc_cpu_list_compare(gconstpointer a, gconstpointer b)
{
    ObjectClass *oc_a = (ObjectClass *)a;
    ObjectClass *oc_b = (ObjectClass *)b;
    PowerPCCPUClass *pcc_a = POWERPC_CPU_CLASS(oc_a);
    PowerPCCPUClass *pcc_b = POWERPC_CPU_CLASS(oc_b);
    const char *name_a = object_class_get_name(oc_a);
    const char *name_b = object_class_get_name(oc_b);

    if (strcmp(name_a, TYPE_HOST_POWERPC_CPU) == 0) {
        return 1;
    } else if (strcmp(name_b, TYPE_HOST_POWERPC_CPU) == 0) {
        return -1;
    } else {
        /* Avoid an integer overflow during subtraction */
        if (pcc_a->pvr < pcc_b->pvr) {
            return -1;
        } else if (pcc_a->pvr > pcc_b->pvr) {
            return 1;
        } else {
            return 0;
        }
    }
}

static void ppc_cpu_list_entry(gpointer data, gpointer user_data)
{
    ObjectClass *oc = data;
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    DeviceClass *family = DEVICE_CLASS(ppc_cpu_get_family_class(pcc));
    const char *typename = object_class_get_name(oc);
    char *name;
    int i;

    if (unlikely(strcmp(typename, TYPE_HOST_POWERPC_CPU) == 0)) {
        return;
    }

    name = g_strndup(typename,
                     strlen(typename) - strlen(POWERPC_CPU_TYPE_SUFFIX));
    qemu_printf("PowerPC %-16s PVR %08x\n", name, pcc->pvr);
    for (i = 0; ppc_cpu_aliases[i].alias != NULL; i++) {
        PowerPCCPUAlias *alias = &ppc_cpu_aliases[i];
        ObjectClass *alias_oc = ppc_cpu_class_by_name(alias->model);

        if (alias_oc != oc) {
            continue;
        }
        /*
         * If running with KVM, we might update the family alias later, so
         * avoid printing the wrong alias here and use "preferred" instead
         */
        if (strcmp(alias->alias, family->desc) == 0) {
            qemu_printf("PowerPC %-16s (alias for preferred %s CPU)\n",
                        alias->alias, family->desc);
        } else {
            qemu_printf("PowerPC %-16s (alias for %s)\n",
                        alias->alias, name);
        }
    }
    g_free(name);
}

void ppc_cpu_list(void)
{
    GSList *list;

    list = object_class_get_list(TYPE_POWERPC_CPU, false);
    list = g_slist_sort(list, ppc_cpu_list_compare);
    g_slist_foreach(list, ppc_cpu_list_entry, NULL);
    g_slist_free(list);

#ifdef CONFIG_KVM
    qemu_printf("\n");
    qemu_printf("PowerPC %s\n", "host");
#endif
}

static void ppc_cpu_defs_entry(gpointer data, gpointer user_data)
{
    ObjectClass *oc = data;
    CpuDefinitionInfoList **first = user_data;
    const char *typename;
    CpuDefinitionInfo *info;

    typename = object_class_get_name(oc);
    info = g_malloc0(sizeof(*info));
    info->name = g_strndup(typename,
                           strlen(typename) - strlen(POWERPC_CPU_TYPE_SUFFIX));

    QAPI_LIST_PREPEND(*first, info);
}

CpuDefinitionInfoList *qmp_query_cpu_definitions(Error **errp)
{
    CpuDefinitionInfoList *cpu_list = NULL;
    GSList *list;
    int i;

    list = object_class_get_list(TYPE_POWERPC_CPU, false);
    g_slist_foreach(list, ppc_cpu_defs_entry, &cpu_list);
    g_slist_free(list);

    for (i = 0; ppc_cpu_aliases[i].alias != NULL; i++) {
        PowerPCCPUAlias *alias = &ppc_cpu_aliases[i];
        ObjectClass *oc;
        CpuDefinitionInfo *info;

        oc = ppc_cpu_class_by_name(alias->model);
        if (oc == NULL) {
            continue;
        }

        info = g_malloc0(sizeof(*info));
        info->name = g_strdup(alias->alias);
        info->q_typename = g_strdup(object_class_get_name(oc));

        QAPI_LIST_PREPEND(cpu_list, info);
    }

    return cpu_list;
}

static void ppc_cpu_set_pc(CPUState *cs, vaddr value)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);

    cpu->env.nip = value;
}

static bool ppc_cpu_has_work(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    return msr_ee && (cs->interrupt_request & CPU_INTERRUPT_HARD);
}

static void ppc_cpu_reset(DeviceState *dev)
{
    CPUState *s = CPU(dev);
    PowerPCCPU *cpu = POWERPC_CPU(s);
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    CPUPPCState *env = &cpu->env;
    target_ulong msr;
    int i;

    pcc->parent_reset(dev);

    msr = (target_ulong)0;
    msr |= (target_ulong)MSR_HVB;
    msr |= (target_ulong)1 << MSR_EP;
#if defined(DO_SINGLE_STEP) && 0
    /* Single step trace mode */
    msr |= (target_ulong)1 << MSR_SE;
    msr |= (target_ulong)1 << MSR_BE;
#endif
#if defined(CONFIG_USER_ONLY)
    msr |= (target_ulong)1 << MSR_FP; /* Allow floating point usage */
    msr |= (target_ulong)1 << MSR_FE0; /* Allow floating point exceptions */
    msr |= (target_ulong)1 << MSR_FE1;
    msr |= (target_ulong)1 << MSR_VR; /* Allow altivec usage */
    msr |= (target_ulong)1 << MSR_VSX; /* Allow VSX usage */
    msr |= (target_ulong)1 << MSR_SPE; /* Allow SPE usage */
    msr |= (target_ulong)1 << MSR_PR;
#if defined(TARGET_PPC64)
    msr |= (target_ulong)1 << MSR_TM; /* Transactional memory */
#endif
#if !defined(TARGET_WORDS_BIGENDIAN)
    msr |= (target_ulong)1 << MSR_LE; /* Little-endian user mode */
    if (!((env->msr_mask >> MSR_LE) & 1)) {
        fprintf(stderr, "Selected CPU does not support little-endian.\n");
        exit(1);
    }
#endif
#endif

#if defined(TARGET_PPC64)
    if (mmu_is_64bit(env->mmu_model)) {
        msr |= (1ULL << MSR_SF);
    }
#endif

    hreg_store_msr(env, msr, 1);

#if !defined(CONFIG_USER_ONLY)
    env->nip = env->hreset_vector | env->excp_prefix;

    if (tcg_enabled()) {
        if (env->mmu_model != POWERPC_MMU_REAL) {
            ppc_tlb_invalidate_all(env);
        }
        pmu_update_summaries(env);
    }
#endif
    hreg_compute_hflags(env);
    env->reserve_addr = (target_ulong)-1ULL;
    /* Be sure no exception or interrupt is pending */
    env->pending_interrupts = 0;
    s->exception_index = POWERPC_EXCP_NONE;
    env->error_code = 0;
    ppc_irq_reset(cpu);

    /* tininess for underflow is detected before rounding */
    set_float_detect_tininess(float_tininess_before_rounding,
                              &env->fp_status);

    for (i = 0; i < ARRAY_SIZE(env->spr_cb); i++) {
        ppc_spr_t *spr = &env->spr_cb[i];

        if (!spr->name) {
            continue;
        }
        env->spr[i] = spr->default_value;
    }
}

#ifndef CONFIG_USER_ONLY

static bool ppc_cpu_is_big_endian(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    cpu_synchronize_state(cs);

    return !msr_le;
}

#ifdef CONFIG_TCG
static void ppc_cpu_exec_enter(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);

    if (cpu->vhyp) {
        PPCVirtualHypervisorClass *vhc =
            PPC_VIRTUAL_HYPERVISOR_GET_CLASS(cpu->vhyp);
        vhc->cpu_exec_enter(cpu->vhyp, cpu);
    }
}

static void ppc_cpu_exec_exit(CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);

    if (cpu->vhyp) {
        PPCVirtualHypervisorClass *vhc =
            PPC_VIRTUAL_HYPERVISOR_GET_CLASS(cpu->vhyp);
        vhc->cpu_exec_exit(cpu->vhyp, cpu);
    }
}
#endif /* CONFIG_TCG */

#endif /* !CONFIG_USER_ONLY */

static void ppc_cpu_instance_init(Object *obj)
{
    PowerPCCPU *cpu = POWERPC_CPU(obj);
    PowerPCCPUClass *pcc = POWERPC_CPU_GET_CLASS(cpu);
    CPUPPCState *env = &cpu->env;

    cpu_set_cpustate_pointers(cpu);
    cpu->vcpu_id = UNASSIGNED_CPU_INDEX;

    env->msr_mask = pcc->msr_mask;
    env->mmu_model = pcc->mmu_model;
    env->excp_model = pcc->excp_model;
    env->bus_model = pcc->bus_model;
    env->insns_flags = pcc->insns_flags;
    env->insns_flags2 = pcc->insns_flags2;
    env->flags = pcc->flags;
    env->bfd_mach = pcc->bfd_mach;
    env->check_pow = pcc->check_pow;

    /*
     * Mark HV mode as supported if the CPU has an MSR_HV bit in the
     * msr_mask. The mask can later be cleared by PAPR mode but the hv
     * mode support will remain, thus enforcing that we cannot use
     * priv. instructions in guest in PAPR mode. For 970 we currently
     * simply don't set HV in msr_mask thus simulating an "Apple mode"
     * 970. If we ever want to support 970 HV mode, we'll have to add
     * a processor attribute of some sort.
     */
#if !defined(CONFIG_USER_ONLY)
    env->has_hv_mode = !!(env->msr_mask & MSR_HVB);
#endif

    ppc_hash64_init(cpu);
}

static void ppc_cpu_instance_finalize(Object *obj)
{
    PowerPCCPU *cpu = POWERPC_CPU(obj);

    ppc_hash64_finalize(cpu);
}

static bool ppc_pvr_match_default(PowerPCCPUClass *pcc, uint32_t pvr)
{
    return pcc->pvr == pvr;
}

static void ppc_disas_set_info(CPUState *cs, disassemble_info *info)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;

    if ((env->hflags >> MSR_LE) & 1) {
        info->endian = BFD_ENDIAN_LITTLE;
    }
    info->mach = env->bfd_mach;
    if (!env->bfd_mach) {
#ifdef TARGET_PPC64
        info->mach = bfd_mach_ppc64;
#else
        info->mach = bfd_mach_ppc;
#endif
    }
    info->disassembler_options = (char *)"any";
    info->print_insn = print_insn_ppc;

    info->cap_arch = CS_ARCH_PPC;
#ifdef TARGET_PPC64
    info->cap_mode = CS_MODE_64;
#endif
}

static Property ppc_cpu_properties[] = {
    DEFINE_PROP_BOOL("pre-2.8-migration", PowerPCCPU, pre_2_8_migration, false),
    DEFINE_PROP_BOOL("pre-2.10-migration", PowerPCCPU, pre_2_10_migration,
                     false),
    DEFINE_PROP_BOOL("pre-3.0-migration", PowerPCCPU, pre_3_0_migration,
                     false),
    DEFINE_PROP_END_OF_LIST(),
};

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps ppc_sysemu_ops = {
    .get_phys_page_debug = ppc_cpu_get_phys_page_debug,
    .write_elf32_note = ppc32_cpu_write_elf32_note,
    .write_elf64_note = ppc64_cpu_write_elf64_note,
    .virtio_is_big_endian = ppc_cpu_is_big_endian,
    .legacy_vmsd = &vmstate_ppc_cpu,
};
#endif

#ifdef CONFIG_TCG
#include "hw/core/tcg-cpu-ops.h"

static const struct TCGCPUOps ppc_tcg_ops = {
  .initialize = ppc_translate_init,

#ifdef CONFIG_USER_ONLY
  .record_sigsegv = ppc_cpu_record_sigsegv,
#else
  .tlb_fill = ppc_cpu_tlb_fill,
  .cpu_exec_interrupt = ppc_cpu_exec_interrupt,
  .do_interrupt = ppc_cpu_do_interrupt,
  .cpu_exec_enter = ppc_cpu_exec_enter,
  .cpu_exec_exit = ppc_cpu_exec_exit,
  .do_unaligned_access = ppc_cpu_do_unaligned_access,
#endif /* !CONFIG_USER_ONLY */
};
#endif /* CONFIG_TCG */

static void ppc_cpu_class_init(ObjectClass *oc, void *data)
{
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_parent_realize(dc, ppc_cpu_realize,
                                    &pcc->parent_realize);
    device_class_set_parent_unrealize(dc, ppc_cpu_unrealize,
                                      &pcc->parent_unrealize);
    pcc->pvr_match = ppc_pvr_match_default;
    device_class_set_props(dc, ppc_cpu_properties);

    device_class_set_parent_reset(dc, ppc_cpu_reset, &pcc->parent_reset);

    cc->class_by_name = ppc_cpu_class_by_name;
    cc->has_work = ppc_cpu_has_work;
    cc->dump_state = ppc_cpu_dump_state;
    cc->set_pc = ppc_cpu_set_pc;
    cc->gdb_read_register = ppc_cpu_gdb_read_register;
    cc->gdb_write_register = ppc_cpu_gdb_write_register;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &ppc_sysemu_ops;
#endif

    cc->gdb_num_core_regs = 71;
#ifndef CONFIG_USER_ONLY
    cc->gdb_get_dynamic_xml = ppc_gdb_get_dynamic_xml;
#endif
#ifdef USE_APPLE_GDB
    cc->gdb_read_register = ppc_cpu_gdb_read_register_apple;
    cc->gdb_write_register = ppc_cpu_gdb_write_register_apple;
    cc->gdb_num_core_regs = 71 + 32;
#endif

    cc->gdb_arch_name = ppc_gdb_arch_name;
#if defined(TARGET_PPC64)
    cc->gdb_core_xml_file = "power64-core.xml";
#else
    cc->gdb_core_xml_file = "power-core.xml";
#endif
    cc->disas_set_info = ppc_disas_set_info;

    dc->fw_name = "PowerPC,UNKNOWN";

#ifdef CONFIG_TCG
    cc->tcg_ops = &ppc_tcg_ops;
#endif /* CONFIG_TCG */
}

static const TypeInfo ppc_cpu_type_info = {
    .name = TYPE_POWERPC_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(PowerPCCPU),
    .instance_align = __alignof__(PowerPCCPU),
    .instance_init = ppc_cpu_instance_init,
    .instance_finalize = ppc_cpu_instance_finalize,
    .abstract = true,
    .class_size = sizeof(PowerPCCPUClass),
    .class_init = ppc_cpu_class_init,
};

#ifndef CONFIG_USER_ONLY
static const TypeInfo ppc_vhyp_type_info = {
    .name = TYPE_PPC_VIRTUAL_HYPERVISOR,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(PPCVirtualHypervisorClass),
};
#endif

static void ppc_cpu_register_types(void)
{
    type_register_static(&ppc_cpu_type_info);
#ifndef CONFIG_USER_ONLY
    type_register_static(&ppc_vhyp_type_info);
#endif
}

void ppc_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
#define RGPL  4
#define RFPL  4

    PowerPCCPU *cpu = POWERPC_CPU(cs);
    CPUPPCState *env = &cpu->env;
    int i;

    qemu_fprintf(f, "NIP " TARGET_FMT_lx "   LR " TARGET_FMT_lx " CTR "
                 TARGET_FMT_lx " XER " TARGET_FMT_lx " CPU#%d\n",
                 env->nip, env->lr, env->ctr, cpu_read_xer(env),
                 cs->cpu_index);
    qemu_fprintf(f, "MSR " TARGET_FMT_lx " HID0 " TARGET_FMT_lx "  HF "
                 "%08x iidx %d didx %d\n",
                 env->msr, env->spr[SPR_HID0], env->hflags,
                 cpu_mmu_index(env, true), cpu_mmu_index(env, false));
#if !defined(NO_TIMER_DUMP)
    qemu_fprintf(f, "TB %08" PRIu32 " %08" PRIu64
#if !defined(CONFIG_USER_ONLY)
                 " DECR " TARGET_FMT_lu
#endif
                 "\n",
                 cpu_ppc_load_tbu(env), cpu_ppc_load_tbl(env)
#if !defined(CONFIG_USER_ONLY)
                 , cpu_ppc_load_decr(env)
#endif
        );
#endif
    for (i = 0; i < 32; i++) {
        if ((i & (RGPL - 1)) == 0) {
            qemu_fprintf(f, "GPR%02d", i);
        }
        qemu_fprintf(f, " %016" PRIx64, ppc_dump_gpr(env, i));
        if ((i & (RGPL - 1)) == (RGPL - 1)) {
            qemu_fprintf(f, "\n");
        }
    }
    qemu_fprintf(f, "CR ");
    for (i = 0; i < 8; i++)
        qemu_fprintf(f, "%01x", env->crf[i]);
    qemu_fprintf(f, "  [");
    for (i = 0; i < 8; i++) {
        char a = '-';
        if (env->crf[i] & 0x08) {
            a = 'L';
        } else if (env->crf[i] & 0x04) {
            a = 'G';
        } else if (env->crf[i] & 0x02) {
            a = 'E';
        }
        qemu_fprintf(f, " %c%c", a, env->crf[i] & 0x01 ? 'O' : ' ');
    }
    qemu_fprintf(f, " ]             RES " TARGET_FMT_lx "\n",
                 env->reserve_addr);

    if (flags & CPU_DUMP_FPU) {
        for (i = 0; i < 32; i++) {
            if ((i & (RFPL - 1)) == 0) {
                qemu_fprintf(f, "FPR%02d", i);
            }
            qemu_fprintf(f, " %016" PRIx64, *cpu_fpr_ptr(env, i));
            if ((i & (RFPL - 1)) == (RFPL - 1)) {
                qemu_fprintf(f, "\n");
            }
        }
        qemu_fprintf(f, "FPSCR " TARGET_FMT_lx "\n", env->fpscr);
    }

#if !defined(CONFIG_USER_ONLY)
    qemu_fprintf(f, " SRR0 " TARGET_FMT_lx "  SRR1 " TARGET_FMT_lx
                 "    PVR " TARGET_FMT_lx " VRSAVE " TARGET_FMT_lx "\n",
                 env->spr[SPR_SRR0], env->spr[SPR_SRR1],
                 env->spr[SPR_PVR], env->spr[SPR_VRSAVE]);

    qemu_fprintf(f, "SPRG0 " TARGET_FMT_lx " SPRG1 " TARGET_FMT_lx
                 "  SPRG2 " TARGET_FMT_lx "  SPRG3 " TARGET_FMT_lx "\n",
                 env->spr[SPR_SPRG0], env->spr[SPR_SPRG1],
                 env->spr[SPR_SPRG2], env->spr[SPR_SPRG3]);

    qemu_fprintf(f, "SPRG4 " TARGET_FMT_lx " SPRG5 " TARGET_FMT_lx
                 "  SPRG6 " TARGET_FMT_lx "  SPRG7 " TARGET_FMT_lx "\n",
                 env->spr[SPR_SPRG4], env->spr[SPR_SPRG5],
                 env->spr[SPR_SPRG6], env->spr[SPR_SPRG7]);

    switch (env->excp_model) {
#if defined(TARGET_PPC64)
    case POWERPC_EXCP_POWER7:
    case POWERPC_EXCP_POWER8:
    case POWERPC_EXCP_POWER9:
    case POWERPC_EXCP_POWER10:
        qemu_fprintf(f, "HSRR0 " TARGET_FMT_lx " HSRR1 " TARGET_FMT_lx "\n",
                     env->spr[SPR_HSRR0], env->spr[SPR_HSRR1]);
        break;
#endif
    case POWERPC_EXCP_BOOKE:
        qemu_fprintf(f, "CSRR0 " TARGET_FMT_lx " CSRR1 " TARGET_FMT_lx
                     " MCSRR0 " TARGET_FMT_lx " MCSRR1 " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_CSRR0], env->spr[SPR_BOOKE_CSRR1],
                     env->spr[SPR_BOOKE_MCSRR0], env->spr[SPR_BOOKE_MCSRR1]);

        qemu_fprintf(f, "  TCR " TARGET_FMT_lx "   TSR " TARGET_FMT_lx
                     "    ESR " TARGET_FMT_lx "   DEAR " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_TCR], env->spr[SPR_BOOKE_TSR],
                     env->spr[SPR_BOOKE_ESR], env->spr[SPR_BOOKE_DEAR]);

        qemu_fprintf(f, "  PIR " TARGET_FMT_lx " DECAR " TARGET_FMT_lx
                     "   IVPR " TARGET_FMT_lx "   EPCR " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_PIR], env->spr[SPR_BOOKE_DECAR],
                     env->spr[SPR_BOOKE_IVPR], env->spr[SPR_BOOKE_EPCR]);

        qemu_fprintf(f, " MCSR " TARGET_FMT_lx " SPRG8 " TARGET_FMT_lx
                     "    EPR " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_MCSR], env->spr[SPR_BOOKE_SPRG8],
                     env->spr[SPR_BOOKE_EPR]);

        /* FSL-specific */
        qemu_fprintf(f, " MCAR " TARGET_FMT_lx "  PID1 " TARGET_FMT_lx
                     "   PID2 " TARGET_FMT_lx "    SVR " TARGET_FMT_lx "\n",
                     env->spr[SPR_Exxx_MCAR], env->spr[SPR_BOOKE_PID1],
                     env->spr[SPR_BOOKE_PID2], env->spr[SPR_E500_SVR]);

        /*
         * IVORs are left out as they are large and do not change often --
         * they can be read with "p $ivor0", "p $ivor1", etc.
         */
        break;
    case POWERPC_EXCP_40x:
        qemu_fprintf(f, "  TCR " TARGET_FMT_lx "   TSR " TARGET_FMT_lx
                     "    ESR " TARGET_FMT_lx "   DEAR " TARGET_FMT_lx "\n",
                     env->spr[SPR_40x_TCR], env->spr[SPR_40x_TSR],
                     env->spr[SPR_40x_ESR], env->spr[SPR_40x_DEAR]);

        qemu_fprintf(f, " EVPR " TARGET_FMT_lx "  SRR2 " TARGET_FMT_lx
                     "   SRR3 " TARGET_FMT_lx  "   PID " TARGET_FMT_lx "\n",
                     env->spr[SPR_40x_EVPR], env->spr[SPR_40x_SRR2],
                     env->spr[SPR_40x_SRR3], env->spr[SPR_40x_PID]);
        break;
    default:
        break;
    }

#if defined(TARGET_PPC64)
    if (env->flags & POWERPC_FLAG_CFAR) {
        qemu_fprintf(f, " CFAR " TARGET_FMT_lx"\n", env->cfar);
    }
#endif

    if (env->spr_cb[SPR_LPCR].name) {
        qemu_fprintf(f, " LPCR " TARGET_FMT_lx "\n", env->spr[SPR_LPCR]);
    }

    switch (env->mmu_model) {
    case POWERPC_MMU_32B:
    case POWERPC_MMU_SOFT_6xx:
#if defined(TARGET_PPC64)
    case POWERPC_MMU_64B:
    case POWERPC_MMU_2_03:
    case POWERPC_MMU_2_06:
    case POWERPC_MMU_2_07:
    case POWERPC_MMU_3_00:
#endif
        if (env->spr_cb[SPR_SDR1].name) { /* SDR1 Exists */
            qemu_fprintf(f, " SDR1 " TARGET_FMT_lx " ", env->spr[SPR_SDR1]);
        }
        if (env->spr_cb[SPR_PTCR].name) { /* PTCR Exists */
            qemu_fprintf(f, " PTCR " TARGET_FMT_lx " ", env->spr[SPR_PTCR]);
        }
        qemu_fprintf(f, "  DAR " TARGET_FMT_lx "  DSISR " TARGET_FMT_lx "\n",
                     env->spr[SPR_DAR], env->spr[SPR_DSISR]);
        break;
    case POWERPC_MMU_BOOKE206:
        qemu_fprintf(f, " MAS0 " TARGET_FMT_lx "  MAS1 " TARGET_FMT_lx
                     "   MAS2 " TARGET_FMT_lx "   MAS3 " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_MAS0], env->spr[SPR_BOOKE_MAS1],
                     env->spr[SPR_BOOKE_MAS2], env->spr[SPR_BOOKE_MAS3]);

        qemu_fprintf(f, " MAS4 " TARGET_FMT_lx "  MAS6 " TARGET_FMT_lx
                     "   MAS7 " TARGET_FMT_lx "    PID " TARGET_FMT_lx "\n",
                     env->spr[SPR_BOOKE_MAS4], env->spr[SPR_BOOKE_MAS6],
                     env->spr[SPR_BOOKE_MAS7], env->spr[SPR_BOOKE_PID]);

        qemu_fprintf(f, "MMUCFG " TARGET_FMT_lx " TLB0CFG " TARGET_FMT_lx
                     " TLB1CFG " TARGET_FMT_lx "\n",
                     env->spr[SPR_MMUCFG], env->spr[SPR_BOOKE_TLB0CFG],
                     env->spr[SPR_BOOKE_TLB1CFG]);
        break;
    default:
        break;
    }
#endif

#undef RGPL
#undef RFPL
}
type_init(ppc_cpu_register_types)
