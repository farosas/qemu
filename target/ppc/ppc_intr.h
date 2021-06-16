#ifndef PPC_INTR_H
#define PPC_INTR_H

#include "qom/object.h"
#include "cpu-qom.h"
#include "cpu.h"

#define TYPE_PPC_INTR_MODEL "ppc-interrupt-model"
#define PPC_INTR_TYPE(_name) "ppc-interrupt-model-" stringify(_name)

/* structs in cpu.h */
OBJECT_DECLARE_TYPE(PPCIntrModel, PPCIntrModelClass, PPC_INTR_MODEL)

/*
 * Helper to define the PPCIntrModel(State|Class) QOM types. The
 * .class_size and .instance_size are omitted on purpose so that the
 * child types can avoid implementing an explicit class type
 * (i.e. child types are expected to *not* define their own
 * attributes, only to override the parent ones).
 */
#define __PPC_INTR_DEFINE(_name, _parent)                               \
    static TypeInfo ppc_intr_##_name##_info = {                         \
        .name = "ppc-interrupt-model-" stringify(_name),                \
        .parent = _parent,                                              \
        .class_init = ppc_intr_##_name##_class_init,                    \
        .instance_init = ppc_intr_##_name##_init,                       \
    };                                                                  \
    static void ppc_intr_##_name##_register_types(void)                 \
    {                                                                   \
        type_register_static(&ppc_intr_##_name##_info);                 \
    }                                                                   \
    type_init(ppc_intr_##_name##_register_types)                        \

/*
 * Defines an interrupt submodel. The caller should provide the _init
 * and _class_init functions. Unimplemented attributes will fallback
 * to the parent implementation.
 */
#define PPC_INTR_DEFINE_SUBMODEL(_name, _parent)                        \
    static void                                                         \
    ppc_intr_##_name##_class_init(ObjectClass *oc, void *data) {}       \
                                                                        \
    __PPC_INTR_DEFINE(_name, PPC_INTR_TYPE(_parent))                    \

#define PPC_INTR_DEFINE_MODEL(_name)                    \
    __PPC_INTR_DEFINE(_name, TYPE_PPC_INTR_MODEL)       \

#endif /* PPC_INTR_H */
