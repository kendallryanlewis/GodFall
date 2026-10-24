@echo off
setlocal
if not exist "%~dp0GodFall_Clean\Content\Paks" (
  echo Copy Play-GodFall.cmd and Start-GodFall.ps1 beside the packaged GodFall_Clean.exe.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0Start-GodFall.ps1" -GameRoot "%~dp0GodFall_Clean"
if errorlevel 1 (
  pause
  exit /b 1
)
exit /b 0
