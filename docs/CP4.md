# CP4 — production recomp module contract

CP1, CP2 and CP3 were confirmed on a physical Xbox Series X.

CP4 changes the module test from a generic probe DLL to the same production
shape used by Wind Waker Recomp's Windows build.

## Production module name

The package now contains:

`gGZLE01_recomp.dll`

That is the exact module name emitted by the upstream Windows composite build.

## Real RecompCore runtime inside the DLL

The CP4 module links the pinned RecompCore GXRuntime CPU implementation:

- `cpu.c`
- `cpu_exception.c`
- `cpu_interpreter.c`
- `cpu_interpreter_table.c`
- `cpu_interpreter_float.c`
- `cpu_interpreter_integer.c`

The translated game chunks are still replaced by one tiny diagnostic dispatch.
No Nintendo game data or generated game source is committed.

The point of CP4 is to prove the production DLL/runtime boundary before the
personal generated chunks are introduced.

## Contract exercised on Xbox

The UWP host loads `gGZLE01_recomp.dll` with `LoadPackagedLibrary` and checks:

- `staticrecomp_get_module`
- StaticRecomp ABI version and CPU ABI version
- game ID `GZLE01`
- fixed `CPUState` export
- fixed 32 MiB MEM1 export
- GXRuntime guest-alias clear/add/get/resolve/remove
- host memory-write journal callback across the DLL boundary
- host edge-service callback across the DLL boundary
- one charged native dispatch
- a real big-endian write into exported MEM1

CI also checks those exports with `dumpbin` and unpacks the finished MSIX to
prove `gGZLE01_recomp.dll` is actually deployed.

## Xbox screen codes

- dark blue: all CP2/CP3/CP4 tests passed
- red: CP2 runtime slice failed
- orange: LocalState/SRAM persistence failed
- purple: CP3 generic module loading failed
- yellow/gold: CP4 production module contract failed
- hold A: green controller confirmation

## Next

Once CP4 is proven on Series X, the production host/module boundary is fixed.
The remaining game-module work is to compile the user's personal verified
GZLE01 composite source for the same UWP/AppContainer target and drop that DLL
into this already-proven package contract. The public repository will continue
to contain no disc image, extracted game files, or generated game source.
