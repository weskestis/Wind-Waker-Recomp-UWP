#include <stdint.h>
#include <string.h>

#include "cycle_domain.h"
#include "ipl_sram.h"
#include "pad_event_schedule.h"
#include "pad_wire.h"
#include "rel_scratch_allocator.h"

extern "C" bool bluewake_cp2_runtime_self_test(void)
{
    // REL scratch allocator: first-fit must skip an occupied 0x100-byte range.
    const BlueWakeScratchRange occupied[] = {
        { 0x1000u, 0x100u }
    };
    uint32_t address = 0u;
    if (!bluewake_rel_scratch_first_fit(
            0x1000u, 0x2000u, 0x20u,
            occupied, 1u, &address) ||
        address != 0x1100u)
        return false;

    // Pad pulse timing: trigger is deliberately visible on the next retrace.
    BluewakePadEventSchedule schedule;
    bluewake_pad_event_schedule_init(&schedule);
    if (!bluewake_pad_event_schedule_configure(&schedule, 0x1000u, 2u))
        return false;
    if (!bluewake_pad_event_schedule_trigger(&schedule, 10u))
        return false;
    if (bluewake_pad_event_schedule_sample(&schedule, 10u, 0u) != 0u)
        return false;
    if (bluewake_pad_event_schedule_sample(&schedule, 11u, 0u) != 0x1000u)
        return false;
    if (bluewake_pad_event_schedule_sample(&schedule, 13u, 0u) != 0u)
        return false;

    // IPL SRAM defaults are the upstream Dolphin-compatible stereo defaults.
    uint8_t sram[BLUEWAKE_IPL_SRAM_SIZE];
    bluewake_ipl_sram_defaults(sram);
    if (sram[19] != 0x2Cu)
        return false;
    if (memcmp(sram + 20, "DOLPHINSLOTA", 12) != 0)
        return false;

    // Cycle accounting: exact elapsed cycles must be credited from downcount.
    CPUState cpu = {};
    BluewakeCycleDomain domain;
    bluewake_cycle_domain_init(&domain, 256, nullptr, nullptr, nullptr);
    bluewake_cycle_domain_begin_turn(&domain, &cpu);
    bluewake_cycle_domain_prepare_dispatch(&domain, &cpu);
    if (cpu.cycle_budget != 256)
        return false;
    cpu.downcount = -13;
    if (bluewake_cycle_domain_end_turn(&domain, &cpu) != 13u)
        return false;
    if (domain.absolute_cycles != 13u)
        return false;

    // Pad merge and GameCube wire encoding.
    DolPadState configured = {};
    DolPadState live = {};
    DolPadState merged = {};
    live.button = 0x1000u;
    live.stick_x = 64;
    bluewake_pad_merge(&configured, &live, &merged);
    if (merged.button != 0x1000u || merged.stick_x != 64)
        return false;

    uint32_t data0 = 0u, data1 = 0u;
    bluewake_pad_wire_encode(&merged, merged.button, &data0, &data1);
    if ((data0 >> 16) != 0x1000u)
        return false;

    return true;
}
