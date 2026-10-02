#include <windows.h>
#include <stdint.h>
#include <string>

#include "StaticRecompABI.h"
#include "ipl_sram.h"

static u8 cp3_read8(void*, u32)
{
    return 0u;
}

static void cp3_write8(void*, u32, u8)
{
}

static bool cp3_narrow_ascii(const wchar_t* wide, std::string& out)
{
    if (wide == nullptr)
        return false;

    out.clear();
    for (const wchar_t* p = wide; *p != 0; ++p)
    {
        if (*p < 0 || *p > 0x7f)
            return false;
        out.push_back((char)*p);
    }
    return !out.empty();
}

extern "C" bool bluewake_cp3_storage_self_test(
    const wchar_t* localFolder,
    uint8_t* beforeOut,
    uint8_t* afterOut)
{
    std::string base;
    if (!cp3_narrow_ascii(localFolder, base))
        return false;

    const std::string path = base + "\\cp3-sram-test.bin";

    BluewakeIplSram first;
    bluewake_ipl_sram_init(&first, path.c_str());
    if (!first.enabled || first.path == nullptr)
        return false;

    const uint8_t before = first.sram[63];
    uint8_t after = (uint8_t)(before + 1u);
    if (after == 0u)
        after = 1u;

    // EXI channel 0 / device 1. Latch an SRAM write command at byte 63,
    // then issue a one-byte immediate write through the upstream device model.
    const u32 exi = 0xCC006800u;
    bluewake_ipl_sram_write(
        &first, exi + 0x00u, 0x100u, cp3_read8, cp3_write8, nullptr);
    bluewake_ipl_sram_write(
        &first, exi + 0x10u, 0xA0000100u + (63u << 6),
        cp3_read8, cp3_write8, nullptr);
    bluewake_ipl_sram_write(
        &first, exi + 0x0Cu, 0x35u, cp3_read8, cp3_write8, nullptr);
    bluewake_ipl_sram_write(
        &first, exi + 0x10u, (u32)after << 24,
        cp3_read8, cp3_write8, nullptr);
    bluewake_ipl_sram_write(
        &first, exi + 0x0Cu, 0x05u, cp3_read8, cp3_write8, nullptr);

    BluewakeIplSram reloaded;
    bluewake_ipl_sram_init(&reloaded, path.c_str());
    if (!reloaded.enabled || reloaded.sram[63] != after)
        return false;

    if (beforeOut != nullptr)
        *beforeOut = before;
    if (afterOut != nullptr)
        *afterOut = after;
    return true;
}

extern "C" bool bluewake_cp3_module_self_test(void)
{
    HMODULE library = LoadPackagedLibrary(L"CP3Module.dll", 0);
    if (library == nullptr)
        return false;

    auto getModule = reinterpret_cast<StaticRecompGetModuleFn>(
        GetProcAddress(library, STATICRECOMP_GET_MODULE_SYMBOL));
    if (getModule == nullptr)
        return false;

    const StaticRecompModuleDesc* module = getModule();
    if (module == nullptr ||
        module->abi_version != STATICRECOMP_ABI_VERSION ||
        module->cpu_abi_version != GXRUNTIME_CPU_ABI_VERSION ||
        module->cpu_state_size != sizeof(CPUState) ||
        memcmp(module->game_id, "GZLE01", 6) != 0 ||
        module->dispatch == nullptr)
        return false;

    CPUState cpu = {};
    if (!module->dispatch(&cpu, 0x80001000u))
        return false;

    return cpu.pc == 0x80001004u && cpu.gpr[3] == 0x43503331u;
}
