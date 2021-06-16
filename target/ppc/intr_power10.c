#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_power10_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_POWER10;
}

static void ppc_intr_power10_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(power10)
