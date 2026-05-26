# JXR2UltraHDR-App

将 HDR 格式的 JPEG XR（.jxr / .wdp）图像转换为 Ultra HDR JPEG（.jpg）的 WinUI 3 桌面 GUI 工具。

## 系统要求

- Windows 10 2004+（build 19041）或 Windows 11
- Visual Studio 2022，需安装以下工作负载：
  - **使用 C++ 的桌面开发**
  - **通用 Windows 平台 (UWP) 开发**（用于 MSIX 打包支持）
- Windows App SDK 2.1.3（通过 NuGet 自动恢复）
- CMake 3.20+（用于编译核心转换库）
- Git
- NuGet CLI — 从 [nuget.org/downloads](https://www.nuget.org/downloads) 下载 `nuget.exe` 并加入 PATH（VS 2022 不再自带）

## 仓库结构

```
JXR2UltraHDR-App/
├── JXR2UltraHDR.WinUI/       ← WinUI 3 GUI 应用
├── scripts/                   ← 构建脚本
├── ThirdParty/
│   └── JXR2UltraHDR-lib/     ← 核心转换库（子模块）
│       ├── src/api            ← ConverterAPI（C 语言 ABI）
│       ├── src/pipeline       ← Converter, JXRDecoder, UltraHDREncoder
│       ├── src/common         ← 共享类型、错误码
│       └── ThirdParty/
│           ├── jxrlib/        ← JPEG XR 编解码器
│           └── libultrahdr/   ← Ultra HDR 编解码器
```

## 构建步骤

### 一键构建

打开 **VS 2022 开发者命令提示符（Developer Command Prompt）**，执行：

```cmd
cd \path\to\JXR2UltraHDR-App
scripts\build_app.bat
```

该脚本依次完成：编译核心库（CMake）→ 恢复 NuGet 包 → 编译 WinUI 应用（MSBuild）。

### 手动分步构建

```cmd
REM 1. 编译核心转换库
cd ThirdParty\JXR2UltraHDR-lib
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
cmake --build . --config Release

REM 2. 恢复 NuGet 包
cd ..\..\..
nuget restore JXR2UltraHDR.sln

REM 3. 编译 WinUI 应用
msbuild JXR2UltraHDR.sln /p:Configuration=Release /p:Platform=x64
```

### 构建产物

- MSIX 安装包：`AppPackages\JXR2UltraHDR.WinUI\`
- 核心库：`ThirdParty\JXR2UltraHDR-lib\build\Release\`

## MSIX 签名

编译时如果项目目录（`JXR2UltraHDR.WinUI\`）下存在 `JXR2UltraHDR_devcert.pfx` 证书文件，会自动对 MSIX 包签名。

### 1. 生成自签名证书

以**管理员身份**运行 PowerShell：

```powershell
# 在 CurrentUser\My 存储中创建证书
$cert = New-SelfSignedCertificate -Type Custom -Subject "CN=JXR2UltraHDR" -KeyUsage DigitalSignature `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
  -CertStoreLocation "Cert:\CurrentUser\My"

# 导出为 PFX（密码：password123）
$cert | Export-PfxCertificate -FilePath JXR2UltraHDR_devcert.pfx -Password (ConvertTo-SecureString "password123" -AsPlainText -Force)
```

将导出的 `JXR2UltraHDR_devcert.pfx` 放入 `JXR2UltraHDR.WinUI\` 目录。

### 2. 安装证书（旁加载用）

在每台需要安装 MSIX 的机器上，以**管理员身份**运行 PowerShell：

```powershell
# 导入证书到 LocalMachine\TrustedPeople（MSIX 信任必需）
Import-PfxCertificate -FilePath JXR2UltraHDR_devcert.pfx `
  -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" `
  -Password (ConvertTo-SecureString "password123" -AsPlainText -Force)
```

不执行此步骤，Windows 安装 MSIX 时会提示"证书不受信任"。

### 3. 删除证书（不再需要时）

```powershell
# 从 LocalMachine\TrustedPeople 移除
Get-ChildItem "Cert:\LocalMachine\TrustedPeople" | Where-Object { $_.Subject -eq "CN=JXR2UltraHDR" } | Remove-Item

# 从 CurrentUser\My 移除（如果是在本机生成的）
Get-ChildItem "Cert:\CurrentUser\My" | Where-Object { $_.Subject -eq "CN=JXR2UltraHDR" } | Remove-Item
```

## 克隆

```cmd
git clone --recursive git@github.com:minatoAI/JXR2UltraHDR-App.git
```

如果已克隆但忘了加 `--recursive`：

```cmd
git submodule update --init --recursive
```

## 常见问题与排查

### Clone / 子模块

**子模块未检出**：克隆时没加 `--recursive`，运行 `git submodule update --init --recursive`。

**克隆或子模块拉取缓慢**：部分网络环境访问 GitHub 较慢，设置代理后重试：

```cmd
set http_proxy=http://your-proxy:port
set https_proxy=http://your-proxy:port
git clone --recursive git@github.com:minatoAI/JXR2UltraHDR-App.git
```

### NuGet

**'nuget' 不是可识别的命令**：VS 2022 不再自带 `nuget.exe`。从 [nuget.org/downloads](https://www.nuget.org/downloads) 下载后放入 PATH。

**缺少 NuGet 程序包**：确保在解决方案根目录（`nuget.config` 所在目录）运行 restore：

```cmd
cd \path\to\JXR2UltraHDR-App
nuget restore JXR2UltraHDR.sln
```

项目级的 `nuget.config` 设置了 `repositoryPath=JXR2UltraHDR.WinUI\packages`，包会自动恢复到正确的路径。

**NuGet restore 慢**：首次运行会下载约 16 个包（~150MB），属正常现象。如需代理，提前设置 `http_proxy` / `https_proxy`。

### CRT 运行时库不匹配（LNK2038）

**症状**：链接错误 `LNK2038: RuntimeLibrary mismatch — MT_StaticRelease vs MD_DynamicRelease`，涉及 `uhdr-static.lib`。

**原因**：libultrahdr 在静态库模式下强制使用 `/MT`（静态 CRT），而 WinUI 项目需要 `/MD`（动态 CRT）。其 CMakeLists.txt 第 211 行是问题根源。

**解决方法**（已在 JXR2UltraHDR-lib 的 CMakeLists.txt 中修复）：在 `add_subdirectory` 之后覆盖 `uhdr-static`、`core`、`image_io` 三个目标的 CRT 设置（仅在 `BUILD_FOR_WINUI=ON` 时生效）。如果你遇到了此问题，请确保 CMake 配置时传入了 `-DBUILD_FOR_WINUI=ON`。

### LNK4098 "default library 'LIBCMT' conflicts"

**症状**：编译 WinUI 时出现链接器警告 LNK4098。

**原因**：libjpeg-turbo（通过 FetchContent 编译）使用 `/MT`，而 WinUI 项目使用 `/MD`。由于 turbojpeg 是经过 `uhdr-static` 链接的，冲突被限制在库内部，不影响最终可执行文件。

**解决方法**：无需处理，此警告无害。如果在意，安装 NASM 后重新编译即可。

### 首次编译核心库需要代理

libultrahdr 会在首次 CMake 配置时通过 `FetchContent` 拉取 libjpeg-turbo（git clone）。如需代理：

```cmd
set http_proxy=http://your-proxy-address:port
set https_proxy=http://your-proxy-address:port
```

然后在运行 `scripts\build_lib.bat`。代理仅在首次需要（turbojpeg 缓存到本地构建目录后即不再需要）。

### NASM 未找到（TurboJPEG 无 SIMD 加速）

**症状**：CMake 警告 `NASM compiler not found — SIMD extensions disabled`。

**影响**：JPEG 编解码性能下降约 20-30%，不影响正确性。

**解决方法**：安装 [NASM](https://nasm.us) 并加入 PATH，然后重新编译：

```cmd
cd ThirdParty\JXR2UltraHDR-lib\build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
cmake --build . --config Release
```

注意：turbojpeg 通过 FetchContent 的 ExternalProject 编译，只在它自己的 CMake 配置阶段检测 NASM。如果初次编译时 NASM 未安装，后续即使安装了 NASM 也需要清理构建目录才能重新检测到。

### 代码页警告 C4819 / C4828

**症状**：编译时出现 `C4819`（"文件包含不能在当前代码页中表示的字符"）和 `C4828`（"文件包含在源字符集中无效的字符"）警告。

**原因**：微软原版 JXR 参考实现的头文件（`guiddef.h`、`JXRGlue.h` 等）包含非 UTF-8 编码的字符（版权符号等）。这些是第三方文件，不可修改。

**影响**：无害，不影响编译结果和运行时行为。

### MSIX 签名失败 / 证书缺失

**症状**：MSIX 包已生成但未签名。

**原因**：vcxproj 中的 `SignMsiXPackage` Target 仅在 `JXR2UltraHDR_devcert.pfx` 文件存在时执行。无证书时自动跳过（不是错误）。

**解决方法**：生成自签名证书（见上方"MSIX 签名"章节），放入 `JXR2UltraHDR.WinUI\` 目录。

### 编译失败："MSB8020: v143 build tools not found"

**症状**：MSBuild 报错找不到 v143 平台工具集。

**原因**：vcxproj 目标为 `v143`（VS 2022），使用旧版 MSVC 编译会失败。

**解决方法**：一定要在 **VS 2022 开发者命令提示符**（而非 VS 2019 或普通 cmd.exe）中执行构建。

## API

WinUI 应用通过纯 C ABI 与核心库交互（`ConverterAPI.h` / `ConverterAPI.cpp`）：

```c
int Converter_ConvertFile(const wchar_t* inputPath, const wchar_t* outputPath,
    int sdrQuality, int gainMapQuality,
    Converter_ProgressCallback callback, void* userData);
void Converter_Cancel(void);
int Converter_GetImageInfo(const wchar_t* filePath, ConverterImageInfo* info);
```

完整 API 文档见 `ThirdParty/JXR2UltraHDR-lib/src/api/ConverterAPI.h`。
