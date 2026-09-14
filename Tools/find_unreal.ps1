# VAELEN - where is Unreal Engine on this machine?
#
# WRITTEN BECAUSE GUESSING FAILED. Build.bat's first version scanned "the usual
# folders" on drives C to H - D:\UE_5.6, D:\Epic Games\UE_5.6, and so on. The
# real answer on the machine this project is built on is
#
#     D:\Users\Utilisateur\UE_5.6
#
# a user profile folder on the second drive. No list of usual folders was ever
# going to contain it, and a longer list would only have failed later somewhere
# else. Windows already knows the answer in two authoritative places, so this
# asks them instead of inventing candidates.
#
# Prints one line - the engine root - and exits 0. Prints nothing and exits 1
# when it cannot find one, so a caller can test the exit code or the emptiness
# of the output and get the same answer either way.
#
# Usable on its own:  powershell -File Tools\find_unreal.ps1 -Version 5.6
#
# STATUS: UNVERIFIED - written on a Linux container with no Windows, no
# registry and no PowerShell. Not one line has run.
[CmdletBinding()]
param(
	# The version the project asks for: Vaelen.uproject's EngineAssociation.
	[Parameter(Mandatory = $true)]
	[string] $Version,

	# Print every candidate and why it was kept or dropped.
	[switch] $Explain
)

$ErrorActionPreference = 'Continue'

# (where it came from, the path) pairs, in the order they are trusted.
$Candidates = New-Object System.Collections.Generic.List[object]

function Add-Candidate([string] $Source, $Path) {
	if ($Path -is [array]) {
		foreach ($One in $Path) { Add-Candidate $Source $One }
		return
	}
	if (-not [string]::IsNullOrWhiteSpace($Path)) {
		$Candidates.Add([pscustomobject]@{ Source = $Source; Path = [string]$Path })
	}
}

# 1. What the Epic launcher installed, and where it put it. This is the file
#    that had the right answer on the machine the guessing failed on.
$Dat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
if (Test-Path -LiteralPath $Dat) {
	try {
		$Installed = (Get-Content -LiteralPath $Dat -Raw | ConvertFrom-Json).InstallationList
		foreach ($Entry in $Installed) {
			if ($Entry.AppName -eq "UE_$Version") {
				Add-Candidate 'LauncherInstalled.dat' $Entry.InstallLocation
			}
		}
	} catch {
		if ($Explain) { Write-Host "[find] $Dat could not be read: $_" }
	}
}

# 2. What a launcher or source install recorded about itself. WOW6432Node is
#    included because a 32-bit installer writes there and a 64-bit PowerShell
#    does not see it otherwise.
foreach ($Key in @(
		"HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Version",
		"HKCU:\SOFTWARE\EpicGames\Unreal Engine\$Version",
		"HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Version")) {
	try {
		Add-Candidate $Key (Get-ItemProperty -Path $Key -ErrorAction Stop).InstalledDirectory
	} catch {
	}
}

# 3. Source builds register themselves under Builds, keyed by a GUID rather
#    than by a version, so every value is a candidate and the check below is
#    what sorts them out.
try {
	$Builds = Get-ItemProperty -Path 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction Stop
	foreach ($Property in $Builds.PSObject.Properties) {
		if ($Property.Name -notlike 'PS*') {
			Add-Candidate 'Builds (source)' $Property.Value
		}
	}
} catch {
}

# A path is only an answer if there is an engine at the end of it. A registry
# entry outlives the folder it names, and a stale one must not beat a real one.
foreach ($Candidate in $Candidates) {
	$Proof = Join-Path $Candidate.Path 'Engine\Build\BatchFiles\Build.bat'
	if (Test-Path -LiteralPath $Proof) {
		if ($Explain) { Write-Host "[find] kept  $($Candidate.Path)  <- $($Candidate.Source)" }
		Write-Output $Candidate.Path
		exit 0
	}
	if ($Explain) { Write-Host "[find] drop  $($Candidate.Path)  <- $($Candidate.Source) (no Engine\Build\BatchFiles)" }
}

if ($Explain) { Write-Host "[find] $($Candidates.Count) candidate(s), none with an engine in it" }
exit 1
