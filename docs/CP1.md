# CP1 — Xbox UWP bring-up

Goal: prove the Xbox/UWP host foundation before integrating the Wind Waker recomp runtime.

## Pass criteria

- x64 UWP project builds on a public GitHub-hosted Windows runner.
- Package manifest targets Windows.Universal.
- Native CoreApplication host starts without XAML.
- Direct3D 12 device, queue, swap chain, RTVs, command list and fence are created.
- The app clears/presents continuously through a CoreWindow swap chain.
- An Xbox gamepad is visible through Windows.Gaming.Input; holding A changes the clear color.
- ApplicationData LocalSettings survives launches.
- Upstream is pinned to Wind Waker Recomp v0.3.0 commit 6def7cd4dcd792ba0b4d25ba9883f96debc8f1b3.
- No Nintendo game data is stored in this repository.

## Not in CP1

The recomp game module, DSP/audio, disc import, memory-card saves, Dawn/Aurora integration,
mods, texture packs, save states and release signing are deliberately deferred until the host
and CI toolchain are proven.

## Next

CP2 will replace the standalone D3D12 probe with the pinned upstream host/runtime pieces,
starting with platform storage and presentation adapters.
