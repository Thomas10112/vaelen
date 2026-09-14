@echo off
REM VAELEN - build the Unreal side of the project.
REM
REM WHY THIS FILE EXISTS. It did not, and that cost a quarter of an hour on
REM 2026-09-14. The one command that builds the fourteen UBT modules lived in
REM one person's shell history and nowhere else: `.\Build.bat` had been typed
REM before, it worked, and nothing in the repository knew what it was. When the
REM history went, so did the command, and the engine half of the project became
REM unbuildable by anyone who had not built it already.
REM
REM THE ENGINE PATH IS ASKED FOR, NEVER GUESSED, and the first version of this
REM file got that wrong. It scanned "the usual folders" on drives C to H -
REM D:\UE_5.6, D:\Epic Games\UE_5.6, and so on. The real answer on the machine
REM this project is built on is:
REM
REM     D:\Users\Utilisateur\UE_5.6
REM
REM A user profile folder, on the second drive. No list of usual folders was
REM ever going to contain it, and a longer list would only have failed later.
REM So the guessing is gone. Windows already KNOWS where the engine is, in two
REM authoritative places, and this file reads them instead:
REM
REM     LauncherInstalled.dat   what the Epic launcher installed, and where
REM     the registry            what a source build registered about itself
REM
REM The lookup itself is Tools\find_unreal.ps1, a real file: one of those two
REM places is JSON and the other is the registry, and a PowerShell one-liner
REM long enough to read both would have to survive cmd's parser first, every
REM pipe and quote escaped by hand. It can also be run on its own with
REM -Explain, which says what each source answered.
REM
REM STATUS: PARTLY VERIFIED - run on Windows 2026-09-14. It read the version
REM from the .uproject, reported where it had looked, and exited cleanly when
REM its guesses failed, which is how the defect above was found. The lookup it
REM now uses has not run yet.

REM Plain setlocal: nothing here needs delayed expansion, and enabling it
REM would quietly eat an exclamation mark in somebody's install path.
setlocal
set "ROOT=%~dp0"

REM The version the project asks for, read from the .uproject rather than
REM assumed, so this file does not go stale the day the engine is upgraded.
set "WANT="
for /f "tokens=2 delims=:," %%a in ('findstr /c:"EngineAssociation" "%ROOT%Vaelen.uproject"') do set "WANT=%%a"
set "WANT=%WANT: =%"
set "WANT=%WANT:"=%"
if "%WANT%"=="" (
	echo [vaelen] could not read EngineAssociation from Vaelen.uproject
	exit /b 1
)
echo [vaelen] the project asks for Unreal Engine %WANT%

REM 1. An explicit answer always wins, and is the escape hatch if the two
REM    authoritative sources below ever disagree with reality.
set "UE=%VAELEN_UE_ROOT%"
if not "%UE%"=="" (
	echo [vaelen] using VAELEN_UE_ROOT
	goto :found
)

REM 2. Ask Windows, in Tools\find_unreal.ps1 - a real file rather than a
REM    PowerShell one-liner wedged into a batch backtick, where every pipe and
REM    quote would have to be escaped past cmd first. It can also be run on its
REM    own, with -Explain, which is what to reach for when this fails.
for /f "usebackq delims=" %%a in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%Tools\find_unreal.ps1" -Version %WANT%`) do set "UE=%%a"
if not "%UE%"=="" goto :found

echo.
echo [vaelen] Unreal Engine %WANT% was not found.
echo [vaelen] To see every place that was asked and what each answered:
echo [vaelen]     powershell -File Tools\find_unreal.ps1 -Version %WANT% -Explain
echo.
echo [vaelen] Say where it is and this file will stop guessing:
echo [vaelen]     setx VAELEN_UE_ROOT "D:\your\path\to\UE_%WANT%"
echo [vaelen] then open a new shell.
exit /b 1

:found
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" (
	echo [vaelen] %UE% has no Engine\Build\BatchFiles\Build.bat - not an engine root
	exit /b 1
)
echo [vaelen] engine: %UE%

REM VaelenEditor is the target the Details panel and the Vaelen.View console
REM command both need; Development is the configuration the project is worked
REM in. Anything typed after this script is passed straight through, so
REM `Build.bat -Clean` and the like still work.
call "%UE%\Engine\Build\BatchFiles\Build.bat" VaelenEditor Win64 Development -Project="%ROOT%Vaelen.uproject" -WaitMutex %*
exit /b %ERRORLEVEL%
