#include <stdint.h>
#include <string.h>
#include "StaticRecompABI.h"

typedef bool (*BluewakeEdgeServiceFn)(void* user, CPUState* cpu, u32 address);

static BluewakeEdgeServiceFn g_edge_service = nullptr;
static void* g_edge_service_user = nullptr;

__declspec(align(4096)) static u8 g_mem1[0x02000000u];
static CPUState g_cpu;

static int cp4_dispatch(CPUState* cpu, u32 address)
{
    if (cpu == nullptr || address != 0x80001000u)
        return 0;

    cpu->gpr[3] = 0x43503431u; // "CP41"
    cpu->pc = 0x80001004u;
    cpu->downcount -= 4;

    if (g_edge_service != nullptr)
        (void)g_edge_service(g_edge_service_user, cpu, cpu->pc);

    return 1;
}

static void cp4_on_state_loaded(CPUState* cpu)
{
    if (cpu != nullptr)
        ppc_fpscr_updated(cpu);
}

static const StaticRecompRange s_code_ranges[] = {
    { 0x80001000u, 0x80001004u }
};

static const u64 s_chunk_hashes[] = { 0u };

static const StaticRecompModuleDesc s_module = {
    STATICRECOMP_ABI_VERSION,
    GXRUNTIME_CPU_ABI_VERSION,
    (u32)sizeof(CPUState),
    { 'G', 'Z', 'L', 'E', '0', '1', 0, 0 },
    0x80001000u,
    cp4_dispatch,
    cp4_on_state_loaded,
    s_code_ranges,
    1u,
    nullptr,
    0u,
    s_code_ranges,
    1u,
    s_chunk_hashes,
    nullptr,
    0u
};

extern "C" __declspec(dllexport)
const StaticRecompModuleDesc* staticrecomp_get_module(void)
{
    return &s_module;
}

extern "C" __declspec(dllexport)
CPUState* bluewake_composite_guest_cpu(void)
{
    return &g_cpu;
}

extern "C" __declspec(dllexport)
u8* bluewake_composite_guest_mem1(u32* size)
{
    if (size != nullptr)
        *size = (u32)sizeof(g_mem1);
    return g_mem1;
}

extern "C" __declspec(dllexport)
void bluewake_set_mem_write_journal(PPCMemWriteJournal fn, void* user)
{
    ppc_set_mem_write_journal(fn, user);
}

extern "C" __declspec(dllexport)
void bluewake_set_edge_service(BluewakeEdgeServiceFn fn, void* user)
{
    g_edge_service = fn;
    g_edge_service_user = user;
}
