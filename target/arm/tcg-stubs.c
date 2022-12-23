/*
 * QEMU ARM stubs for some TCG helper functions
 *
 * Copyright 2021 SUSE LLC
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"
#include "exec/memattrs.h"

void write_v7m_exception(CPUARMState *env, uint32_t new_exc)
{
    g_assert_not_reached();
}

void arm_rebuild_hflags(CPUARMState *env)
{
    g_assert_not_reached();
}

void assert_hflags_rebuild_correctly(CPUARMState *env)
{
}

void raise_exception_ra(CPUARMState *env, uint32_t excp, uint32_t syndrome,
                        uint32_t target_el, uintptr_t ra)
{
    g_assert_not_reached();
}

void arm_reset_sve_state(CPUARMState *env)
{
    g_assert_not_reached();
}

hwaddr arm_cpu_get_phys_page_attrs_debug(CPUState *cs, vaddr addr,
                                         MemTxAttrs *attrs)
{
    return 0;
}
