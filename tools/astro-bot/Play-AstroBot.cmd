@echo off
cd /d "%~dp0"
if "%~1"=="" (
 powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-AstroBot.ps1"
) else (
 powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-AstroBot.ps1" -GamePath "%~1"
)
if errorlevel 1 pause
