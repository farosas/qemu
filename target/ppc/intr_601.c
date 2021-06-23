#include "qemu/osdep.h"
#include "ppc_intr.h"

static void ppc_intr_601_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_601;
    imc->hreset_vector = 0x00000100UL;
}

static void ppc_intr_601_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    im->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    im->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    im->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    im->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    im->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    im->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    im->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    im->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    im->excp_vectors[POWERPC_EXCP_IO]       = 0x00000A00;
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    im->excp_vectors[POWERPC_EXCP_RUNM]     = 0x00002000;
}
PPC_INTR_DEFINE_MODEL(601)
