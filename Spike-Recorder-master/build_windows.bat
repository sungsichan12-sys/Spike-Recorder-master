@echo off
setlocal EnableExtensions

rem Build from the repository root, regardless of the current directory.
pushd "%~dp0" || exit /b 1

set "MINGW_BIN=C:\msys64\mingw32\bin"
if exist "%MINGW_BIN%\g++.exe" set "PATH=%MINGW_BIN%;%PATH%"

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ was not found. Install the MSYS2 MinGW 32-bit toolchain or add it to PATH.
    echo Expected compiler path: %MINGW_BIN%\g++.exe
    popd
    exit /b 1
)

set "MAKE_EXE="
if exist "%MINGW_BIN%\mingw32-make.exe" set "MAKE_EXE=%MINGW_BIN%\mingw32-make.exe"
if not defined MAKE_EXE if exist "C:\msys64\usr\bin\make.exe" set "MAKE_EXE=C:\msys64\usr\bin\make.exe"
if not defined MAKE_EXE (
    for /f "delims=" %%M in ('where make 2^>nul') do if not defined MAKE_EXE set "MAKE_EXE=%%M"
)
if not defined MAKE_EXE (
    echo ERROR: make was not found. Install make through MSYS2.
    popd
    exit /b 1
)

echo Copying application resources into win\build\data...
robocopy "data" "win\build\data" /E /NFL /NDL /NJH /NJS /NC /NS /NP
if errorlevel 8 (
    echo ERROR: Failed to copy application resources.
    popd
    exit /b 1
)

echo Copying runtime libraries into win\build...
robocopy "support" "win\build" *.dll /NFL /NDL /NJH /NJS /NC /NS /NP
if errorlevel 8 (
    echo ERROR: Failed to copy runtime libraries.
    popd
    exit /b 1
)

rem Replace a missing or malformed downloaded cache with the bundled valid list.
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { [xml]$doc = Get-Content -LiteralPath 'win\build\compatibility.xml' -Raw; if ($doc.compatibility.software -and $doc.compatibility.firmwares) { exit 0 }; exit 1 } catch { exit 1 }"
if errorlevel 1 (
    echo Restoring bundled firmware compatibility list...
    copy /Y "src\engine\firmware\compatibility.xml" "win\build\compatibility.xml" >nul
    if errorlevel 1 (
        echo ERROR: Could not restore compatibility.xml.
        popd
        exit /b 1
    )
)

echo Building the application. Details: win\build\build.log
rem Stage the link, then update the original SpikeRecorder.exe in place.
rem A running recording session is never stopped by the build.
set "BUILD_EXE=win\build\SpikeRecorder-next.exe"
set "APP_EXE=win\build\SpikeRecorder.exe"
"%MAKE_EXE%" -j2 TARGET=./win/build/SpikeRecorder-next.exe > "win\build\build.log" 2>&1
if errorlevel 1 (
    echo ERROR: Build failed. Last compiler messages:
    powershell -NoProfile -Command "Get-Content -LiteralPath 'win\build\build.log' -Tail 24"
    echo Full details: "%CD%\win\build\build.log"
    popd
    exit /b 1
)

if not exist "%BUILD_EXE%" (
    echo ERROR: Build reported success, but %BUILD_EXE% was not created.
    popd
    exit /b 1
)

if exist "game" (
    echo Synchronizing installable games into win\build\game...
    robocopy "game" "win\build\game" /MIR /NFL /NDL /NJH /NJS /NC /NS /NP
    if errorlevel 8 (
        echo ERROR: Failed to copy game modules.
        popd
        exit /b 1
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -File "tools\install-build.ps1"
if errorlevel 4 (
    echo ERROR: Could not apply the completed build.
    popd
    exit /b 1
)
if errorlevel 3 (
    echo The running application will use the update after it is closed and launched again.
) else if errorlevel 1 (
    echo ERROR: Could not apply the completed build.
    popd
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "tools\build-state.ps1" -Mode Record
if errorlevel 1 (
    echo ERROR: Could not record the completed build state.
    popd
    exit /b 1
)

echo Build completed: %APP_EXE%
popd
exit /b 0
