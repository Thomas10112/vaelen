@echo off
REM VAELEN - open the project in the Unreal editor.
REM
REM The companion of Build.bat, and here for the same reason: the command that
REM opens this project lived nowhere in the repository, so the second person to
REM need it had to be told. It finds the engine the same way - Tools\find_unreal.ps1,
REM which asks Windows instead of guessing folder names.
REM
REM Anything typed after this script is passed through to the editor, which is
REM how the two flags that matter get used:
REM
REM     Editor.bat -d3d11
REM         A D3D12 crash on startup is what this machine hit in 13.07c, and
REM         -d3d11 is what got past it. Try it before anything else.
REM
REM     Editor.bat -ExecCmds="Vaelen.View 128 120" -nullrhi
REM         No window, no shaders, no GPU: the world is still built, the four
REM         views are still taken, the world is still destroyed, and the
REM         LogVaelenView numbers still come out. That is how 13.07c was
REM         checked on a machine whose editor would not open, and it is the
REM         fastest way to read the figures when the picture is not the point.
REM
REM STATUS: VERIFIED 2026-09-14. The editor log of that day reads
REM     Command Line: -log -d3d11
REM -log first, then what was typed after the script - which is the order this
REM file writes and not the order of the hand-typed command it replaced. That is
REM how a batch file that prints nothing of its own gets to say it ran.

REM Nothing here needs delayed expansion, and it would eat an exclamation mark
REM in somebody's install path.
setlocal
set "ROOT=%~dp0"

set "WANT="
for /f "tokens=2 delims=:," %%a in ('findstr /c:"EngineAssociation" "%ROOT%Vaelen.uproject"') do set "WANT=%%a"
set "WANT=%WANT: =%"
set "WANT=%WANT:"=%"
if "%WANT%"=="" (
	echo [vaelen] could not read EngineAssociation from Vaelen.uproject
	exit /b 1
)

set "UE=%VAELEN_UE_ROOT%"
if not "%UE%"=="" goto :found
for /f "usebackq delims=" %%a in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%Tools\find_unreal.ps1" -Version %WANT%`) do set "UE=%%a"
if not "%UE%"=="" goto :found

echo.
echo [vaelen] Unreal Engine %WANT% was not found.
echo [vaelen] To see every place that was asked and what each answered:
echo [vaelen]     powershell -File Tools\find_unreal.ps1 -Version %WANT% -Explain
echo.
echo [vaelen] Or say where it is, without angle brackets around the path:
echo [vaelen]     setx VAELEN_UE_ROOT "D:\Users\You\UE_%WANT%"
echo [vaelen] then open a NEW shell - setx does not reach the one you are in.
exit /b 1

:found
set "EDITOR=%UE%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%EDITOR%" (
	echo [vaelen] no UnrealEditor.exe under %UE%\Engine\Binaries\Win64
	echo [vaelen] the engine is there but not built, or VAELEN_UE_ROOT is not an engine root
	exit /b 1
)
echo [vaelen] engine: %UE%
echo [vaelen] opening Vaelen.uproject

REM -log gives the console window the LogVaelenView lines are read from. Drop
REM it by passing -nolog if a clean session is wanted.
start "" "%EDITOR%" "%ROOT%Vaelen.uproject" -log %*
exit /b 0
