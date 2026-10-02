# CP3 — LocalState persistence and recomp module loading

CP1 and CP2 were both confirmed on a physical Xbox Series X.

CP3 tests the two platform seams required before the real Wind Waker module can
be introduced: persistent app-container storage and loading a packaged native
recomp module through the real StaticRecomp ABI.

## Storage

The app passes `ApplicationData.Current.LocalFolder.Path` to the unmodified
upstream `ipl_sram.c` implementation. The probe uses a diagnostic
`cp3-sram-test.bin` file in LocalState, performs an EXI SRAM write through
the upstream device model, reloads the file, and verifies the byte.

The expected marker is also recorded in LocalSettings. On the next launch,
the marker loaded from the SRAM file must match the value saved by the prior
launch. This proves persistence across process restarts.

This is a diagnostic SRAM file only. The eventual game uses its own
`sram.bin` and `GZLE01.card`.

## Module loading

`CP3Module.dll` is an x64 UWP-compatible DLL packaged inside the MSIX.
It exports the exact RecompCore ABI symbol:

`staticrecomp_get_module`

The returned `StaticRecompModuleDesc` uses the pinned RecompCore
`StaticRecompABI.h`, `CPUState`, ABI version, and CPU ABI version.

The UWP host loads it with `LoadPackagedLibrary`, resolves the exported
symbol with `GetProcAddress`, validates the ABI, and dispatches one test
guest address.

CI also unpacks the finished MSIX and fails if `CP3Module.dll` is absent.

## Xbox screen codes

- dark blue: CP2 runtime + CP3 storage + CP3 module loading all passed
- red: CP2 runtime slice failed
- orange: LocalState / SRAM persistence failed
- purple: packaged recomp module load / ABI / dispatch failed
- hold A: green controller confirmation

Launch CP3 twice. The second launch adds the cross-process LocalState
persistence check.
