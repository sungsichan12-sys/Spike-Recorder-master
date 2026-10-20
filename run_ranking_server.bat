@echo off
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server\manage-server.ps1" -Mode Start
if errorlevel 1 (
    echo ERROR: Ranking server could not start.
    if /I not "%~1"=="--quiet" pause
    exit /b 1
)
exit /b 0
