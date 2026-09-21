@echo off
chcp 65001 >nul
setlocal
if not defined AGENT_AUTO_SDK_ROOT (
    if exist "%~dp0..\project" if exist "%~dp0..\platform" set "AGENT_AUTO_SDK_ROOT=%~dp0.."
)

set "PYTHON=python"
if defined AGENT_AUTO_PYTHON set "PYTHON=%AGENT_AUTO_PYTHON%"

"%PYTHON%" "%~dp0modules\core\cli.py" %*
exit /b %ERRORLEVEL%
