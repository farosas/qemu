#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_7xx_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_7xx;
}

static void ppc_intr_7xx_init(Object *obj)
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
    im->excp_vectors[POWERPC_EXCP_SYSCALL]  = 0x00000C00;
    im->excp_vectors[POWERPC_EXCP_TRACE]    = 0x00000D00;
    im->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    im->excp_vectors[POWERPC_EXCP_IABR]     = 0x00001300;
}
PPC_INTR_DEFINE_MODEL(7xx)


static void ppc_intr_7x0_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_SMI]   = 0x00001400;
    im->excp_vectors[POWERPC_EXCP_THERM] = 0x00001700;
}
PPC_INTR_DEFINE_SUBMODEL(7x0, 7xx)


static void ppc_intr_750cl_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_SMI] = 0x00001400;
}
PPC_INTR_DEFINE_SUBMODEL(750cl, 7xx)


static void ppc_intr_750cx_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_THERM] = 0x00001700;
}
PPC_INTR_DEFINE_SUBMODEL(750cx, 7xx)


static void ppc_intr_7x5_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);

    im->excp_vectors[POWERPC_EXCP_IFTLB] = 0x00001000;
    im->excp_vectors[POWERPC_EXCP_DLTLB] = 0x00001100;
    im->excp_vectors[POWERPC_EXCP_DSTLB] = 0x00001200;
    im->excp_vectors[POWERPC_EXCP_SMI]   = 0x00001400;
    im->excp_vectors[POWERPC_EXCP_THERM] = 0x00001700;
}
PPC_INTR_DEFINE_SUBMODEL(7x5, 7xx)
