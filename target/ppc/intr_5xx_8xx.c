#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_mpc5xx_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_5xx_8xx;
}

static void ppc_intr_mpc5xx_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(mpc5xx)


static void ppc_intr_mpc8xx_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(mpc8xx, mpc5xx)
