@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d "%~dp0.."
msbuild JXR2UltraHDR.sln /p:Configuration=Release /p:Platform=x64 /p:AppxPackage=true
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [build_release] Done.
