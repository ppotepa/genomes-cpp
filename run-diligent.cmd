@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-diligent.ps1" %*
exit /b %ERRORLEVEL%
