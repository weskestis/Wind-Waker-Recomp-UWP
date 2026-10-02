# Wind-Waker-Recomp-UWP

Personal/unofficial Xbox Series X|S UWP port work for Wind Waker Recomp.

## Current checkpoint: CP1

Branch: `xbox-uwp`

CP1 proves the Xbox/UWP foundation before any Wind Waker game runtime is integrated:

- x64 Universal Windows Platform project
- native `CoreApplication` / `CoreWindow` host
- Direct3D 12 device + CoreWindow swap chain
- continuous present/clear loop
- Xbox controller probe through `Windows.Gaming.Input` (hold **A** to change the clear color)
- persistent UWP LocalSettings launch counter
- public GitHub Actions build
- upstream pinned to Wind Waker Recomp **v0.3.0** at
  `6def7cd4dcd792ba0b4d25ba9883f96debc8f1b3`

See [docs/CP1.md](docs/CP1.md).

## Upstream

This project is based on / interoperates with:

- https://github.com/elliotttate/Wind-Waker-Recomp
- release: v0.3.0
- branch: `windows-release`

The upstream revision is recorded in `UPSTREAM.lock`.

## Game data

No Wind Waker disc image, extracted Nintendo game files, saves, textures, audio, keys, or other game data belong in this repository. The eventual Xbox build will use a bring-your-own-disc import flow.

## Status

CP1 is bring-up only. It does **not** yet contain the Wind Waker recomp game module, audio runtime, disc importer, memory-card implementation, mods, texture packs, or save states.
