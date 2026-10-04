# Ariane SA-MP Editor — Windows Build Procedure & Environment

> [!NOTE]
> This document and the associated build scripts are in-development delivery preparation drafts. Windows build, core/rotation runner and internal packaging were executed on 2026-10-03; clean-install and release acceptance remain open.

This document defines the reproducible build procedure for the Ariane SA-MP editor fork on Windows x64.

## Overview & Scope

- **Target Binary**: Windows x64 Direct3D 9 (`bin/win-amd64-d3d9/Release/ariane.exe`).
- **Target Architecture**: `x86_64` (win-amd64).
- **Branch**: `codex/samp-support`.
- **Base Version**: `v1.40.9-agent-alpha.1` (commit `1a99d14b24b24b7cac238f2afbea3571b92518fc`).
- **Pinned librw**: Commit `15ffa585216a9a7573ecc597b19ce2fde9b935f2` on branch `ariane` of `Southland-FR/librw`.
- **Upstream Repository**: `https://github.com/Dryxio/ariane` (do NOT push here).
- **Intended Publishing Fork**: `https://github.com/justintroy/ariane`.

## Toolchain & Prerequisites

| Component | Minimum Version | Recommended / Tested | Purpose |
|---|---|---|---|
| **Operating System** | Windows 10 x64 (1903+) | Windows 11 x64 | Host environment |
| **MSVC Compiler** | Visual Studio 2019 (v142) | Visual Studio 2022 (v143 / v145) | C++14 compiler (`cl.exe`) |
| **Windows SDK** | 10.0.18362.0 | 10.0.22621.0+ | Windows headers and D3D9 libraries |
| **MSBuild** | 16.0 | 17.0+ (bundled with VS) | Solution building |
| **Premake5** | 5.0.0-beta8 | 5.0.0-beta8 (Windows x64) | VS solution generation (`vs2022` / `vs2019`) |
| **librw** | Commit `15ffa585...` | Pinned commit `15ffa585216a9a7573ecc597b19ce2fde9b935f2` | RenderWare abstraction library |
| **Python** | 3.10 | 3.10 – 3.12 x64 | Agent CLI / MCP and test automation |
| **Pawn Compiler** | 3.2.3664 / Pawn 3.10 | qawno / open.mp pawncc | Verification of exported Pawn scripts |

> [!IMPORTANT]
> **Premake version requirement**: Older Premake 5.0 releases (e.g. alpha or early beta) cannot generate `vs2022` project files correctly. Use Premake 5.0.0-beta8 or higher.
> **librw dependency pin**: Do not upgrade or switch the librw commit. Ariane relies on specific memory layout and skeleton modifications in `15ffa585216a9a7573ecc597b19ce2fde9b935f2`.

## Workspace Layout

The recommended workspace layout places the Ariane fork and librw as sibling directories:

```text
C:/Users/<User>/Documents/
├── ariane/                  <-- Ariane fork (branch: codex/samp-support)
│   ├── bin/
│   ├── docs/
│   ├── samples/
│   ├── tools/
│   └── premake5.lua
└── ariane-librw/            <-- librw checkout (commit: 15ffa585216a9a7573ecc597b19ce2fde9b935f2)
    ├── skeleton/
    ├── src/
    └── premake5.lua
```

Set the environment variable `LIBRW` to point to the librw worktree:
```powershell
$env:LIBRW = "C:\Users\<User>\Documents\ariane-librw"
```

## Automated Build Procedure

The repository provides a tracked, portable PowerShell build script with toolchain auto-detection:

### 1. Non-destructive Environment Inspection (Dry-Run)
Verify that tools, compiler, and dependency pins are satisfied without compiling:
```powershell
.\tools\build\build_windows.ps1 -CheckOnly
```

### 2. Standard Release Build
Compile both librw and Ariane in `Release` configuration for `win-amd64-d3d9`:
```powershell
.\tools\build\build_windows.ps1
```

Or via the Command Prompt wrapper:
```cmd
tools\build\build_windows.cmd
```

### Script Options

- `-LibrwPath <path>`: Explicit path to the librw worktree.
- `-PremakePath <path>`: Explicit path to `premake5.exe`.
- `-Configuration <Release|Debug>`: Build configuration (default: `Release`).
- `-Platform <win-amd64-d3d9|...>`: Target platform (default: `win-amd64-d3d9`).
- `-Channel <master|PE>`: Ariane channel define (default: `master`).
- `-VsVersion <vs2022|vs2019>`: Target Visual Studio generator (default: `vs2022`, auto-switched to `vs2019` if VS 2019 detected).
- `-PlatformToolset <auto|v142|v143|v145>`: MSBuild C++ toolset (default: `auto`).
- `-SkipLibrw`: Skip rebuilding librw if already compiled.
- `-CheckOnly`: Verify toolchain and dependency pins without compiling.

## Manual Step-by-Step Build Procedure

If invoking commands manually from a developer command prompt:

### 1. Open Visual Studio Developer Shell
```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
:: Or invoke vcvars64.bat directly from your VS installation:
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### 2. Compile librw
```cmd
set LIBRW=C:\path\to\ariane-librw
cd /d %LIBRW%
premake5 vs2022
:: Pass matching PlatformToolset: v142 (VS 2019), v143 (VS 2022 v17), or v145 (VS 2022 v18 Build Tools)
msbuild build\librw.sln /p:Configuration=Release /p:Platform=win-amd64-d3d9 /p:PlatformToolset=v143 /t:librw /m /v:minimal
```

### 3. Compile Ariane & Euryopa
```cmd
cd /d C:\path\to\ariane
premake5 vs2022 --channel=master
msbuild build\librwgta.sln /p:Configuration=Release /p:Platform=win-amd64-d3d9 /p:PlatformToolset=v143 /t:librwgta;euryopa /m /v:minimal
```

The resulting executable is generated at:
```text
bin\win-amd64-d3d9\Release\ariane.exe
```

## Testing & Verification Procedures

### 1. SA-MP Standalone Core Test Suite
Compiles and executes the standalone C++ document and parser tests (`tests/samp/core_test.cpp` and `tools/euryopa/samp_document.cpp`) alongside the rotation equivalence tests (`tests/samp/rotation_test.cpp` and `tools/euryopa/samp_rotation.cpp`). Core tests have no librw or game asset dependency. Rotation tests use pinned librw headers and its Release library:

```powershell
.\tools\build\run_samp_core_test.ps1
```

Or dry-run environment check:
```powershell
.\tools\build\run_samp_core_test.ps1 -CheckOnly
```

Covers:
- Static Pawn tokenizer and expression parser
- Semantic resolution of `CreateObject` and `CreateDynamicObject`
- Material texture and material text parameter parsing
- Version 1 `.samp.json` serialization and deserialization
- Undo/redo transaction bounds and historical snapshots
- Exported Pawn generation and multi-group separation
- Quaternion-Euler conversions across mixed axes and gimbal-lock singularities

### 2. SA-MP Rotation Equivalence Test Suite
Rotation equivalence tests (`tests/samp/rotation_test.cpp` and `tools/euryopa/samp_rotation.cpp`) are compiled from their standalone sources and executed by default. To skip rotation testing explicitly:
```powershell
.\tools\build\run_samp_core_test.ps1 -SkipRotation
```
Verifies quaternion-Euler conversions across mixed axes and gimbal-lock singularities using the pinned librw headers and compiled Release `rw.lib`.

### 3. Python Agent Test Suite
Verifies the CLI, service dispatch, and MCP forwarding layers:
```powershell
.\.venv-agent\Scripts\python.exe -m unittest discover -s tools/agent/tests -v
```

### 4. Pawn Export Compilation Verification
Verifies that exported `.pwn` output compiles cleanly with standard Pawn tooling:
```powershell
& 'C:\path\to\pawncc.exe' local-build\roundtrip-compile.pwn '-iC:\path\to\qawno\include' '-olocal-build\roundtrip.amx'
```

## Internal package verification

The package is still a development candidate. Run the offline packaging tests and inspect prerequisites before creating a uniquely named candidate:

```powershell
python -m unittest discover -s tests/release -p test_package_samp_windows.py -v
python tools/release/package_samp_windows.py --strict
python tools/release/package_samp_windows.py --create-archive --allow-unverified --output local-build/package-check --name ariane-samp-check
```

Packaging refuses an existing staging directory or archive; use a new name instead of deleting previous evidence. `BUILD_INFO.json`, `SHA256SUMS` and `INSTALL.txt` describe the candidate, checksums and installation layout. Without an explicit `--wheel`, the archive contains the editor only; it does not supply the Python CLI. A successful archive/checksum check is not clean-install or client-render validation.

Extract outside the game installation first and read `INSTALL.txt`. The executable must run from the game root. Preserve an existing editor binary by giving the development candidate a separate name; preserve existing fonts before copying supplied fonts. Do not publish until the remaining acceptance gates in the specification are verified.

## Security & Asset Isolation Rules

1. **No Bundled Game Assets**: The Ariane repository and distribution packages must never contain GTA proprietary data (`.dff`, `.txd`, `.col`, `.ipl`, `.ide`, `.dat`, `.img`). Users supply their own legitimate game installation.
2. **No Credentials in Git**: Never commit authentication tokens, private keys, or `runtime-env.json`. The agent engine uses dynamically generated tokens at runtime.
3. **Isolated Test Binaries**: Development testing must use dedicated binary names (e.g. `ariane-samp-objects-dev.exe`) to prevent overwriting the user's primary installation.
4. **Preserve Pinned Dependencies**: Do not advance librw or alter dependency pins without explicit specification updates.
