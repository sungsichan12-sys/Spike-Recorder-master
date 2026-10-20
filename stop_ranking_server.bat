@echo off
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server\manage-server.ps1" -Mode Stop
if errorlevel 1 (
    echo ERROR: Ranking server could not be stopped safely.
    pause
    exit /b 1
)
exit /b 0
