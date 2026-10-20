@echo off
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cloud\configure.ps1" -ConnectionFile "%~1"
if errorlevel 1 echo ERROR: Cloud connection could not be configured.
pause
