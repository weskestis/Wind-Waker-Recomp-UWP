# CP5 — full non-Aurora GXRuntime chassis

CP1 through CP4 were confirmed on a physical Xbox Series X.

CP5 moves the host from selected runtime components to the full reusable
non-Aurora GXRuntime library from the pinned RecompCore revision.

## Runtime library

`CP5Runtime.lib` compiles the same C source set as upstream
`GXRuntime::runtime`:

- CPU core and interpreter
- DOL loader and boot globals
- ARAM
- DVD and DI
- event clock and VI clock
- guest memory and dirty tracking
- GX recomp parser/runtime
- PI interrupts
- SI, EXI and MMIO bus
- platform abstraction
- audio DMA / ADPCM / voice / event
- memory-card runtime
- deterministic headless backend
- save states
- HLE core, CARD, DVD and input

Aurora/Dawn rendering is deliberately not part of this checkpoint.

## Xbox self-test

At app startup CP5 exercises the linked production runtime:

- CPU allocation and GameCube boot globals
- 16 MiB ARAM and MEM1↔ARAM DMA
- 60 Hz VI event timing at the GameCube timebase
- PI interrupt cause/mask delivery
- the real GXRuntime platform installation layer
- the deterministic headless backend
- platform pad reads
- VI configuration/presentation counters
- platform audio sample-rate and PCM push path
- SI register/interrupt behavior

## Screen codes

- dark blue: all CP2/CP3/CP4/CP5 tests passed
- red: CP2 runtime slice failed
- orange: LocalState/SRAM persistence failed
- purple: CP3 generic packaged module failed
- yellow/gold: CP4 production module contract failed
- cyan/teal: CP5 full GXRuntime chassis failed
- hold A: green controller confirmation

## Next

The next host step is Aurora/Dawn presentation and audio integration against the
already-proven UWP CoreWindow. The private/personal generated Wind Waker module
can be introduced later without changing the public module ABI or chassis.

No disc image, extracted game files, generated game source, or saves are stored
in this public repository.
