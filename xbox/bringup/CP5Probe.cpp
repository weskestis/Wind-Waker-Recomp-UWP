#include <stdint.h>
#include <string.h>

extern "C" {
#include "core/cpu.h"
#include "gxruntime/aram.h"
#include "gxruntime/boot.h"
#include "gxruntime/headless_backend.h"
#include "gxruntime/interrupts.h"
#include "gxruntime/platform.h"
#include "gxruntime/si.h"
#include "gxruntime/vi_clock.h"
}

extern "C" bool bluewake_cp5_chassis_self_test(void)
{
    CPUState cpu = {};
    if (!cpu_init(&cpu))
        return false;

    // Boot globals: verify the runtime establishes a GameCube-like low-memory
    // environment and initial stack exactly through the production boot helper.
    DolLayout layout = {};
    layout.bss_address = GC_RAM_BASE + 0x4000u;
    layout.bss_size = 0x2000u;
    layout.entry_point = GC_RAM_BASE + 0x3100u;
    boot_setup_os_globals(&cpu, &layout);

    if (mem_read32(&cpu, 0x80000020u) != 0x0D15EA5Eu ||
        mem_read32(&cpu, 0x80000028u) != 0x01800000u ||
        cpu.gpr[1] != 0x817FFF00u)
    {
        cpu_free(&cpu);
        return false;
    }

    // ARAM: real 16 MiB backing plus MEM1 DMA in both directions.
    aram_free();
    aram_init();
    if (!aram_contains(ARAM_BASE) ||
        !aram_contains(ARAM_BASE + ARAM_SIZE - 1u))
    {
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    mem_write32(&cpu, GC_RAM_BASE + 0x180u, 0xDEADBEEFu);
    aram_dma_to_aram(cpu.ram, GC_RAM_BASE + 0x180u, 0x40u, 4u);
    if (aram_read(0x40u, 4u) != 0xDEADBEEFu)
    {
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    mem_write32(&cpu, GC_RAM_BASE + 0x190u, 0u);
    aram_dma_to_ram(cpu.ram, GC_RAM_BASE + 0x190u, 0x40u, 4u);
    if (mem_read32(&cpu, GC_RAM_BASE + 0x190u) != 0xDEADBEEFu)
    {
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    // VI event clock: one 60 Hz retrace at the GameCube timebase.
    DolViClock vi = {};
    dol_vi_clock_init(&vi);
    dol_vi_clock_configure(&vi, 100u, 60u, 40500000ull);
    dol_vi_clock_advance(&vi, 100u);
    u64 ticks = 0u;
    if (!dol_vi_clock_pop_retrace(&vi, &ticks) || ticks != 675000u)
    {
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    // PI interrupt mask/cause delivery.
    DolInterrupts interrupts = {};
    dol_interrupts_init(&interrupts);
    dol_interrupts_set_source(&interrupts, DOL_PI_CAUSE_VI, true);
    dol_interrupts_mmio_write(
        &interrupts, DOL_PI_INTERRUPT_MASK, 4u, DOL_PI_CAUSE_VI);
    if (!dol_interrupts_external_pending(&interrupts))
    {
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    // Real GXRuntime platform layer with the deterministic headless backend.
    DolHeadlessBackend backend = {};
    dol_headless_backend_init(&backend);
    backend.pad_mask = 1u;
    backend.pad[0].button = 0x1000u;
    backend.pad[0].stick_x = 64;
    dol_headless_backend_install(&backend);

    if (!dol_platform_available() || !dol_platform_pad_init())
    {
        dol_platform_reset();
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    DolPadState pads[4] = {};
    const u32 padMask = dol_platform_pad_read(pads);
    if (padMask != 1u ||
        pads[0].button != 0x1000u ||
        pads[0].stick_x != 64 ||
        backend.pad_read_count != 1u)
    {
        dol_platform_reset();
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    dol_platform_configure_vi(0u, 640u, 480u, 480u, 640u, 480u);
    dol_platform_mark_gx_begin();
    dol_platform_present();

    s16 silence[16] = {};
    dol_platform_audio_set_sample_rate(32000u);
    dol_platform_audio_push(silence, 8u);

    if (backend.configure_vi_count != 1u ||
        backend.begin_count != 1u ||
        backend.present_count != 1u ||
        backend.audio_sample_rate != 32000u ||
        backend.audio_frames != 8u ||
        backend.audio_push_count != 1u)
    {
        dol_platform_reset();
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    // SI register state and interrupt behavior.
    DolSiDevice si = {};
    dol_si_init(&si);
    dol_si_mmio_write(
        &si, DOL_SI_BASE + DOL_SI_COMCSR_OFF, 4u, DOL_SI_RDSTINTMSK);
    dol_si_latch_poll(&si, 1u);
    if (!dol_si_interrupt_pending(&si))
    {
        dol_platform_reset();
        aram_free();
        cpu_free(&cpu);
        return false;
    }

    dol_platform_reset();
    aram_free();
    cpu_free(&cpu);
    return true;
}
