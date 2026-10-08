# Platform constraints

These notes describe the compatibility baseline used by the template. PS5
firmware and homebrew loaders vary, so applications must verify their target
environment.

## Loader and runtime

- The application is a native PS5 ELF of type `0xFE10` inside a development
  FSELF.
- FSELF magic `0x1D3D154F` and SDK pair
  `0x02000009 / 0x08050001` are the template defaults.
- The supported title layout includes `sce_module/libc.prx`. The generated
  clean-room shim supplies the loader-visible compatibility contract and
  contains no Sony implementation code.
- The byte-identical generated shim is hardware-verified on firmware 6.02 and
  12.70 with ShadowMountPlus. Other firmware and loader combinations remain
  unverified.
- The shim is not a C library. Application imports bind to platform modules
  selected by the linker.
- The root skeleton keeps `main` alive and relies on the host application
  lifecycle for process closure.

## Filesystem

- The mounted application source is read-only while the app runs; it is
  resolved from `/app0` and known ShadowMountPlus folder mounts.
- Startup first gives a resident Lapy service a bounded opportunity, then
  sends the bundled exact-title one-shot helper to loopback TCP port 9021.
- With filesystem elevation, persistent application data is stored at
  `/data/prosperolichess` and folder installs may update in place.
- `/download0/prosperolichess` remains the sandbox fallback when elevation is
  unavailable; `downloadDataSize` keeps that fallback available.
- Applications should use the sandbox path rather than relying on its host
  backing-file location.
- Availability of `/temp0` and other mounts depends on the loader and title
  environment.
- Native SaveData setup is not used. User state is deliberately separate from
  `/data/homebrew/PPSA99009`, so replacing the app folder preserves it.

## Home-screen presentation

- `icon0.png` is a 512x512 launcher icon.
- `pic0.dds` is the selected-app background and `pic1.dds` is the observed
  launch-transition background. Both are 3840x2160, single-surface,
  `DXGI_FORMAT_BC7_UNORM` DX10 DDS images.
- The package controls `titleName`, the launcher icon, selection/launch
  backgrounds, and optional selection audio.
- Retail-style custom logos and descriptions are online catalog metadata and
  cannot be defined by the supported package-local fields for a synthetic
  homebrew concept.
- Selection audio uses a looped 48 kHz ATRAC9 RIFF file named `snd0.at9`. The
  converter emits stereo 192 kb/s and defaults to 15 seconds.
- The Shell's file-size ceiling for selection audio is exactly 2,097,152 bytes
  (2 MiB); it rejects files starting at 2,097,153 bytes. It has no independent
  duration limit. At the template's stereo 192 kb/s profile, ATRAC9 frame
  granularity makes 87.354666667 seconds (2,096,808 bytes) the largest
  constructible whole-loop file below that ceiling.
- The same validator accepts 48 kHz mono up to 96 kb/s and 48 kHz stereo up to
  192 kb/s. These are documented platform observations, not additional output
  profiles promised by the asset-preparation script.
- Presentation metadata may be cached by the shell or loader. Follow the
  loader's refresh procedure after structural asset changes.
- In `param.json`, category `0` with badge `1` selects Games; category `65536`
  with badge `2` selects Media. Classification does not grant capabilities or
  entitlements.

## Application capabilities

The root skeleton includes notification, basic lifecycle code, CPU-rendered
VideoOut output, and packaged read-only asset access. Filesystem writes,
networking, controller input, AudioOut, and third-party libraries can be added
through public SDK interfaces and explicit build inputs. Start with the focused
[capability recipes](RECIPES.md).

GPU decoding is outside this repository's scope.
