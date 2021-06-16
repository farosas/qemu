#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_74xx_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_74xx;
}

static void ppc_intr_74xx_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(74xx)


static void ppc_intr_7400_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(7400, 74xx)


static void ppc_intr_7450_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(7450, 74xx)
