#include "qemu/osdep.h"
#include "ppc_intr.h"


static void ppc_intr_model_class_init(ObjectClass *oc, void *data)
{
}

static const TypeInfo ppc_intr_model_info = {
    .name = TYPE_PPC_INTR_MODEL,
    .parent = TYPE_OBJECT,
    .class_size = sizeof(PPCIntrModelClass),
    .class_init = ppc_intr_model_class_init,
    .abstract = true,
};

static void ppc_intr_register_types(void)
{
    type_register_static(&ppc_intr_model_info);
}
type_init(ppc_intr_register_types);
