#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_g2_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_G2;
}

static void ppc_intr_g2_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(g2)
