@echo off
setlocal EnableExtensions

rem This launcher lives one level above the source project.
set "PROJECT_DIR=%~dp0Spike-Recorder-master"
set "APP_DIR=%PROJECT_DIR%\win\build"
set "APP_EXE=%APP_DIR%\SpikeRecorder.exe"

if not exist "%PROJECT_DIR%\build_windows.bat" (
    echo ERROR: Project folder was not found:
    echo   "%PROJECT_DIR%"
    pause
    exit /b 1
)

rem Apply a completed update before checking inputs or starting the program.
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\tools\install-build.ps1"
if errorlevel 1 (
    echo Close the existing Spike Recorder window, then run this BAT again.
    pause
    exit /b 1
)

rem Compare build inputs; generated object files must not cause rebuilds.
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\tools\build-state.ps1" -Mode Check
if errorlevel 1 (
    echo Preparing the latest version. Please wait...
    call "%PROJECT_DIR%\build_windows.bat"
    if errorlevel 1 (
        echo ERROR: Build failed. See the message above.
        pause
        exit /b 1
    )
)

rem A build may have finished while the old application was still open.
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\tools\install-build.ps1"
if errorlevel 1 (
    echo Close the existing Spike Recorder window, then run this BAT again.
    pause
    exit /b 1
)

if not exist "%APP_EXE%" (
    echo ERROR: The executable was not created:
    echo   "%APP_EXE%"
    pause
    exit /b 1
)

set "LOTTOL_SCORE_OUTBOX=%~dp0server\outbox"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cloud\manage-sync.ps1" -Mode Start
if errorlevel 1 echo Cloud sender unavailable. Game records remain queued for the next successful connection.
start "Spike Recorder" /D "%APP_DIR%" "%APP_EXE%"
exit /b 0
