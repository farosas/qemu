#include "trace/trace-target_ppc.h"

static inline void trace_ppc_syscall(CPUPPCState *env, int lev)
{
    PowerPCCPU *cpu = env_archcpu(env);

    if (lev == 1 && cpu->vhyp) {
        trace_ppc_excp_hypercall(ppc_dump_gpr(env, 3), ppc_dump_gpr(env, 4),
                                 ppc_dump_gpr(env, 5), ppc_dump_gpr(env, 6),
                                 ppc_dump_gpr(env, 7), ppc_dump_gpr(env, 8),
                                 ppc_dump_gpr(env, 9), ppc_dump_gpr(env, 10),
                                 env->nip);
    } else {
        trace_ppc_excp_syscall(ppc_dump_gpr(env, 0), ppc_dump_gpr(env, 3),
                               ppc_dump_gpr(env, 4), ppc_dump_gpr(env, 5),
                               ppc_dump_gpr(env, 6), ppc_dump_gpr(env, 7),
                               ppc_dump_gpr(env, 8), env->nip);
    }
}
