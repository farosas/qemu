#include "qemu/osdep.h"
#include "ppc_intr.h"

static void ppc_intr_40x_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_40x;
    imc->hreset_vector = 0xFFFFFFFCUL;
}

static void ppc_intr_40x_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_CRITICAL] = 0x00000100;
    im->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    im->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    im->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    im->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    im->excp_vectors[POWERPC_EXCP_PIT]      = 0x00001000;
    im->excp_vectors[POWERPC_EXCP_FIT]      = 0x00001010;
    im->excp_vectors[POWERPC_EXCP_WDT]      = 0x00001020;
    im->excp_vectors[POWERPC_EXCP_DEBUG]    = 0x00002000;

    im->ivpr_mask = 0xFFFF0000UL;
}
PPC_INTR_DEFINE_MODEL(40x)


static void ppc_intr_40x_softmmu_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_DSI]  = 0x00000300;
    im->excp_vectors[POWERPC_EXCP_ISI]  = 0x00000400;
    im->excp_vectors[POWERPC_EXCP_DTLB] = 0x00001100;
    im->excp_vectors[POWERPC_EXCP_ITLB] = 0x00001200;
}
PPC_INTR_DEFINE_SUBMODEL(40x_softmmu, 40x)
