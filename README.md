# Wind-Waker-Recomp-UWP

Personal/unofficial Xbox Series X|S UWP port work for Wind Waker Recomp.

## Current checkpoint: CP4

Active branch: `cp4-real-module`

Physical Xbox Series X checkpoints already confirmed:

- **CP1:** x64 UWP launch, Direct3D 12 CoreWindow presentation, Xbox controller input and LocalSettings
- **CP2:** pinned Wind Waker Recomp v0.3.0 runtime components compiled and executed inside the UWP app
- **CP3:** LocalState persistence plus packaged native DLL loading through the real StaticRecomp ABI

CP4 moves from a generic probe DLL to the production Wind Waker module contract:

- package name `gGZLE01_recomp.dll`
- pinned RecompCore GXRuntime CPU core linked inside the DLL
- real `StaticRecompModuleDesc` ABI
- fixed guest `CPUState`
- fixed 32 MiB MEM1 export
- guest-alias runtime exports
- memory-write journal callback
- host edge-service callback
- UWP `LoadPackagedLibrary` loading path
- CI export and MSIX-content validation

See [docs/CP1.md](docs/CP1.md), [docs/CP2.md](docs/CP2.md),
[docs/CP3.md](docs/CP3.md), and [docs/CP4.md](docs/CP4.md).

## Upstream pins

- Wind Waker Recomp v0.3.0:
  `6def7cd4dcd792ba0b4d25ba9883f96debc8f1b3`
- RecompCore:
  `634895470af6e61e601f06a351f3a72888215159`

The Wind Waker pin is also recorded in `UPSTREAM.lock`.

## Game data

No Wind Waker disc image, extracted Nintendo game files, generated translated game source,
saves, textures, audio, keys, or other private game data belong in this repository.

The public repository contains the Xbox/UWP platform work and diagnostic module skeletons.
The eventual real GZLE01 translated module remains a personal build from the user's legally
obtained disc, while using the same CP4 package/module contract proven here.

## Goal

The next major milestone after CP4 is to replace the diagnostic body of
`gGZLE01_recomp.dll` with the verified personal GZLE01 composite while keeping the proven
Xbox UWP loader, storage and module boundary unchanged.
