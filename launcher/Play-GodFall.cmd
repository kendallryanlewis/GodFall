@echo off
setlocal
if not exist "%~dp0Start-GodFallRelease.ps1" (
  echo Keep Play-GodFall.cmd, Start-GodFallRelease.ps1 and Start-GodFall.ps1 together.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0Start-GodFallRelease.ps1" -InstallRoot "%LOCALAPPDATA%\GodFallGitHub"
if errorlevel 1 (
  pause
  exit /b 1
)
exit /b 0
