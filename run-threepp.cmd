@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-threepp.ps1" %*
exit /b %ERRORLEVEL%
