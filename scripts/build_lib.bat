@echo off
REM Build the core conversion library via CMake
REM Prerequisites: Visual Studio 2022, CMake, Git

setlocal
cd /d "%~dp0..\ThirdParty\JXR2UltraHDR-lib"

if not exist build mkdir build
cd build

REM Configure with WinUI CRT (/MD)
echo [build_lib] Configuring CMake (x64, Release, BUILD_FOR_WINUI=ON) ...
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Build all targets
echo [build_lib] Building Release ...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [build_lib] Done. Libraries at:
echo   ThirdParty\JXR2UltraHDR-lib\build\Release\
echo   ThirdParty\JXR2UltraHDR-lib\build\uhdr_build\Release\
endlocal
