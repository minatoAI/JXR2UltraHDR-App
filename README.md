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

The build target signs the MSIX package if a certificate file `JXR2UltraHDR_devcert.pfx` is present in the project root (`JXR2UltraHDR.WinUI\\`).

### 1. Generate a Self-Signed Certificate

Run in PowerShell as **administrator**:

```powershell
# Create certificate in CurrentUser\My store
$cert = New-SelfSignedCertificate -Type Custom -Subject "CN=JXR2UltraHDR" -KeyUsage DigitalSignature `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
  -CertStoreLocation "Cert:\CurrentUser\My"

# Export to PFX (password: password123)
$cert | Export-PfxCertificate -FilePath JXR2UltraHDR_devcert.pfx -Password (ConvertTo-SecureString "password123" -AsPlainText -Force)
```

Place the exported `JXR2UltraHDR_devcert.pfx` in the `JXR2UltraHDR.WinUI\` directory.

### 2. Install Certificate for Sideloading

On each machine that will install the MSIX, run PowerShell as **administrator**:

```powershell
# Import the certificate to LocalMachine\TrustedPeople (required for MSIX trust)
Import-PfxCertificate -FilePath JXR2UltraHDR_devcert.pfx `
  -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" `
  -Password (ConvertTo-SecureString "password123" -AsPlainText -Force)
```

Without this step, Windows will show a "certificate not trusted" warning when installing the MSIX.

### 3. Verify Signature & Install Certificate (optional)

After building or downloading the signed MSIX, you can verify and optionally install the certificate through the file's Properties:

**GUI — via File Properties**
1. Right-click the `.msix` file → **Properties**
2. Go to the **Digital Signatures** tab
3. Select the signature entry and click **Details**
4. In the Digital Signature Details dialog, click **View Certificate**
5. In the Certificate dialog, click **Install Certificate...**
6. Certificate Import Wizard opens:
   - Choose **Local Machine** → Next (requires admin)
   - Choose **Place all certificates in the following store**
   - Click **Browse** → select **Trusted People** → OK → Next → Finish
7. The certificate is now trusted on this machine — the MSIX will install without warnings

> Alternatively, use the PowerShell method on p.2 (MSIX Signing → Install Certificate for Sideloading).

**CLI — signtool verify**
```cmd
signtool verify /pa AppPackages\JXR2UltraHDR.WinUI\JXR2UltraHDR.WinUI_1.0.0.0_x64_Test\JXR2UltraHDR.WinUI_1.0.0.0_x64.msix
```

Note: For a self-signed certificate not yet installed to a trusted store, `signtool verify` will report "not trusted by the trust provider". This is expected — the MSIX is still signed and installable.

### 4. Remove Certificate (when no longer needed)

```powershell
# Remove from LocalMachine\TrustedPeople
Get-ChildItem "Cert:\LocalMachine\TrustedPeople" | Where-Object { $_.Subject -eq "CN=JXR2UltraHDR" } | Remove-Item

# Remove from CurrentUser\My (if you generated it on this machine)
Get-ChildItem "Cert:\CurrentUser\My" | Where-Object { $_.Subject -eq "CN=JXR2UltraHDR" } | Remove-Item
```

## Cloning

```cmd
git clone --recursive git@github.com:minatoAI/JXR2UltraHDR-App.git
```

If you already cloned without `--recursive`:

```cmd
git submodule update --init --recursive
```

## Known Issues & Troubleshooting

### Clone / Submodule

**Submodule not checked out**: If you cloned without `--recursive`, run:

```cmd
git submodule update --init --recursive
```

**Slow clone or submodule fetch**: GitHub access in some regions is slow. Set a proxy and retry:

```cmd
set http_proxy=http://your-proxy:port
set https_proxy=http://your-proxy:port
git clone --recursive git@github.com:minatoAI/JXR2UltraHDR-App.git
```

### NuGet

**'nuget' is not recognized as a command**: Download `nuget.exe` from [nuget.org/downloads](https://www.nuget.org/downloads) and place it in your PATH, or copy it into the solution root.

**NuGet restore fails / "缺少 NuGet 程序包"**: Ensure you run `nuget restore` from the solution root (where `nuget.config` is located):

```cmd
cd \path\to\JXR2UltraHDR-App
nuget restore JXR2UltraHDR.sln
```

The project's `nuget.config` sets `repositoryPath=JXR2UltraHDR.WinUI\packages`, so packages go to the expected location.

**NuGet restore slow**: This is normal on first run — NuGet downloads ~16 packages (~150MB total). If behind a proxy, set `http_proxy` / `https_proxy` before running restore.

### CRT RuntimeLibrary Mismatch (LNK2038)

**Symptom**: Linker error `LNK2038: RuntimeLibrary mismatch — MT_StaticRelease vs MD_DynamicRelease` in `uhdr-static.lib`.

**Cause**: libultrahdr forces `/MT` (static CRT) for static library builds on MSVC (line 211 of its CMakeLists.txt). The WinUI project requires `/MD` (dynamic CRT).

**Fix** (already applied in JXR2UltraHDR-lib `CMakeLists.txt`): After `add_subdirectory`, the CRT is overridden for `uhdr-static`, `core`, and `image_io` targets when `BUILD_FOR_WINUI=ON`. If you encounter this, ensure you run CMake with `-DBUILD_FOR_WINUI=ON`.

### LNK4098 "default library 'LIBCMT' conflicts"

**Symptom**: Linker warning `LNK4098` during WinUI build.

**Fix**: None needed. This warning is harmless and does not affect the binary.

### Proxy Required for Core Library First Build

libultrahdr downloads libjpeg-turbo via CMake `ExternalProject_Add` (git clone) during the first `cmake --build`. If you need a proxy:

```cmd
set http_proxy=http://your-proxy-address:port
set https_proxy=http://your-proxy-address:port
```

before running `scripts\build_lib.bat`. The proxy is only needed once — turbojpeg is cached locally in the build tree for subsequent builds.

### NASM Not Found (TurboJPEG without SIMD)

**Symptom**: CMake warning `NASM compiler not found — SIMD extensions disabled`.

**Impact**: JPEG encoding/decoding performance is ~20-30% lower without SIMD. Correctness is unaffected.

**Fix**: Install NASM (https://nasm.us) and add it to PATH, then rebuild:

```cmd
cd ThirdParty\JXR2UltraHDR-lib\build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
cmake --build . --config Release
```

(Note: turbojpeg is built via `ExternalProject_Add` — it checks for NASM only during its own CMake configure step, so you may need to clean the build directory if NASM wasn't present initially.)

### Warning C4819 / C4828 (Code Page)

**Symptom**: Warnings `C4819` ("file contains characters not representable in current code page") and `C4828` ("file contains character starting at offset ... invalid in current source character set").

**Cause**: Microsoft's original JXR reference implementation headers (`guiddef.h`, `JXRGlue.h`, etc.) contain non-UTF-8 encoded characters (copyright symbols and other bytes). These are third-party files and cannot be modified.

**Impact**: Harmless. The warnings do not affect compilation or runtime behavior.

### MSIX Signing Fails / Certificate Missing

**Symptom**: MSIX package is built but not signed.

**Cause**: The vcxproj contains a `SignMsiXPackage` target that runs only if `JXR2UltraHDR_devcert.pfx` exists in the project directory. If no certificate is present, the target is skipped (not an error).

**Fix**: Generate a self-signed certificate (see "MSIX Signing" section above) and place it as `JXR2UltraHDR_devcert.pfx` in the `JXR2UltraHDR.WinUI\` directory.

### Build fails with "MSB8020: v143 build tools not found"

**Symptom**: MSBuild error `MSB8020: The build tools for v143 (Platform Toolset = 'v143') cannot be found`.

**Fix**: Ensure you have **Visual Studio 2022** installed with the **Desktop development with C++** workload, and run the build from a **Developer Command Prompt for VS 2022**.

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
