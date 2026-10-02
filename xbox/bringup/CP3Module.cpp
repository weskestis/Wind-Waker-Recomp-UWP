#include "StaticRecompABI.h"
#include <string.h>

static int cp3_dispatch(CPUState* cpu, u32 address)
{
    if (cpu == nullptr || address != 0x80001000u)
        return 0;

    cpu->gpr[3] = 0x43503331u; // "CP31"
    cpu->pc = 0x80001004u;
    return 1;
}

static const StaticRecompRange s_code_ranges[] = {
    { 0x80001000u, 0x80001004u }
};

static const StaticRecompModuleDesc s_module = {
    STATICRECOMP_ABI_VERSION,
    GXRUNTIME_CPU_ABI_VERSION,
    (u32)sizeof(CPUState),
    { 'G', 'Z', 'L', 'E', '0', '1', 0, 0 },
    0x80001000u,
    cp3_dispatch,
    nullptr,
    s_code_ranges,
    1u,
    nullptr,
    0u,
    s_code_ranges,
    1u,
    nullptr,
    nullptr,
    0u
};

extern "C" __declspec(dllexport)
const StaticRecompModuleDesc* staticrecomp_get_module(void)
{
    return &s_module;
}
