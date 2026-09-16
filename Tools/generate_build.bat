@echo off
pwsh -NoProfile -File "%~dp0build.ps1" -Component Native -ConfigureOnly %*
exit /b %errorlevel%
