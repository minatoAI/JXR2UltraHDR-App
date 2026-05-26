@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d "%~dp0..\ThirdParty\JXR2UltraHDR-lib\build"
cmake --build . --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [build_lib_release] Done.
