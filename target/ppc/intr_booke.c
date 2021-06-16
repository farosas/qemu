#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_booke_class_init(ObjectClass *oc, void *data)
{
    PPCIntrModelClass *imc = PPC_INTR_MODEL_CLASS(oc);

    imc->id = POWERPC_EXCP_BOOKE;
}

static void ppc_intr_booke_init(Object *obj)
{
}
PPC_INTR_DEFINE_MODEL(booke)


static void ppc_intr_e200_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(e200, booke)


static void ppc_intr_e500_init(Object *obj)
{
}
PPC_INTR_DEFINE_SUBMODEL(e500, booke)
