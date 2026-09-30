@echo off
setlocal EnableExtensions

REM Run this file from the root of the ESP32BusDash repository.
REM It safely fast-forwards from GitHub, builds, and uploads to COM6.
cd /d "%~dp0"

set "PIO=pio"
where pio >nul 2>&1
if not errorlevel 1 goto :pio_ready

REM PlatformIO Core's standard per-user Windows installation location.
if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" (
  set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
  goto :pio_ready
)

echo [ERROR] PlatformIO Core was not found.
echo Open this project in VS Code with the PlatformIO IDE extension installed,
echo then run this file from PlatformIO's terminal, or add pio.exe to PATH.
goto :failed

:pio_ready
where git >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Git was not found on PATH.
  echo Install Git for Windows, then run this file again.
  goto :failed
)

echo.
echo [1/2] Updating ESP32BusDash from GitHub...
git pull --ff-only origin main
if errorlevel 1 (
  echo [ERROR] Git update failed. Resolve any local changes, then retry.
  goto :failed
)

echo.
echo [2/2] Building and flashing COM6...
"%PIO%" run -e esp32-s3-cyd35 -t upload --upload-port COM6
if errorlevel 1 (
  echo [ERROR] Upload failed.
  echo Put the board into download mode: hold BOOT, tap RESET, release BOOT, then retry.
  goto :failed
)

echo.
echo [OK] Firmware was built and uploaded to COM6.
echo To view the startup diagnostics, run:
echo   "%PIO%" device monitor -p COM6 -b 115200
pause
exit /b 0

:failed
pause
exit /b 1
