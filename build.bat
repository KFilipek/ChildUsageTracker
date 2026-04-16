@echo off
setlocal

set "VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
set "VCVARS64=%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_DIR=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

if not exist "%VCVARS64%" goto :no_vs
goto :vs_found
:no_vs
echo ERROR: VS 2019 BuildTools not found.
echo Expected: %VS_ROOT%
exit /b 1
:vs_found

echo Initializing VS2019 x64 environment...
set VSCMD_SKIP_SENDTELEMETRY=1
call "%VCVARS64%"
where cl.exe >nul 2>&1
if errorlevel 1 goto :vcvars_fail
goto :vcvars_ok
:vcvars_fail
echo ERROR: Failed to initialize VS2019 environment (cl.exe not found).
exit /b 1
:vcvars_ok

set "PATH=%NINJA_DIR%;%PATH%"

echo.
echo Configuring with CMake (Ninja / Release)...
"%CMAKE%" -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :cmake_fail
goto :cmake_ok
:cmake_fail
echo ERROR: CMake configuration failed.
exit /b 1
:cmake_ok

echo.
echo Building...
"%CMAKE%" --build build
if errorlevel 1 goto :build_fail
goto :build_ok
:build_fail
echo ERROR: Build failed.
exit /b 1
:build_ok

echo.
echo ============================================================
echo  Build successful!
echo  Output: build\ChildUsageTracker.exe
echo ============================================================
echo.
echo NEXT STEPS:
echo   1. Edit config.ini - set your GitHub PAT (token=...)
echo   2. Copy config.ini next to ChildUsageTracker.exe
echo   3. Run ChildUsageTracker.exe  - starts silently in background
echo   4. Run ChildUsageTracker.exe /install  - auto-starts on Windows login
echo   5. Run ChildUsageTracker.exe /uninstall - removes auto-start
echo.
