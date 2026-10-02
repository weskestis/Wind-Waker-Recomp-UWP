#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "StaticRecompABI.h"

typedef CPUState* (*GuestCpuFn)(void);
typedef u8* (*GuestMemFn)(u32* size);
typedef void (*AliasClearFn)(void);
typedef bool (*AliasAddSharedFn)(u32 linked_start, u32 size, u8* storage);
typedef bool (*AliasGetStorageFn)(u32 linked_start, u32 size, u8** storage);
typedef bool (*AliasRemoveFn)(u32 linked_start, u32 size);
typedef bool (*AliasResolveFn)(u32 address, u32 size, u8** pointer, u32* journal_offset);
typedef void (*SetJournalFn)(PPCMemWriteJournal fn, void* user);
typedef bool (*EdgeServiceFn)(void* user, CPUState* cpu, u32 address);
typedef void (*SetEdgeServiceFn)(EdgeServiceFn fn, void* user);

struct Cp4Counters
{
    unsigned journalCalls;
    unsigned edgeCalls;
};

static void cp4_journal(u32 offset, u32 size, void* user)
{
    Cp4Counters* counters = static_cast<Cp4Counters*>(user);
    if (counters != nullptr && offset == 0u && size == 4u)
        counters->journalCalls++;
}

static bool cp4_edge(void* user, CPUState*, u32 address)
{
    Cp4Counters* counters = static_cast<Cp4Counters*>(user);
    if (counters != nullptr && address == 0x80001004u)
        counters->edgeCalls++;
    return false;
}

extern "C" bool bluewake_cp4_real_module_self_test(void)
{
    HMODULE library = LoadPackagedLibrary(L"gGZLE01_recomp.dll", 0);
    if (library == nullptr)
        return false;

    auto getModule = reinterpret_cast<StaticRecompGetModuleFn>(
        GetProcAddress(library, STATICRECOMP_GET_MODULE_SYMBOL));
    auto guestCpu = reinterpret_cast<GuestCpuFn>(
        GetProcAddress(library, "bluewake_composite_guest_cpu"));
    auto guestMem = reinterpret_cast<GuestMemFn>(
        GetProcAddress(library, "bluewake_composite_guest_mem1"));
    auto aliasClear = reinterpret_cast<AliasClearFn>(
        GetProcAddress(library, "ppc_guest_alias_clear"));
    auto aliasAddShared = reinterpret_cast<AliasAddSharedFn>(
        GetProcAddress(library, "ppc_guest_alias_add_shared"));
    auto aliasGetStorage = reinterpret_cast<AliasGetStorageFn>(
        GetProcAddress(library, "ppc_guest_alias_get_storage"));
    auto aliasRemove = reinterpret_cast<AliasRemoveFn>(
        GetProcAddress(library, "ppc_guest_alias_remove"));
    auto aliasResolve = reinterpret_cast<AliasResolveFn>(
        GetProcAddress(library, "ppc_guest_alias_resolve"));
    auto setJournal = reinterpret_cast<SetJournalFn>(
        GetProcAddress(library, "bluewake_set_mem_write_journal"));
    auto setEdge = reinterpret_cast<SetEdgeServiceFn>(
        GetProcAddress(library, "bluewake_set_edge_service"));

    if (getModule == nullptr || guestCpu == nullptr || guestMem == nullptr ||
        aliasClear == nullptr || aliasAddShared == nullptr ||
        aliasGetStorage == nullptr || aliasRemove == nullptr ||
        aliasResolve == nullptr || setJournal == nullptr || setEdge == nullptr)
        return false;

    const StaticRecompModuleDesc* module = getModule();
    if (module == nullptr ||
        module->abi_version != STATICRECOMP_ABI_VERSION ||
        module->cpu_abi_version != GXRUNTIME_CPU_ABI_VERSION ||
        module->cpu_state_size != sizeof(CPUState) ||
        memcmp(module->game_id, "GZLE01", 6) != 0 ||
        module->dispatch == nullptr)
        return false;

    u32 memSize = 0u;
    u8* mem1 = guestMem(&memSize);
    CPUState* cpu = guestCpu();
    if (mem1 == nullptr || memSize != 0x02000000u || cpu == nullptr)
        return false;

    memset(mem1, 0, memSize);
    memset(cpu, 0, sizeof(*cpu));
    cpu->ram = mem1;
    cpu->ram_size = memSize;

    // Exercise the real GXRuntime alias registry exported by the module.
    u8 backing[64] = {};
    aliasClear();
    if (!aliasAddShared(0x81F80000u, sizeof(backing), backing))
        return false;

    u8* storage = nullptr;
    if (!aliasGetStorage(0x81F80000u, sizeof(backing), &storage) ||
        storage != backing)
        return false;

    u8* resolved = nullptr;
    u32 journalOffset = 0u;
    if (!aliasResolve(0x81F80008u, 4u, &resolved, &journalOffset) ||
        resolved != backing + 8u ||
        journalOffset != 0x01F80008u)
        return false;

    if (!aliasRemove(0x81F80000u, sizeof(backing)))
        return false;
    resolved = nullptr;
    if (aliasResolve(0x81F80008u, 4u, &resolved, nullptr))
        return false;

    Cp4Counters counters = {};
    setJournal(cp4_journal, &counters);
    setEdge(cp4_edge, &counters);

    if (module->on_state_loaded != nullptr)
        module->on_state_loaded(cpu);

    cpu->downcount = 0;
    if (!module->dispatch(cpu, 0x80001000u))
        return false;

    if (cpu->pc != 0x80001004u ||
        cpu->gpr[3] != 0x43503431u ||
        cpu->downcount != -4 ||
        counters.journalCalls != 1u ||
        counters.edgeCalls != 1u)
        return false;

    if (read_be32(mem1) != 0x43503431u)
        return false;

    setJournal(nullptr, nullptr);
    setEdge(nullptr, nullptr);
    aliasClear();
    return true;
}
