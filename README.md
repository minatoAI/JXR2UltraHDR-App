# JXR2UltraHDR-App

WinUI 3 desktop GUI for converting HDR-format JXR/WDP images to Ultra HDR JPEG.

## Prerequisites

- Windows 10 2004+ (build 19041) or Windows 11
- Visual Studio 2022 with the following workloads:
  - **Desktop development with C++**
  - **Universal Windows Platform (UWP) development** (for MSIX packaging support)
- Windows App SDK 2.1.3 (restored via NuGet)
- CMake 3.20+ (for building the core library)
- Git
|- NuGet CLI — download `nuget.exe` from [nuget.org/downloads](https://www.nuget.org/downloads) and place it in PATH (not bundled with VS 2022)

## Repository Structure

```
JXR2UltraHDR-App/
├── JXR2UltraHDR.WinUI/       ← WinUI 3 GUI application
├── scripts/                   ← Build scripts
├── ThirdParty/
│   └── JXR2UltraHDR-lib/     ← Core conversion library (submodule)
│       ├── src/api            ← ConverterAPI (C ABI)
│       ├── src/pipeline       ← Converter, JXRDecoder, UltraHDREncoder
│       ├── src/common         ← Shared types, error codes
│       └── ThirdParty/
│           ├── jxrlib/        ← JPEG XR codec
│           └── libultrahdr/   ← Ultra HDR codec
```

## Building

### Quick Build (one-shot)

Open a **Developer Command Prompt for VS 2022** and run:

```cmd
cd \path\to\JXR2UltraHDR-App
scripts\build_app.bat
```

This builds the core library (CMake), restores NuGet packages, then builds the WinUI app (MSBuild).

### Manual Step-by-Step

```cmd
REM 1. Build core library
cd ThirdParty\JXR2UltraHDR-lib
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
cmake --build . --config Release

REM 2. Restore NuGet packages
cd ..\..\..
nuget restore JXR2UltraHDR.sln

REM 3. Build WinUI app
msbuild JXR2UltraHDR.sln /p:Configuration=Release /p:Platform=x64
```

### Output

- MSIX installer: `AppPackages\JXR2UltraHDR.WinUI\`
- Core library: `ThirdParty\JXR2UltraHDR-lib\build\Release\`

## MSIX Signing

The build target signs the MSIX package if a certificate file `JXR2UltraHDR_devcert.pfx` is present in the project root (`JXR2UltraHDR.WinUI\`). To generate a self-signed certificate:

```powershell
New-SelfSignedCertificate -Type Custom -Subject "CN=JXR2UltraHDR" -KeyUsage DigitalSignature `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
  -CertStoreLocation "Cert:\CurrentUser\My" | Format-List Subject, Thumbprint
```

Then export to `JXR2UltraHDR_devcert.pfx` with password `password123`.

## Cloning

```cmd
git clone --recursive git@github.com:minatoAI/JXR2UltraHDR-App.git
```

If you already cloned without `--recursive`:

```cmd
git submodule update --init --recursive
```

## Known Issues & Troubleshooting

### CRT RuntimeLibrary Mismatch (LNK2038)

**Symptom**: Linker error `LNK2038: RuntimeLibrary mismatch — MT_StaticRelease vs MD_DynamicRelease` in `uhdr-static.lib`.

**Cause**: libultrahdr forces `/MT` (static CRT) for static library builds on MSVC. The WinUI project requires `/MD` (dynamic CRT).

**Fix** (already applied in JXR2UltraHDR-lib): CMakeLists.txt overrides the CRT for `uhdr-static`, `core`, and `image_io` targets after `add_subdirectory` when `BUILD_FOR_WINUI=ON`. If you encounter this in a fresh clone, ensure you're running the CMake build with `-DBUILD_FOR_WINUI=ON`.

### Proxy Required for First Build

libultrahdr fetches libjpeg-turbo via CMake `FetchContent` (git clone) during the first CMake configure. If you need a proxy:

```cmd
set http_proxy=http://your-proxy-address:port
set https_proxy=http://your-proxy-address:port
```

before running `scripts\build_lib.bat`. The proxy is only needed once (turbojpeg is cached locally in the build tree). NuGet restore may also need these if your network requires it.

### NuGet Packages Path

`nuget restore` places packages in the project directory (`JXR2UltraHDR.WinUI\packages\`) automatically because the project includes a `nuget.config` with `repositoryPath=packages`. If you run restore from a different location, cd to the solution root first:

```cmd
cd \path\to\JXR2UltraHDR-App
nuget restore JXR2UltraHDR.sln
```

### NASM Not Found (TurboJPEG without SIMD)

**Symptom**: CMake warning `NASM compiler not found — SIMD extensions disabled`.

**Impact**: JPEG encoding/decoding performance is ~20-30% lower without SIMD. This does **not** affect correctness.

**Fix**: Install NASM (https://nasm.us) and add it to PATH before rebuilding the core library:

```cmd
cmake --build ThirdParty\JXR2UltraHDR-lib\build --config Release
```

(The turbojpeg ExternalProject won't re-detect NASM unless its build directory is cleaned.)

### Warning C4819 / C4828 (Code Page)

jxrlib headers contain non-UTF-8 characters that trigger code-page warnings on Chinese/Japanese Windows. These are harmless and originate from Microsoft's original JXR reference implementation.

## API

The WinUI app communicates with the core library through a plain C ABI (`ConverterAPI.h` / `ConverterAPI.cpp`):

```c
int Converter_ConvertFile(const wchar_t* inputPath, const wchar_t* outputPath,
    int sdrQuality, int gainMapQuality,
    Converter_ProgressCallback callback, void* userData);
void Converter_Cancel(void);
int Converter_GetImageInfo(const wchar_t* filePath, ConverterImageInfo* info);
```

See `ThirdParty/JXR2UltraHDR-lib/src/api/ConverterAPI.h` for full API docs.
