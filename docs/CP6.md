# CP6 — Dawn CoreWindow and XAudio2

CP1 through CP5 were confirmed on a physical Xbox Series X.

CP6 proves the two platform services needed before the real Aurora backend can
replace the diagnostic renderer: WebGPU/Dawn presentation directly against the
UWP CoreWindow and native UWP/Xbox audio through XAudio2.

## Dawn pin

CP6 uses the same Dawn release pinned by the upstream Aurora source:

- tag: `v20260618.032059`
- source commit: `266c1cf8de969a364afa4fa49311631fc99a881e`
- package: `dawn-windows-amd64.tar.gz`
- SHA-256: `d26d3107142c5a123d2102979c7403308668038372616fbd33a537e45c22f0dc`

CI verifies that hash before linking or packaging Dawn.

The package includes `webgpu_dawn.dll`, `dxcompiler.dll`, and `dxil.dll`,
matching Aurora's Windows runtime-DLL contract.

## CoreWindow WebGPU path

The CP6 probe:

1. creates a WebGPU instance,
2. creates a `SurfaceDescriptorFromWindowsCoreWindow` from the real UWP
   `CoreWindow`,
3. requests a high-performance D3D12 adapter compatible with that surface,
4. creates a Dawn device/queue,
5. queries surface capabilities,
6. configures the surface,
7. acquires the current surface texture,
8. records and submits a WebGPU render pass,
9. clears/presents one frame through Dawn,
10. unconfigures/releases the surface.

The long-lived CP1 D3D12 diagnostic swap chain starts only after the Dawn probe
has released its surface. This avoids competing swap chains on one CoreWindow.

## XAudio2 path

CP6 uses the operating-system XAudio2 2.9 API available to UWP/Xbox:

- creates an XAudio2 engine,
- creates a mastering voice with `AudioCategory_GameMedia`,
- creates a 32 kHz stereo PCM source voice,
- submits a short low-volume 440 Hz startup tone,
- starts playback.

The eventual GXRuntime audio sink can feed this same 32/48 kHz voice path
instead of Aurora's desktop SDL3 audio stream.

## CI contracts

CI verifies:

- CP5 full GXRuntime chassis still builds,
- production `gGZLE01_recomp.dll` exports remain intact,
- `BlueWakeUWP.exe` imports `webgpu_dawn.dll` and `xaudio2_9.dll`,
- Dawn/DXC runtime DLLs are physically inside the finished MSIX.

## Xbox screen codes

- dark blue: all tests through CP6 passed
- red: CP2 runtime slice failed
- orange: LocalState/SRAM persistence failed
- purple: CP3 generic module loading failed
- yellow/gold: CP4 production module contract failed
- cyan/teal: CP5 full GXRuntime chassis failed
- pink: CP6 Dawn/CoreWindow/D3D12 probe failed
- gray: CP6 XAudio2 probe failed
- hold A: green controller confirmation

A successful CP6 launch should also play one short tone.

No Nintendo disc data, extracted files, generated translated game source, or
saves are stored in this public repository.
