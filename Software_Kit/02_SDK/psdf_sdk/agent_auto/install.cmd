@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "PYTHON=python"
if defined AGENT_AUTO_PYTHON set "PYTHON=%AGENT_AUTO_PYTHON%"
set "CLI=%ROOT%modules\core\cli.py"
set "DLL=%ROOT%assets\DownloadLib.dll"
set "SKILL_SRC=%ROOT%skill\autoburn"
set "MODE=all"

if /i "%~1"=="--cli-only" set "MODE=cli"
if /i "%~1"=="--skill-only" set "MODE=skill"

"%PYTHON%" --version >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python not found.
    echo Set AGENT_AUTO_PYTHON to python.exe and retry.
    exit /b 2
)

if not "%MODE%"=="skill" (
    "%PYTHON%" "%CLI%" install --add-path --dll "%DLL%"
    if errorlevel 1 exit /b 1
)

if not "%MODE%"=="cli" (
    if defined AGENT_AUTO_SKILLS set "SKILL_DEST=!AGENT_AUTO_SKILLS!"
    if not defined SKILL_DEST if defined CODEX_HOME set "SKILL_DEST=!CODEX_HOME!\skills\autoburn"
    if not defined SKILL_DEST set "SKILL_DEST=!USERPROFILE!\.codex\skills\autoburn"
    if not exist "!SKILL_DEST!" mkdir "!SKILL_DEST!"
    xcopy "%SKILL_SRC%\*" "!SKILL_DEST!\" /E /I /Y >nul
    if errorlevel 1 exit /b 1
    echo SKILL_INSTALLED=!SKILL_DEST!
)

echo INSTALL_OK=1
echo Restart the terminal before using the bare command: AgentAuto
exit /b 0
