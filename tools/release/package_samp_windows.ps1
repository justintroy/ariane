<#
.SYNOPSIS
    PowerShell wrapper for package_samp_windows.py.

.DESCRIPTION
    Runs the Ariane SA-MP Windows distribution package preparation script.
    Defaults to dry-run mode. Pass -CreateArchive -AllowUnverified to stage an archive.
#>

[CmdletBinding()]
param(
    [string]$EnginePath = "",
    [string]$OutputDir = "",
    [switch]$CreateArchive,
    [switch]$AllowUnverified,
    [Parameter(ValueFromRemainingArguments)]
    [string[]]$RemainingArgs
)

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$PythonExe = "python.exe"

# Look for venv python if available
$venvPython = Join-Path $RepoRoot ".venv-agent\Scripts\python.exe"
if (Test-Path $venvPython) {
    $PythonExe = $venvPython
}

$ScriptPath = Join-Path $PSScriptRoot "package_samp_windows.py"

$ArgsList = @($ScriptPath)
if ($EnginePath) { $ArgsList += @("--engine", $EnginePath) }
if ($OutputDir) { $ArgsList += @("--output", $OutputDir) }
if ($CreateArchive) { $ArgsList += "--create-archive" }
if ($AllowUnverified) { $ArgsList += "--allow-unverified" }
if ($RemainingArgs) { $ArgsList += $RemainingArgs }

& $PythonExe @ArgsList
exit $LASTEXITCODE
