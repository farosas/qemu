#include "qemu/osdep.h"
#include "ppc_intr.h"

static void ppc_intr_970_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_970;
    imc->hreset_vector = 0x0000000000000100ULL;
}

static void ppc_intr_970_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    im->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    im->excp_vectors[POWERPC_EXCP_DSI]      = 0x00000300;
    im->excp_vectors[POWERPC_EXCP_DSEG]     = 0x00000380;
    im->excp_vectors[POWERPC_EXCP_ISI]      = 0x00000400;
    im->excp_vectors[POWERPC_EXCP_ISEG]     = 0x00000480;
    im->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    im->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    im->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    im->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    im->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    im->excp_vectors[POWERPC_EXCP_HDECR]    = 0x00000980;
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    im->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    im->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    im->excp_vectors[POWERPC_EXCP_VPU]      = 0x00000F20;
    im->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
    im->excp_vectors[POWERPC_EXCP_MAINT]    = 0x00001600;
    im->excp_vectors[POWERPC_EXCP_VPUA]     = 0x00001700;
    im->excp_vectors[POWERPC_EXCP_THERM]    = 0x00001800;
}
PPC_INTR_DEFINE_MODEL(970)
