#include "qemu/osdep.h"
#include "ppc_intr.h"

static void ppc_intr_40x_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_40x;
}

static void ppc_intr_40x_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(40x)


static void ppc_intr_40x_softmmu_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(40x_softmmu, 40x)
