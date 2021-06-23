#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_power9_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_POWER9;
    imc->hreset_vector = 0x0000000000000100ULL;
}

static void ppc_intr_power9_init(Object *obj)
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
    im->excp_vectors[POWERPC_EXCP_HDSI]     = 0x00000E00;
    im->excp_vectors[POWERPC_EXCP_HISI]     = 0x00000E20;
    im->excp_vectors[POWERPC_EXCP_HV_EMU]   = 0x00000E40;
    im->excp_vectors[POWERPC_EXCP_HV_MAINT] = 0x00000E60;
    im->excp_vectors[POWERPC_EXCP_PERFM]    = 0x00000F00;
    im->excp_vectors[POWERPC_EXCP_VPU]      = 0x00000F20;
    im->excp_vectors[POWERPC_EXCP_VSXU]     = 0x00000F40;
    im->excp_vectors[POWERPC_EXCP_SDOOR]    = 0x00000A00;
    im->excp_vectors[POWERPC_EXCP_FU]       = 0x00000F60;
    im->excp_vectors[POWERPC_EXCP_HV_FU]    = 0x00000F80;
    im->excp_vectors[POWERPC_EXCP_SDOOR_HV] = 0x00000E80;
    im->excp_vectors[POWERPC_EXCP_HVIRT]    = 0x00000EA0;
    im->excp_vectors[POWERPC_EXCP_SYSCALL_VECTORED] = 0x00017000;
}
PPC_INTR_DEFINE_MODEL(power9)
