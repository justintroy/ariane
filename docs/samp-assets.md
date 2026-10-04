# SA-MP asset inspection and texture previews

These read-only engine operations inspect local model data and write PNG previews. They do not edit the SA-MP document, advance its revision, or begin a session. The same operations are available through the `arianectl samp` CLI and `ArianeService.samp` forwarding.

## Preview world, interior, and draw distance

SA-MP world and interior values stay on each document record and in exported Pawn. For viewport rendering, owned SA-MP instances use native area `13` so Ariane's current GTA area cannot cull an object tagged for another SA-MP interior. The persisted document preview filters, or temporary filters requested by `capture_views`, decide which owned records are shown. Streamer `area` and `stream` values remain metadata; the editor does not simulate streamer behavior.

A positive finite `draw` value overrides the stock model's largest draw distance in the renderer, so authored values such as `750` remain visible at that range in Ariane. A zero/default value uses the stock model distance. This only describes Ariane's preview culling; it does not establish matching SA-MP/open.mp client visibility, streaming, or LOD behavior.

```powershell
python tools/agent/arianectl.py samp model_info --params '{"model":19379}'
python tools/agent/arianectl.py samp textures --params '{"model":19379,"limit":25}'
python tools/agent/arianectl.py samp texture_preview --params '{"output_dir":"C:/ariane-previews","texture":{"model":19379,"txd":"all_walls","texture":"mp_shop_floor2"}}'
```

`texture_preview` can also read the same JSON request from `--file`. Choose an `output_dir` writable by the running engine. All three commands return the engine response; invalid requests surface as structured engine errors.

## `model_info`

Pass exactly one selector:

```json
{"op":"model_info","model":19379}
```

`model` must be an integer in `[0, NUMOBJECTDEFS)`. Use `id` instead to select an object record in the open document; the returned geometry and bounds still describe that record's model in local coordinates, without its placement transform. A valid model number with no local definition returns a result with `render_type: "missing"` and a `model_missing` diagnostic. A nonexistent document object ID or an out-of-range model is an error.

The response contains:

- `model`, optional `object_id`, `name`, `render_type` (`atomic`, `clump`, or `missing`), and the model `txd` when defined.
- `origin_axes`, an origin plus unit X/Y/Z axes in model-local coordinates.
- `visual_bounds` and `collision_bounds`. Each has `available`, `min`, `max`, `size`, and `source`. Visual bounds also have `status`: `complete`, `partial`, or `unavailable`. Unreadable native geometry can make the visual result partial; missing geometry or collision data is reported as unavailable instead of guessed.
- `provenance`: `defined`, `installed`, `loaded_before`, `loaded_after`, `renderable`, `availability`, `source_kind`, and `source_archive`. `availability` distinguishes `missing`, `definition_only`, `defined_unloaded`, `loaded_unverified`, `load_failed`, `preview_unavailable`, and `renderable`. `source_kind` identifies `missing`, `definition_only`, `game_archive`, or `modloader_override`.
- `submeshes`: the render object parts, each with its index, name, and geometry material-slot indices. A clump may contain one submesh or several.
- `material_slots`: the union of geometry slots across submeshes. Each slot lists its `surfaces` and contributing `submeshes`. A surface reports `geometry_slot`, submesh index/name, texture name, TXD name when resolved, `original_txd_resolved`, triangle count when readable, and `used_by_geometry` when known.
- `diagnostics` and the SA-MP document `revision` observed by the read.

`provenance.renderable` means the local engine could create an isolated preview object. It does not establish how a separate SA-MP/open.mp client will render the model. A missing original TXD mapping is returned as `txd: null` with `original_texture_dictionary_unknown`; the destination or model TXD is never substituted as provenance.

Common diagnostics include `model_missing`, `model_txd_missing`, `collision_bounds_unavailable`, `visual_geometry_unavailable`, `visual_vertices_unavailable`, `submesh_geometry_unavailable`, `visual_submeshes_unavailable`, and `original_texture_dictionary_unknown`.

## `textures`

Texture indexing is incremental. A request can filter the indexed rows while asking the engine to scan more local models and dictionaries:

```json
{"op":"textures","model":19379,"limit":25,"scan_budget":16}
```

Supported request fields:

- `query`: case-insensitive substring over TXD name, texture name, or associated model name. A numeric query in the valid model range selects that model's rows.
- `model`: one model ID in `[0, NUMOBJECTDEFS)`.
- `txd`: exact case-insensitive TXD name.
- `name`: case-insensitive substring of the texture name.
- `limit`: page size, default `100`, range `1..1000`.
- `scan_budget`: indexing work for this call, default `32`, range `1..128`.
- `cursor`: opaque `next_cursor` from the preceding page, used with the same filters.

Filters combine. For a non-numeric `query` without explicit `model`, `txd`, or `name` filters, model-name-assisted matching indexes at most the first 30 matching model names to keep interactive searches bounded; query matches in indexed TXD and texture names are still included. `complete` describes completion of the global texture index, not that 30-model name-match cap. Use a model ID or TXD filter when you need a narrow exhaustive search. Rows are ordered deterministically by case-insensitive TXD and texture name (original spelling breaks ties), then by associated model ID. A shared TXD texture can therefore appear once per associated model. The response has `textures`, `next_cursor`, `index_generation`, `result_count`, `offset`, `scanned_models`, `total_models`, `indexed_dictionaries`, `total_dictionaries`, and `complete`.

Each texture row has `model`, `model_name`, `txd`, `texture`, `width`, `height`, `available`, `source_valid`, and `usable`. `available` means a readable raster with positive dimensions was found. `source_valid` means the row has a defined associated model; `usable` additionally requires the raster to be available. A row with `model: -1` has no valid model association in the current index and is not a usable model/TXD/texture material source. Wait for `complete: true` before treating a broad search as a complete catalog.

`next_cursor` is opaque and bound to the filters and the matching indexed result set. If filters change, the cursor is stale, or indexing changes the result set, restart the search without a cursor. Do not construct or edit cursors. Common errors are `invalid texture cursor`, `stale texture cursor: filters changed; restart the search`, `stale texture cursor: indexed results changed; restart the search`, and `invalid texture cursor offset; restart the search`. Out-of-range `model`, `limit`, or `scan_budget` values are rejected. An out-of-range numeric `query` returns no model results; it does not become a broad search.

## `texture_preview`

Provide an output directory and exactly one of `texture` or `candidates`:

```json
{"op":"texture_preview","output_dir":"local-build/previews","texture":{"model":19379,"txd":"all_walls","texture":"mp_shop_floor2"}}
```

An individual texture image is composited over a checkerboard. `candidates` accepts 1–64 entries and returns a labeled checkerboard candidate board. Each entry requires a non-empty `texture`; it can include `model`, `txd`, `label`, and ARGB `color`. If `txd` is omitted, it is inferred from a valid non-negative model. Colors accept an integer in `[0, 0xFFFFFFFF]` or a 1–8 digit hexadecimal string, optionally prefixed with `0x`.

The operation creates a unique `samp_texture_preview_*` subdirectory below `output_dir` for each call so prior previews and explicitly supplied files are not overwritten. The response contains `items`, `files`, `board_path` (null for a single texture), `output_dir`, `comparison` (null when absent), and the unchanged document `revision`. Each item includes `model`, `txd`, `texture`, `label`, `kind`, `path`, `width`, `height`, `available`, `source_valid`, and `usable`. A failed item keeps its place in the board and adds a `diagnostic`; its `path` is null.

To compare material candidates on a model, pass `candidates` and `compare` together:

```json
{"op":"texture_preview","output_dir":"local-build/previews","candidates":[{"model":19379,"txd":"all_walls","texture":"mp_shop_floor2","color":"0xFFFFFFFF"}],"compare":{"model":19379,"slot":0,"size":256,"angle":0.65}}
```

`compare.model` must be in `[0, NUMOBJECTDEFS)`, `compare.slot` in `0..255`, `compare.size` in `96..1024` (default `256`), and `compare.angle` finite (default `0.65`). Comparison is only supported with `candidates`. A normal candidate renders the named TXD texture on the requested model slot and returns `kind: "material_on_object"`, `material_source: "candidate_texture"`, `comparison_model`, and `slot`.

`model: -1` is the legal tint-only sentinel. In a comparison it applies the candidate color to the cloned target material while preserving that geometry slot's original texture; the candidate's TXD/texture fields remain metadata and do not replace the slot texture. Its response uses `kind: "tint_on_object"`, `tint_only: true`, and `material_source: "original_geometry"`; `source_valid` remains false because there is no candidate model association. A standalone `texture` preview may use `model: -1` with an explicit TXD to inspect that TXD texture as an image, but that does not make it a valid model material tuple.

All candidate JSON, limits, selectors, and colors are checked before creating the per-call output directory or pushing TXD state. Missing textures or unreadable images are returned as per-item diagnostics, including `texture_unavailable`, `texture_raster_unavailable`, `texture_image_unavailable`, `png_write_failed`, or `material_preview_unavailable: ...`. Invalid shapes, colors, model/slot/size values, and empty names are request errors. The engine restores its message/TXD context and off-screen preview camera/render state after the operation; the document and main camera remain unchanged.

## Verification scope

The Windows development build and direct engine API matrix passed for model bounds/material slots, a shared TXD, clump inspection, a missing model, pagination and stale-cursor rejection, checkerboard and candidate-board PNGs, normal material comparison, tint-only texture preservation, invalid late-candidate prevalidation, and document/session/camera preservation. Evidence is retained in ignored `local-build/20261005-assets/`. This verifies the Ariane editor's local asset and preview behavior only; no SA-MP/open.mp client rendering parity check was performed.
