@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0rundev.ps1" %*
exit /b %ERRORLEVEL%
