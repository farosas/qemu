#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_7xx_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_7xx;
}

static void ppc_intr_7xx_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(7xx)


static void ppc_intr_7x0_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(7x0, 7xx)


static void ppc_intr_750cl_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(750cl, 7xx)


static void ppc_intr_750cx_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(750cx, 7xx)


static void ppc_intr_7x5_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(7x5, 7xx)
