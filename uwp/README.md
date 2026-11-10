# Tsugaru for Xbox (UWP): FM Towns emulator, universal frontend

An experimental Xbox (dev-mode UWP / MSIX) port of the TOWNSEMU FM Towns emulator, with the universal frontend
(derived from SwanStationPS5, GPL-3). This folder holds the UWP app and the frontend; the emulator sources are in
`src/` and the upstream files they change are in the same branch.

## Status

- Built with the Visual Studio 2022 WindowsStore toolchain, x64.
- Tested on a Windows 10 PC with a CD image supplied locally (not included here).
- **Not yet verified on an Xbox console.**

## Not included in this repository

- The FM Towns BIOS (`FMT_*.ROM`). Supply your own dumps. `uwp/tools/package.ps1` copies them from a folder
  you choose; that folder is hard-coded to a local path, so edit it before use.
- Disc and floppy test images and the LaunchBox metadata. Download and generate the art index yourself with
  `uwp/tools/make_towns_art.ps1`.
- Third-party art and cheat index lists (`*-index.txt`, `*-urls.txt`, `chtdb*`). Regenerate them the same way.
- Flamenco One artwork and reference code. The frontend's own credits in the source files are kept.
- PlayStation artwork and sound (`psx_*`, `psxdiscsound.wav`, `sbi_files/`).
- Signing certificates and built packages.

`package.ps1` also expects `swanui\assets\flamenco-emblem.png` as a required file. That file is not included, so
remove that line from the check list before you run it.

## Build

```
cmake -S uwp -B build -G "Visual Studio 17 2022" -DCMAKE_SYSTEM_NAME=WindowsStore ^
      -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DCMAKE_SYSTEM_PROCESSOR=AMD64
cmake --build build --config Release --target tsugaru_uwp
```

The CMake project in `uwp/` sets `TSUGARU_UWP` itself and builds the `tsugaru_uwp` target. Packaging is done by
`uwp/tools/package.ps1`, and the manifest template is in `uwp/package/`.

## Licences

- Emulator sources: see `LICENSE` at the root of this repository.
- Frontend (`uwp/swanui`, `uwp/app`, `uwp/bridge`): GPL-3.0, derived from SwanStationPS5. The licence text is in
  `uwp/COPYING-GPL-3.txt`. The original notices in the source files are kept.
- Fonts: see the licence files in `uwp/swanui/assets/fonts/`.
