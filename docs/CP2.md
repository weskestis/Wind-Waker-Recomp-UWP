# CP2 — upstream runtime slice

CP1 was confirmed on a physical Xbox Series X: UWP launch, Direct3D 12 presentation,
Windows.Gaming.Input, and LocalSettings all worked.

CP2 keeps that exact foundation and starts compiling Wind Waker Recomp's real
v0.3.0 runtime source into the Xbox executable.

## Pinned sources

- Wind Waker Recomp: `6def7cd4dcd792ba0b4d25ba9883f96debc8f1b3`
- RecompCore: `634895470af6e61e601f06a351f3a72888215159`

Both are fetched by CI at their exact commit hashes.

## Runtime components in this checkpoint

The UWP executable directly compiles these unmodified upstream host files:

- `runtime/host/src/cycle_domain.c`
- `runtime/host/src/ipl_sram.c`
- `runtime/host/src/pad_event_schedule.c`
- `runtime/host/src/pad_wire.c`
- `runtime/host/src/rel_scratch_allocator.c`

The real RecompCore `CPUState`, type definitions, and platform pad structures
are used from the pinned GXRuntime headers.

## Runtime self-test

At startup `RuntimeProbe.cpp` exercises:

- REL scratch first-fit allocation
- controller pulse scheduling
- Dolphin-compatible IPL SRAM defaults
- guest cycle accounting through `CPUState.downcount`
- GameCube controller merge/wire encoding

A pass keeps the normal dark blue CP1 background. A failure changes the idle
screen to red. Holding Xbox A still changes the screen to green.

The pass/fail state is also written to UWP LocalSettings as `CP2RuntimeProbe`.

## Deferred

The full 10k+ line host, Aurora/Dawn, DSP audio, CARD HLE, dynamic game-module
loading, disc import, and translated GZLE01 module remain later CP2/CP3 work.
The next portability seam is storage + module loading: replace POSIX/desktop
path and dynamic-library assumptions with UWP-safe implementations.
