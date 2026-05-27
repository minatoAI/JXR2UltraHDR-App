@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d "C:\Users\20506\source\repos\JXR2UltraHDR-App\ThirdParty\JXR2UltraHDR-lib"
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_FOR_WINUI=ON
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
cmake --build . --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [build_lib] Done.
