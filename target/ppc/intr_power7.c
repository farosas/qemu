#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_power7_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_POWER7;
}

static void ppc_intr_power7_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(power7)
