<#
.SYNOPSIS
    Portable standalone SA-MP core test compiler and runner for Windows.

.DESCRIPTION
    Compiles and executes the standalone SA-MP document core tests (tests/samp/core_test.cpp)
    and rotation equivalence tests (tests/samp/rotation_test.cpp).
    Auto-detects the Visual Studio toolchain via vswhere if not already initialized.
    Supports -CheckOnly for non-destructive environment inspection.

.PARAMETER OutDir
    Directory for intermediate compilation and test artifacts. Default: build\test.

.PARAMETER SkipRotation
    Skip the rotation equivalence test.

.PARAMETER CheckOnly
    Perform toolchain and source inspection only. Does not compile or run tests.
#>

[CmdletBinding()]
param(
    [string]$LibrwPath = "",
    [string]$OutDir = "build\test",
    [switch]$SkipRotation,
    [switch]$CheckOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$TargetOutDir = Join-Path $RepoRoot $OutDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Ariane SA-MP Core Test Runner (Windows x64)" -ForegroundColor Cyan
Write-Host " [NOTE] In-development delivery preparation draft (untested)" -ForegroundColor DarkGray
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Repository root: $RepoRoot"
Write-Host "Output directory: $TargetOutDir"

# 1. Locate Visual Studio environment
$clCmd = Get-Command "cl.exe" -ErrorAction SilentlyContinue
if (-not $clCmd) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstallPath -and (Test-Path $vsInstallPath)) {
            $vcvars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvars) {
                Write-Host "Initializing MSVC environment from $vcvars..." -ForegroundColor Cyan
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
            }
        }
    }
}

$clCmd = Get-Command "cl.exe" -ErrorAction SilentlyContinue
if (-not $clCmd) {
    Write-Warning "cl.exe (MSVC compiler) not found. Run from a Developer Command Prompt or install VS Build Tools."
    if (-not $CheckOnly) { throw "Test halted: cl.exe required." }
} else {
    Write-Host "MSVC compiler:    $($clCmd.Source)" -ForegroundColor Green
}

# 2. Check source files
$coreTestSrc = Join-Path $RepoRoot "tests\samp\core_test.cpp"
$sampDocSrc = Join-Path $RepoRoot "tools\euryopa\samp_document.cpp"
$rotationTestSrc = Join-Path $RepoRoot "tests\samp\rotation_test.cpp"
$sampRotationSrc = Join-Path $RepoRoot "tools\euryopa\samp_rotation.cpp"

$sourcesToCheck = @($coreTestSrc, $sampDocSrc)
if (-not $SkipRotation) {
    $sourcesToCheck += @($rotationTestSrc, $sampRotationSrc)
}

foreach ($f in $sourcesToCheck) {
    if (-not (Test-Path $f)) {
        throw "Required source file missing: $f"
    }
}
if ($SkipRotation) {
    Write-Host "Source files:     core_test.cpp and samp_document.cpp verified (rotation skipped)" -ForegroundColor Green
} else {
    Write-Host "Source files:     core and rotation test sources verified" -ForegroundColor Green
}

if ($CheckOnly) {
    Write-Host ""
    Write-Host "[CheckOnly] Environment, compiler, and test source files verified." -ForegroundColor Yellow
    if ($SkipRotation) {
        Write-Host "[CheckOnly] Rotation test marked as skipped via -SkipRotation." -ForegroundColor Yellow
    }
    Write-Host "[CheckOnly] No test compilation or execution was performed." -ForegroundColor Yellow
    return
}

# 3. Prepare output directory
if (-not (Test-Path $TargetOutDir)) {
    New-Item -ItemType Directory -Path $TargetOutDir -Force | Out-Null
}

# 4. Compile and run core tests
Write-Host "`n--- Compiling SA-MP Core Tests ---" -ForegroundColor Cyan
$coreExe = Join-Path $TargetOutDir "samp-core-test.exe"
$foArg = "/Fo:" + $TargetOutDir + "/"
$feArg = "/Fe:" + $coreExe
$incArg = "/I" + (Join-Path $RepoRoot "tools\euryopa")

& cl.exe /nologo /EHsc /std:c++14 $incArg $coreTestSrc $sampDocSrc $feArg $foArg
if ($LASTEXITCODE -ne 0) { throw "Core test compilation failed with exit code $LASTEXITCODE" }

Write-Host "--- Running SA-MP Core Tests ---" -ForegroundColor Cyan
$roundtripPrefix = Join-Path $TargetOutDir "roundtrip"
& $coreExe $roundtripPrefix
if ($LASTEXITCODE -ne 0) { throw "Core tests failed with exit code $LASTEXITCODE" }

# 5. Compile and run rotation tests if requested
if (-not $SkipRotation) {
    Write-Host "`n--- Compiling SA-MP Rotation Tests ---" -ForegroundColor Cyan
    $rotExe = Join-Path $TargetOutDir "samp-rotation-test.exe"
    $rotFeArg = "/Fe:" + $rotExe

    if (-not $LibrwPath) { $LibrwPath = Join-Path $RepoRoot "..\ariane-librw" }
    $rwInclude = "/I" + (Resolve-Path $LibrwPath).Path
    $rwLibrary = Join-Path $LibrwPath "lib\win-amd64-d3d9\Release\rw.lib"
    if (-not (Test-Path $rwLibrary)) { throw "Pinned librw Release library required for rotation tests: $rwLibrary" }
    & cl.exe /nologo /EHsc /MD /std:c++14 $incArg $rwInclude $rotationTestSrc $sampRotationSrc $rwLibrary user32.lib gdi32.lib d3d9.lib $rotFeArg $foArg
    if ($LASTEXITCODE -ne 0) { throw "Rotation test compilation failed with exit code $LASTEXITCODE" }

    Write-Host "--- Running SA-MP Rotation Tests ---" -ForegroundColor Cyan
    & $rotExe
    if ($LASTEXITCODE -ne 0) { throw "Rotation tests failed with exit code $LASTEXITCODE" }
}

Write-Host "`n==========================================================" -ForegroundColor Green
if ($SkipRotation) {
    Write-Host " SA-MP core tests passed successfully (rotation tests skipped)." -ForegroundColor Green
} else {
    Write-Host " All requested SA-MP core and rotation tests passed successfully!" -ForegroundColor Green
}
Write-Host "==========================================================" -ForegroundColor Green
