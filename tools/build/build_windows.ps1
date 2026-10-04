<#
.SYNOPSIS
    Reproducible Windows x64 build script for the Ariane SA-MP editor fork.

.DESCRIPTION
    Configures and builds librw and Ariane for Windows (Release win-amd64-d3d9).
    Locates Visual Studio toolchain automatically via vswhere, validates the
    pinned librw commit, runs Premake5 project generation, and builds via MSBuild.

    Supports -CheckOnly for non-destructive toolchain and environment inspection
    without invoking compilation or premake actions.

.PARAMETER LibrwPath
    Path to the librw worktree. Defaults to $env:LIBRW, sibling ..\ariane-librw,
    or a local librw directory.

.PARAMETER PremakePath
    Path to premake5.exe. Defaults to $env:PREMAKE5, local-build\premake\premake5.exe,
    or premake5 in PATH.

.PARAMETER Configuration
    Build configuration. Default: Release.

.PARAMETER Platform
    Target platform. Default: win-amd64-d3d9.

.PARAMETER Channel
    Ariane update channel: 'master' or 'PE'. Default: master.

.PARAMETER VsVersion
    Premake Visual Studio target action. Default: vs2022 (fallback: vs2019).

.PARAMETER PlatformToolset
    MSBuild C++ toolset. Default: v145 (VS2022 v18 preview) or v143 (VS2022 v17).
    If set to 'auto', selects toolset based on installed MSVC compiler.

.PARAMETER CheckOnly
    Perform toolchain, dependency, and path inspection only. Does not build.

.PARAMETER SkipLibrw
    Skip building librw if already compiled.
#>

[CmdletBinding()]
param(
    [string]$LibrwPath = "",
    [string]$PremakePath = "",
    [string]$Configuration = "Release",
    [string]$Platform = "win-amd64-d3d9",
    [string]$Channel = "master",
    [string]$VsVersion = "vs2022",
    [string]$PlatformToolset = "auto",
    [switch]$CheckOnly,
    [switch]$SkipLibrw
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PINNED_LIBRW_COMMIT = "15ffa585216a9a7573ecc597b19ce2fde9b935f2"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Ariane SA-MP Windows Build Preparation Tool" -ForegroundColor Cyan
Write-Host " [NOTE] In-development delivery preparation draft (untested)" -ForegroundColor DarkGray
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Repository root: $RepoRoot"

# 1. Resolve Premake5
$ResolvedPremake = ""
if ($PremakePath -and (Test-Path $PremakePath)) {
    $ResolvedPremake = (Resolve-Path $PremakePath).Path
} elseif ($env:PREMAKE5 -and (Test-Path $env:PREMAKE5)) {
    $ResolvedPremake = (Resolve-Path $env:PREMAKE5).Path
} elseif (Test-Path (Join-Path $RepoRoot "local-build\premake\premake5.exe")) {
    $ResolvedPremake = (Resolve-Path (Join-Path $RepoRoot "local-build\premake\premake5.exe")).Path
} else {
    $cmd = Get-Command "premake5.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        $ResolvedPremake = $cmd.Source
    }
}

if (-not $ResolvedPremake) {
    Write-Warning "Premake5 executable not found. Set -PremakePath or PREMAKE5 environment variable."
    Write-Warning "Recommended version: Premake 5.0.0-beta8+ for VS2022 solution generation."
    if (-not $CheckOnly) {
        throw "Build halted: Premake5 is required."
    }
} else {
    Write-Host "Premake5 executable: $ResolvedPremake" -ForegroundColor Green
}

# 2. Resolve librw path
$ResolvedLibrw = ""
$CandidateLibrwPaths = @(
    $LibrwPath,
    $env:LIBRW,
    (Join-Path $RepoRoot "..\ariane-librw"),
    (Join-Path $RepoRoot "librw")
)

foreach ($candidate in $CandidateLibrwPaths) {
    if ($candidate -and (Test-Path $candidate)) {
        $ResolvedLibrw = (Resolve-Path $candidate).Path
        break
    }
}

if (-not $ResolvedLibrw) {
    Write-Warning "librw directory not found. Checked candidate paths:"
    $CandidateLibrwPaths | Where-Object { $_ } | ForEach-Object { Write-Warning "  - $_" }
    if (-not $CheckOnly) {
        throw "Build halted: librw worktree is required."
    }
} else {
    Write-Host "librw directory:    $ResolvedLibrw" -ForegroundColor Green

    # Check git commit of librw
    if (Test-Path (Join-Path $ResolvedLibrw ".git")) {
        try {
            $librwHead = (git -C $ResolvedLibrw rev-parse HEAD 2>$null).Trim()
            if ($librwHead -eq $PINNED_LIBRW_COMMIT) {
                Write-Host "librw commit:       $librwHead (MATCHES pinned pin)" -ForegroundColor Green
            } else {
                if (-not $CheckOnly) {
                    throw "Dependency pin mismatch: librw commit is '$librwHead', expected pinned pin '$PINNED_LIBRW_COMMIT'. Build aborted."
                } else {
                    Write-Warning "Dependency pin mismatch: librw commit is '$librwHead', expected pinned pin '$PINNED_LIBRW_COMMIT'."
                }
            }
        } catch {
            if (-not $CheckOnly) {
                throw "Could not verify librw git commit: $_"
            } else {
                Write-Warning "Could not query librw git commit: $_"
            }
        }
    } else {
        if (-not $CheckOnly) {
            throw "librw is not a git repository checkout; unable to verify required commit pin $PINNED_LIBRW_COMMIT. Build aborted."
        } else {
            Write-Warning "librw is not a git repository checkout; unable to verify commit $PINNED_LIBRW_COMMIT."
        }
    }
}

# Helper: Map MSVC toolset version to PlatformToolset string
function Resolve-ToolsetFromVersion([string]$ver) {
    if ($ver) {
        if ($ver.StartsWith("14.2")) { return "v142" }
        if ($ver.StartsWith("14.3") -or $ver.StartsWith("14.4")) { return "v143" }
        if ($ver.StartsWith("14.5")) { return "v145" }
    }
    return "v143"
}

# 3. Locate Visual Studio / MSBuild toolchain
function Initialize-VsDevEnvironment {
    param([ref]$SelectedToolset)

    # If cl and msbuild are already in PATH
    $clCmd = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    $msbuildCmd = Get-Command "msbuild.exe" -ErrorAction SilentlyContinue
    if ($clCmd -and $msbuildCmd) {
        Write-Host "MSVC environment already initialized in current shell:" -ForegroundColor Green
        Write-Host "  Compiler: $($clCmd.Source)"
        Write-Host "  MSBuild:  $($msbuildCmd.Source)"
        if ($SelectedToolset.Value -eq "auto") {
            $SelectedToolset.Value = Resolve-ToolsetFromVersion $env:VCToolsVersion
        }
        return $true
    }

    # Locate vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Warning "vswhere.exe not found at $vswhere"
        return $false
    }

    $vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstallPath -or -not (Test-Path $vsInstallPath)) {
        Write-Warning "Visual Studio with C++ tools not found by vswhere."
        return $false
    }

    Write-Host "Visual Studio install: $vsInstallPath" -ForegroundColor Green
    $vcvars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        Write-Warning "vcvars64.bat not found at $vcvars"
        return $false
    }

    Write-Host "vcvars64.bat found:   $vcvars" -ForegroundColor Green

    # Import environment variables from vcvars64.bat safely with cmd.exe /s /c
    $tempFile = [System.IO.Path]::GetTempFileName()
    cmd.exe /d /c "call `"$vcvars`" > nul && set > `"$tempFile`""
    if ($LASTEXITCODE -ne 0) { throw "MSVC environment initialization failed with exit code $LASTEXITCODE" }
    if (Test-Path $tempFile) {
        Get-Content $tempFile | ForEach-Object {
            if ($_ -match "^(.*?)=(.*)$") {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], [System.EnvironmentVariableTarget]::Process)
            }
        }
        Remove-Item $tempFile -Force
    }

    if ($SelectedToolset.Value -eq "auto") {
        $SelectedToolset.Value = Resolve-ToolsetFromVersion $env:VCToolsVersion
    }

    return $true
}

$effectiveToolset = $PlatformToolset
$vsReady = Initialize-VsDevEnvironment -SelectedToolset ([ref]$effectiveToolset)
if (-not $vsReady) {
    Write-Warning "Could not automatically initialize MSVC dev environment."
    Write-Warning "Please run this script from a 'x64 Native Tools Command Prompt for VS' or Developer PowerShell."
    if (-not $CheckOnly) {
        throw "Build halted: Visual Studio toolchain is required."
    }
} else {
    Write-Host "Effective PlatformToolset: $effectiveToolset" -ForegroundColor Green
    # Adjust Premake action if VS 2019 is detected
    if ($effectiveToolset -eq "v142" -and $VsVersion -eq "vs2022") {
        Write-Host "Detected VS 2019 toolset (v142); adjusting Premake target to vs2019." -ForegroundColor Yellow
        $VsVersion = "vs2019"
    }
}

# 4. Check target paths
$ArianeTargetDir = Join-Path $RepoRoot "bin\$Platform\$Configuration"
$ArianeBinary = Join-Path $ArianeTargetDir "ariane.exe"
Write-Host "Target output path:  $ArianeBinary"

if ($CheckOnly) {
    Write-Host ""
    Write-Host "[CheckOnly] Inspection complete. No builds, modifications, or tests were executed." -ForegroundColor Yellow
    Write-Host "[CheckOnly] Toolchain and paths verified successfully." -ForegroundColor Yellow
    return
}

# 5. Build execution
Write-Host ""
Write-Host "Starting build process..." -ForegroundColor Cyan
$env:LIBRW = $ResolvedLibrw

# Build librw
if (-not $SkipLibrw) {
    Write-Host "`n--- Generating librw solution ($VsVersion) ---" -ForegroundColor Cyan
    Push-Location $ResolvedLibrw
    try {
        & $ResolvedPremake $VsVersion
        if ($LASTEXITCODE -ne 0) { throw "librw premake generation failed with exit code $LASTEXITCODE" }

        Write-Host "--- Compiling librw ($Configuration $Platform $effectiveToolset) ---" -ForegroundColor Cyan
        $librwSln = Join-Path $ResolvedLibrw "build\librw.sln"
        & msbuild.exe $librwSln /p:Configuration=$Configuration /p:Platform=$Platform /p:PlatformToolset=$effectiveToolset /t:librw /m /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "librw compilation failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "Skipping librw build (-SkipLibrw requested)." -ForegroundColor Yellow
}

# Build Ariane
Write-Host "`n--- Generating Ariane solution ($VsVersion --channel=$Channel) ---" -ForegroundColor Cyan
Push-Location $RepoRoot
try {
    & $ResolvedPremake $VsVersion "--channel=$Channel"
    if ($LASTEXITCODE -ne 0) { throw "Ariane premake generation failed with exit code $LASTEXITCODE" }

    Write-Host "--- Compiling Ariane ($Configuration $Platform $effectiveToolset) ---" -ForegroundColor Cyan
    $arianeSln = Join-Path $RepoRoot "build\librwgta.sln"
    & msbuild.exe $arianeSln /p:Configuration=$Configuration /p:Platform=$Platform /p:PlatformToolset=$effectiveToolset "/t:librwgta;euryopa" /m /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "Ariane compilation failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}

if (Test-Path $ArianeBinary) {
    $sha256 = (Get-FileHash -Path $ArianeBinary -Algorithm SHA256).Hash
    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host " Ariane build succeeded!" -ForegroundColor Green
    Write-Host " Binary:  $ArianeBinary" -ForegroundColor Green
    Write-Host " SHA-256: $sha256" -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
} else {
    throw "Build completed but target binary not found at $ArianeBinary"
}
