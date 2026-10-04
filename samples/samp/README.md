# Ariane SA-MP Sample Maps

This directory contains standalone sample map files designed to test and demonstrate static Pawn parsing, live 3D preview, and project persistence in the Ariane SA-MP editor.

## Contents

| File | Description | Calls Exercised |
|---|---|---|
| `streamer_map.pwn` | Streamer dynamic object and material suite | `CreateDynamicObject`, `SetDynamicObjectMaterial`, `SetDynamicObjectMaterialText`, `RemoveBuildingForPlayer` |
| `native_map.pwn` | Native SA-MP object and material suite | `CreateObject`, `SetObjectMaterial`, `SetObjectMaterialText`, `RemoveBuildingForPlayer` |
| `multi_group/structures.pwn` | Multi-file group sample (buildings) | `CreateDynamicObject` (import group: `structures.pwn`) |
| `multi_group/amenities.pwn` | Multi-file group sample (furnishings) | `CreateDynamicObject`, `SetDynamicObjectMaterialText` (import group: `amenities.pwn`) |
| `sample_project.samp.json` | Validated Version 1 project schema | Pre-configured project with objects, materials, removals, and preview filters |

> [!NOTE]
> When importing static Pawn files, Ariane scopes records into groups based on the source file name (e.g. `structures.pwn`). Project files (`.samp.json`) store user-defined group identifiers (e.g. `"structures"`, `"amenities"`).

## Asset Independence Guarantee

- **No Proprietary Assets**: All sample maps reference standard, stock model IDs (such as `19379` — the ubiquitous SA-MP flat sign / cube model, and common building IDs).
- No `.dff`, `.txd`, or `.col` files are packaged or required by these samples.
- The editor renders geometry and textures directly from the user's local GTA installation.

## Usage in Ariane SA-MP Editor

### 1. Importing Pawn Maps
1. Launch Ariane and open the **SA-MP** window (Menu: *SA-MP*).
2. On the **Files & Objects** tab:
   - Type or paste the path to `streamer_map.pwn` (or click **Replace input from clipboard** after copying the file contents).
   - Click **Preview import** to review parsed objects, materials, removals, and diagnostics.
   - Click **Apply import** to add the records to your document.
3. Switch to the 3D viewport to inspect the placed objects and material overrides.

### 2. Multi-File Import
To import multiple files simultaneously into distinct source groups:
- Provide both paths in the files list (e.g. `multi_group/structures.pwn` and `multi_group/amenities.pwn`).
- Ariane automatically scopes each file into its own group and assigns unique, stable object IDs.
- On export, per-group exports produce `SAMP_CreateMap_G1()`, `SAMP_CreateMap_G2()`, etc.

### 3. Opening the Sample Project
1. In the **Files & Objects** tab, select **Open Project**.
2. Browse to `sample_project.samp.json`.
3. The project loads immediately with full group hierarchy, streamer metadata, material overrides, and building removals intact.

## Pawn Compilation

All `.pwn` sample files in this directory are valid Pawn source files. They include standard compilation wrappers (`main()`, `OnGameModeInit()`, `OnPlayerConnect()`) so they can be compiled directly with `pawncc`:

```cmd
pawncc streamer_map.pwn "-ipath/to/qawno/include" -ostreamer_map.amx
pawncc native_map.pwn "-ipath/to/qawno/include" -onative_map.amx
```

## Executed validation (2026-10-03)

All four Pawn files compile. Multi-group files contain minimal `main()` entrypoints and otherwise remain include-style helpers. Native sample emits two bool-tag warnings; all files emit the existing legacy `a_samp` wrapper warning. Import previews report unsupported `Remove*Buildings` helper definitions/calls. The static importer does not execute helpers; inspect diagnostics before accepting supported records. Sample project opens with three objects.
