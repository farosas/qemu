#ifndef PPC_INTR_H
#define PPC_INTR_H

#include "qom/object.h"
#include "cpu-qom.h"

#define TYPE_PPC_INTERRUPT "ppc-interrupt"
OBJECT_DECLARE_SIMPLE_TYPE(PPCInterrupt, PPC_INTERRUPT)

void __ppc_intr_add(CPUPPCState *env, target_ulong addr, int id,
                    const char *intr_name);

#define ppc_intr_add(_env, _addr, _id)                  \
    do {                                                \
        QEMU_BUILD_BUG_ON(_id <= POWERPC_EXCP_NONE);    \
        QEMU_BUILD_BUG_ON(_id >= POWERPC_EXCP_NB);      \
        __ppc_intr_add(_env, _addr, _id, #_id);         \
    } while (0)                                         \

/*
 * Registers an interrupt callback as a class. This makes it so that
 * the interrupt callback implementation is stored on QOM and we can
 * instantiate only the ones needed for a specific processor later.
 *
 * @_id: The interrupt id as in the POWERPC_EXCP_* enum. This will be
 *   the QOM hash table key for the type.
 * @_sym: The interrupt name as a valid C identifier. This will be
 *   used to compose the symbol name for the callback to be invoked
 *   for this interrupt.
 * @_name: The interrupt name as a string for display.
 */
#define PPC_DEFINE_INTR(_id, _sym, _name)       \
                                                \
    static void __##_id##_init(Object *obj)     \
    {                                           \
        PPCInterrupt *pi = PPC_INTERRUPT(obj);  \
                                                \
        pi->id = _id;                           \
        pi->name = _name;                       \
        pi->setup_regs = ppc_intr_##_sym;       \
    }                                           \
                                                \
    static const TypeInfo __##_id##_info = {    \
        .parent = TYPE_PPC_INTERRUPT,           \
        .name = #_id,                           \
        .instance_init = __##_id##_init,        \
    };                                          \
                                                \
    static void __##_id##_register_types(void)  \
    {                                           \
        type_register_static(&__##_id##_info);  \
    }                                           \
    type_init(__##_id##_register_types);        \

#endif /* PPC_INTR_H */
