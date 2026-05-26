@echo off
REM Build the JXR2UltraHDR WinUI application
REM Prerequisites: Visual Studio 2022, CMake, Git, NuGet

setlocal
cd /d "%~dp0.."

REM Step 1: Build core library
echo [build_app] Building core library ...
call scripts\build_lib.bat
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Step 2: Restore NuGet packages
echo [build_app] Restoring NuGet packages ...
nuget restore JXR2UltraHDR.sln
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Step 3: Build WinUI app
echo [build_app] Building WinUI application (Release, x64) ...
msbuild JXR2UltraHDR.sln /p:Configuration=Release /p:Platform=x64
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [build_app] Done. See AppPackages\ for the MSIX installer.
endlocal
