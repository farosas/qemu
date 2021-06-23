#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_mpc5xx_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_5xx_8xx;
    imc->hreset_vector = 0x00000100UL;
}

static void ppc_intr_mpc5xx_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_RESET]    = 0x00000100;
    im->excp_vectors[POWERPC_EXCP_MCHECK]   = 0x00000200;
    im->excp_vectors[POWERPC_EXCP_EXTERNAL] = 0x00000500;
    im->excp_vectors[POWERPC_EXCP_ALIGN]    = 0x00000600;
    im->excp_vectors[POWERPC_EXCP_PROGRAM]  = 0x00000700;
    im->excp_vectors[POWERPC_EXCP_FPU]      = 0x00000800;
    im->excp_vectors[POWERPC_EXCP_DECR]     = 0x00000900;
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    im->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    im->excp_vectors[POWERPC_EXCP_FPA]      = 0x00000E00;
    im->excp_vectors[POWERPC_EXCP_EMUL]     = 0x00001000;
    im->excp_vectors[POWERPC_EXCP_DABR]     = 0x00001C00;
    im->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001C00;
    im->excp_vectors[POWERPC_EXCP_MEXTBR]   = 0x00001E00;
    im->excp_vectors[POWERPC_EXCP_NMEXTBR]  = 0x00001F00;
}
PPC_INTR_DEFINE_MODEL(mpc5xx)


static void ppc_intr_mpc8xx_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_DSI]   = 0x00000300;
    im->excp_vectors[POWERPC_EXCP_ISI]   = 0x00000400;
    im->excp_vectors[POWERPC_EXCP_ITLB]  = 0x00001100;
    im->excp_vectors[POWERPC_EXCP_DTLB]  = 0x00001200;
    im->excp_vectors[POWERPC_EXCP_ITLBE] = 0x00001300;
    im->excp_vectors[POWERPC_EXCP_DTLBE] = 0x00001400;
}
PPC_INTR_DEFINE_SUBMODEL(mpc8xx, mpc5xx)
