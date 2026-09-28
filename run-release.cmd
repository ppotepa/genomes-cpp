@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-release.ps1" %*
exit /b %ERRORLEVEL%
