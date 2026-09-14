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
REM The engine path is NOT hardcoded. It is asked for, in four ways, because
REM the last guess - "C:\Program Files\Epic Games\UE_5.6" - was wrong on the
REM very machine this project is built on.
REM
REM STATUS: UNVERIFIED - written on a Linux container with no Unreal Engine and
REM no cmd.exe. Every line here is untested until it has run on Windows once.

setlocal enabledelayedexpansion
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

REM 1. An explicit answer always wins.
set "UE=%VAELEN_UE_ROOT%"
if not "%UE%"=="" (
	echo [vaelen] using VAELEN_UE_ROOT
	goto :found
)

REM 2. The registry, which is where the Epic installer records it.
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%WANT%" /v InstalledDirectory 2^>nul ^| findstr /c:"InstalledDirectory"') do set "UE=%%b"
if not "%UE%"=="" goto :found
for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\EpicGames\Unreal Engine\%WANT%" /v InstalledDirectory 2^>nul ^| findstr /c:"InstalledDirectory"') do set "UE=%%b"
if not "%UE%"=="" goto :found

REM 3. A source build registers itself under Builds, keyed by a GUID.
for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" 2^>nul ^| findstr /c:"REG_SZ"') do (
	if exist "%%b\Engine\Build\BatchFiles\Build.bat" set "UE=%%b"
)
if not "%UE%"=="" goto :found

REM 4. The usual places, on every drive letter that exists.
for %%d in (C D E F G H) do (
	for %%p in ("%%d:\UE_%WANT%" "%%d:\Epic Games\UE_%WANT%" "%%d:\Program Files\Epic Games\UE_%WANT%" "%%d:\UnrealEngine") do (
		if exist "%%~p\Engine\Build\BatchFiles\Build.bat" set "UE=%%~p"
	)
)
if not "%UE%"=="" goto :found

echo.
echo [vaelen] Unreal Engine %WANT% was not found.
echo [vaelen] Looked in the registry (HKLM, HKCU, and source builds) and in the
echo [vaelen] usual folders on drives C to H.
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
