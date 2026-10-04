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

The exterior yard plan intentionally places its `SERVICE ACCESS` text panel in the left south opening. Keep the separate central 5-unit south opening clear as the entrance.

## Executed validation (2026-10-03)

All four Pawn files compile. Multi-group files contain minimal `main()` entrypoints and otherwise remain include-style helpers. Native sample emits two bool-tag warnings; all files emit the existing legacy `a_samp` wrapper warning. Import previews report unsupported `Remove*Buildings` helper definitions/calls. The static importer does not execute helpers; inspect diagnostics before accepting supported records. Sample project opens with three objects.

On 2026-10-05 all four samples and both generated round-trip fixtures compiled again. The native sample now uses boolean arguments for text boldness, removing its two bool-tag warnings. The local legacy `a_samp` include warning remains for all six compilations.

## Declarative authoring plans

`interior-perimeter.plan.json` creates 40 objects: three floor panels, three room perimeters with explicit openings, repeated furniture, two wall palettes and a material-text sign. It uses world 42/interior 1 near Z 1000. `exterior-yard.plan.json` creates 24 objects plus one removal: floor panels, perimeter openings, planters, bollards, a cafe set and an entrance sign. It uses world 43/interior 0 near Z 1500. Both are asset-free schema-1 templates; replace `expected_revision: 0` with the current inspected document revision before resolution.

The exterior removal targets stock model 6048 at its original LA location, separately from the elevated demonstration courtyard. Adapt both placement and removal to an intended site before server use. These plans do not prove player access or collision. Model 19379 floor panels are rotated visual geometry with unavailable collision bounds in the tested local asset installation.

Follow [the authoring workflow](../../docs/samp-authoring.md) for discovery, isolated previews, atomic application, review, revisions, persistence and Pawn export. Generated renders, projects, Pawn wrappers and compiler output remain in ignored local directories. The [verification report](../../docs/samp-verification-20261005.md) records the exact tested scope and outstanding client checks.
