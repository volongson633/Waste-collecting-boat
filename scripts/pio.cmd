@echo off
setlocal
set "PLATFORMIO_CORE_DIR=%~dp0..\.pio-core"
set "PLATFORMIO_SETTING_ENABLE_TELEMETRY=No"
if not defined PLATFORMIO_BUILD_DIR set "PLATFORMIO_BUILD_DIR=%~dp0..\.pio\verified-build"
if not defined PLATFORMIO_LIBDEPS_DIR set "PLATFORMIO_LIBDEPS_DIR=%~dp0..\.pio\verified-libdeps"
if exist "%~dp0..\.tools\python\python.exe" (
    "%~dp0..\.tools\python\python.exe" -m platformio %*
) else (
    pio %*
)
exit /b %errorlevel%
