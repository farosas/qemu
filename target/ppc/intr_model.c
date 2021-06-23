#include "qemu/osdep.h"
#include "ppc_intr.h"

static void ppc_intr_model_class_init(ObjectClass *oc, void *data)
{
}

static void ppc_intr_model_post_init(Object *obj)
{
    PPCIntrModel *im = PPC_INTR_MODEL(obj);
    int i;

    /* Set all exception vectors to an invalid address */
    for (i = 0; i < POWERPC_EXCP_NB; i++) {
        im->excp_vectors[i] = (target_ulong)(-1ULL);
    }
}

static const TypeInfo ppc_intr_model_info = {
    .name = TYPE_PPC_INTR_MODEL,
    .parent = TYPE_OBJECT,
    .class_size = sizeof(PPCIntrModelClass),
    .class_init = ppc_intr_model_class_init,
    .instance_post_init = ppc_intr_model_post_init,
    .abstract = true,
};

static void ppc_intr_register_types(void)
{
    type_register_static(&ppc_intr_model_info);
}
type_init(ppc_intr_register_types);
