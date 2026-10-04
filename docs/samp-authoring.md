# SA-MP authoring adapters

The authoring layer adds declarative plans and read-only review to the engine-owned SA-MP document. It is available through the CLI, `ArianeService.samp(...)`, and shared-service dispatch methods such as `samp.resolve_plan`. Run `samp authoring_schema` for the live machine-readable operation and field contract.

## Resolve, review, apply

Inspect the open SA-MP document to get its current revision. Put that exact integer in `expected_revision` in a plan. Resolution inspects asset metadata and texture sources, expands bounded layouts, validates actual material slots, and returns `valid`, diagnostics, computed bounds, groups, and symbolic patch operations. Computed bounds cover created objects plus existing objects affected by transforms/materials or listed in `existing_ids`; deleted objects are omitted. Resolution is read-only: it allocates no stable IDs and changes neither document nor session.

```powershell
python tools/agent/arianectl.py samp inspect
python tools/agent/arianectl.py samp model_info --params '{"model":19379}'
python tools/agent/arianectl.py samp textures --params '{"model":19379,"txd":"all_walls","name":"mp_shop_floor2","limit":64}'
python tools/agent/arianectl.py samp resolve_plan --file samples/samp/interior-perimeter.plan.json
```

The bundled interior and exterior files are templates. Replace `expected_revision` with the inspected revision before resolving. They use isolated elevations and explicit world/interior metadata so review captures are easy to frame. Their measured modular wall is model 19353 (3.212 units long and 3.500 high); the broad model 19379 measures 9.635 by 10.501 units and has no collision bounds, so any sample floor panels using it are decorative visual geometry, not verified walkable/supporting floors. The two inspected texture tuples are `19353 / all_walls / 711_walltemp` and `19379 / all_walls / mp_shop_floor2` (model / TXD / texture). Resolution still verifies installed renderability, local texture availability, and each actual target slot on the current machine.

Keep the full resolver JSON response. Pass it unchanged as `resolved_plan` to `apply_plan`, with the same expected revision if you provide one. The CLI also accepts that response as the entire `apply_plan --file` JSON body:

```powershell
python tools/agent/arianectl.py samp apply_plan --file resolved-plan.json
```

`apply_plan` requires an active scratch session and rechecks both the document revision and the canonical base-document hash. It sends one atomic `samp.patch` request. Created records use plan keys in later operations; the response maps those keys to stable IDs. If transport fails, the adapter inspects the document and history: it retries once only if the original revision and document are unchanged, or reports success if the exact predicted document and one `SA-MP patch` history entry prove the commit. Otherwise it returns an uncertain-result error for manual inspection; there is no replay receipt.

Engine-backed CLI operations preserve the existing `{ok, samp: ...}` response envelope. Python authoring adapters emit their bare operation result. Shared-service `dispatch("samp.<operation>", params)` returns the same bare adapter result.

## Plan schema

A plan is a schema-1 JSON object. `expected_revision` is required. Optional top-level arrays are `objects`, `layouts`, `duplicates`, `removals`, `transforms`, `overrides`, `bulk_materials`, and `deletes`; `groups` adds source group names atomically, and `existing_ids` requests metadata for selected stable IDs. Groups are exact, non-empty source/export strings (spaces and path separators are retained). Symbolic keys are separate: 1–128 ASCII identifier characters, unique within the plan. References must name an earlier create key or an existing integer stable ID. Forward references fail.

An object has `key`, `model`, and either `position` or `relative_to` plus an optional `offset`/`offset_space`; it can also set `rotation`, `group`, `world`, `interior`, `player`, `stream`, `draw`, `area`, and `priority`. `objects` may be an array or a key-to-object mapping. A duplicate has `key` and `source`, with optional `position`, `rotation`, `relative_to`, `offset`, and `group`. A removal requires `key`, `model` (`-1` means wildcard), `position`, and non-negative `radius`. A transform has `target` and any of absolute `position`/`rotation`, `translation`, `rotation_delta`, `pivot`, world/interior/player/stream/draw/area/priority, and `group` fields. Rotation composition uses SA-MP render order `Rz * Rx * Ry`.

Positions and rotations are explicit. `relative_to` plus `offset` uses an existing or earlier plan key as an anchor; `offset_space: "local"` rotates that offset by the anchor's measured Euler rotation. World and interior `-1` selectors mean all records; a non-negative selector includes records tagged with that value plus global (`-1`) records. The adapter does not raycast or infer support, and it rejects `scale`, `support`, `snap`, boolean, and mesh-operation fields. Floor height is the caller's chosen Z coordinate. Review only reports measured visual and collision bounds separately.

Layouts support `row`, `grid`, `radial`, and `perimeter`:

- `row`: `key_prefix`, `model`, `count`, `origin`, and either local `step` or `spacing` with optional `axis`.
- `grid`: `key_prefix`, `model`, `rows`, `columns`, `origin`, and scalar or two-axis `spacing`.
- `radial`: `key_prefix`, `model`, `count`, `origin`, `radius`, optional `start_angle`, `sweep` (up to 360 degrees), and `facing` (`none`, `inward`, `outward`, or `tangent`).
- `perimeter`: `key_prefix`, `model`, `origin`, `width`, and `depth`, with optional `wall_axis` (`0` for model-local X length or `1` for local Y), `spacing`, `rotation`, and up to 128 non-overlapping side openings. A zero spacing fits each measured side extent. Layout rotation rigidly transforms the full 3D positions and orientations around the origin.

`palettes` map names to texture or text materials. A texture material has `model`, `txd`, `texture`, and `color`; `model: -1` is the tint-only sentinel and skips texture-source lookup. Other model/TXD/texture tuples must be verified in the local texture index. A text material has `text`, `font`, optional `bold`, `size` (10–140 by tens), `font_size` (1–255), `align` (0–2), and optional foreground/background colors. `overrides` and `bulk_materials` always validate the target model's actual inspected slot mapping. Unknown or unavailable geometry/assets produce explicit diagnostics; the adapter never invents bounds or collision data.

The default safety limits are 4096 patch operations, groups, and selected records; 2048 objects per layout; 128 perimeter openings and clearance zones; and 500,000 pair checks. Oversized inputs fail before expansion or mutation. Review findings are capped with explicit truncation/coverage diagnostics.

## Assets and review

`model_info` takes exactly one of `model` or document `id`. It reports installation provenance, render type, origin axes, visual bounds, optional collision bounds, actual material slots, diagnostics, and document revision. `textures` supports `model`, `txd`, `name`, `limit`, and opaque `cursor` filters. Cursors are filter- and index-generation-bound; stale/mismatched cursors fail. `texture_preview` accepts one texture or a candidate list; optional `compare` renders candidates on an isolated clone using an inspected model/slot. These are read-only engine operations.

`bounds` reports visual and collision AABBs separately for selected IDs, source groups, and world/interior filters. Missing collision bounds remain explicitly unavailable. `validate_composition` reports conservative visual AABB overlaps, near-coplanar surfaces, explicitly labelled floor/wall gaps, and caller-supplied rectangular clearance checks. Those checks do not prove collision, walkability, route safety, or gameplay behavior.

`capture_views` frames selected model visual bounds and takes overview, plan, and interior captures at a pinned document revision. A `-1` world/interior filter selects all records; a non-negative filter includes that value and global (`-1`) records. It passes those filters to a temporary render override; it does not change the document's preview filters or stable camera. Each capture restores the live camera and render filters, and the manifest records the revision, filters, bounds, and asset diagnostics. Captures fail on stale document/camera revisions or unavailable visual bounds.

`group_inspect`, `group_transform`, `group_clone`, `group_delete`, and `material_bulk` accept exact source group names. Transform, clone, delete, and bulk material updates require a matching revision and active scratch session; each mutation is one atomic patch. Empty groups remain in the exported document's group index. Group transforms and clones rotate around an explicit pivot; inspect and bounds never claim unavailable collision data.

## Runnable CLI recipe

Run from the Ariane checkout with the configured authenticated engine and a Python environment containing the declared dependencies. Use a fresh ignored output directory. The helper below invokes only the CLI and checks its exit code; file output uses UTF-8 without a BOM. It accepts both the engine envelope and bare adapter result. Finish an existing session before switching the scratch scene.

```powershell
$python = 'python'
$cli = 'tools/agent/arianectl.py'
$out = Join-Path (Get-Location) 'local-build/my-interior-review'
New-Item -ItemType Directory -Path $out -ErrorAction Stop | Out-Null
function Invoke-Ariane {
    param([Parameter(ValueFromRemainingArguments=$true)][string[]] $Arguments)
    $raw = & $python $cli @Arguments
    if ($LASTEXITCODE -ne 0) { throw ($raw -join "`n") }
    $value = ($raw -join "`n") | ConvertFrom-Json
    if ($null -ne $value.samp) { return $value.samp }
    return $value
}
function Write-Request {
    param([string] $Name, $Value)
    $path = Join-Path $out $Name
    [IO.File]::WriteAllText($path, ($Value | ConvertTo-Json -Depth 100),
        [Text.UTF8Encoding]::new($false))
    return $path
}

Invoke-Ariane @('assets', 'search', 'chair', '--limit', '8')
Invoke-Ariane @('assets', 'preview', '19379', '--output', (Join-Path $out 'floor.png'), '--size', '320')
$infoFile = Write-Request 'model-info.json' @{ model = 19379 }
Invoke-Ariane @('samp', 'model_info', '--file', $infoFile)
$textureFile = Write-Request 'texture-search.json' @{ model = 19379; txd = 'all_walls'; name = 'mp_shop_floor2' }
Invoke-Ariane @('samp', 'textures', '--file', $textureFile)
$previewFile = Write-Request 'texture-preview.json' @{ texture = @{ model = 19379; txd = 'all_walls'; texture = 'mp_shop_floor2' }; output_dir = $out }
Invoke-Ariane @('samp', 'texture_preview', '--file', $previewFile)
Invoke-Ariane @('scene', 'ariane\my-interior-review', (Join-Path $out 'scratch.ipl'))
Invoke-Ariane @('session', 'begin', 'interior-review')
$inspection = Invoke-Ariane @('samp', 'inspect')
$plan = Get-Content samples/samp/interior-perimeter.plan.json -Raw | ConvertFrom-Json
$plan.expected_revision = $inspection.revision
$planFile = Write-Request 'plan.json' $plan
$resolved = Invoke-Ariane @('samp', 'resolve_plan', '--file', $planFile)
if (-not $resolved.valid) { throw ($resolved.diagnostics | ConvertTo-Json -Depth 100) }
$resolvedFile = Write-Request 'resolved.json' $resolved
$applied = Invoke-Ariane @('samp', 'apply_plan', '--file', $resolvedFile)
$review = Write-Request 'review.json' @{ world = 42; interior = 1 }
Invoke-Ariane @('samp', 'bounds', '--file', $review)
Invoke-Ariane @('samp', 'validate_composition', '--file', $review)
$capture = Write-Request 'capture.json' @{ world = 42; interior = 1; output_dir = $out }
Invoke-Ariane @('samp', 'capture_views', '--file', $capture)

$inspection = Invoke-Ariane @('samp', 'inspect')
$move = Write-Request 'move-props.json' @{ group = 'Interior\North Hall\Props'; expected_revision = $inspection.revision; pivot = @(0,0,0); translation = @(0.2,0,0) }
Invoke-Ariane @('samp', 'group_transform', '--file', $move)
$inspection = Invoke-Ariane @('samp', 'inspect')
$wall = $inspection.document.objects | Where-Object model -eq 19353 | Select-Object -First 1
$tint = Write-Request 'tint-wall.json' @{ expected_revision = $inspection.revision; ids = @($wall.id); slot = 0; material = @{ type = 'texture'; model = -1; txd = 'none'; texture = 'none'; color = 4293252286 } }
Invoke-Ariane @('samp', 'material_bulk', '--file', $tint)
Invoke-Ariane @('samp', 'capture_views', '--file', $capture)
Invoke-Ariane @('session', 'commit')
$save = Write-Request 'save.json' @{ path = (Join-Path $out 'interior.samp.json') }
$export = Write-Request 'export.json' @{ path = (Join-Path $out 'interior.pwn') }
Invoke-Ariane @('samp', 'save', '--file', $save)
Invoke-Ariane @('samp', 'export', '--file', $export)
```

Inspect PNGs and diagnostics before commit. If a step fails, inspect session status and use `session rollback` to discard the full edit session. Commit accepts changes; save and export are separate explicit writes. Reopen the saved project in another session with `samp open --file save.json`, compare the entire `document` with the saved state, then commit or rollback. Repeat with the exterior template using world 43/interior 0 and group `Exterior\Service Yard\Props`. Each plan is additive; start with an empty scratch document when reproducing the documented 40/24 object counts.

Generated Pawn defines map/removal helpers. Include `a_samp` and `streamer`, call the map helper during gamemode initialization and the removal helper for each connecting player. Compile a wrapper with those includes and `main() {}` using the project's Pawn compiler; the local verification emits only the existing legacy `a_samp` include warning. Collision bounds are unavailable for the modular floor/wall assets in the tested installation, so editor screenshots do not establish a walkable map.
