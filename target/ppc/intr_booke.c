#include "qemu/osdep.h"
#include "cpu-models.h"
#include "ppc_intr.h"

static void ppc_intr_booke_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_BOOKE;
    imc->hreset_vector = 0xFFFFFFFCUL;
}

static void ppc_intr_booke_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_CRITICAL] = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_APU]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_FIT]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_WDT]      = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_DTLB]     = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_ITLB]     = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_DEBUG]    = 0x00000000;

    im->ivor_mask = 0x0000FFF0UL;
    im->ivpr_mask = 0xFFFF0000UL;
}
PPC_INTR_DEFINE_MODEL(booke)


static void ppc_intr_e200_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000FFC;
    im->excp_vectors[POWERPC_EXCP_SPEU]     = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_EFPDI]    = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_EFPRI]    = 0x00000000;

    im->ivor_mask = 0x0000FFF7UL;
    im->ivpr_mask = 0xFFFF0000UL;
}
PPC_INTR_DEFINE_SUBMODEL(e200, booke)


static void ppc_intr_e500_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);
    ObjectClass *oc = object_class_by_name(TYPE_POWERPC_CPU);
    PowerPCCPUClass *pcc = POWERPC_CPU_CLASS(oc);
    uint64_t ivpr_mask = 0xFFFF0000ULL;

    im->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000FFC;
    im->excp_vectors[POWERPC_EXCP_SPEU]     = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_EFPDI]    = 0x00000000;
    im->excp_vectors[POWERPC_EXCP_EFPRI]    = 0x00000000;

    if (pcc->pvr == CPU_POWERPC_e5500 || pcc->pvr == CPU_POWERPC_e6500) {
        ivpr_mask = (target_ulong)~0xFFFFULL;
    }

    im->ivor_mask = 0x0000FFF7UL;
    im->ivpr_mask = ivpr_mask;
}
PPC_INTR_DEFINE_SUBMODEL(e500, booke)
