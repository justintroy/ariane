# Ariane SA-MP editor fork

## Objective

Deliver a Windows x64 Ariane fork with a dedicated SA-MP editor window, live object/material/removal previews, static Pawn import/export, versioned project persistence, and equivalent CLI operations via the shared document service. Keep existing cross-platform functionality building. Use open.mp as a behavior reference; do not embed a server or streamer runtime.

This specification is self-contained and idempotent for development across fresh sessions. Read `AGENTS.md`, this file, and `docs/samp-handoff.md` before changing code. This file defines the target and batch acceptance criteria; the handoff records evidence and the exact stopping point. Implemented code is not automatically verified or release-ready.

## Status

- Updated 2026-10-05. Three GPT-6 Luna agents at Max completed the automated continuation; the user excludes computer use. B0/B1/B2 and B9.1/B9.2/B9.3 are complete. Final Windows and hosted Linux builds/core checks pass, as do Python/wheel/package CI jobs in run 37241887826 at source 752e9a9. B9.4 CLI recipes pass. UI, real device reset, internal librw clone allocation, target-client comparison and clean-install/publication gates remain explicit. See [the current verification report](docs/samp-verification-20261005.md) for exact evidence; overall release acceptance remains partial.
- Latest Windows x64 D3D9 build and automated renderer tests pass. Binary SHA-256: `314D1CDE66BD1E685C05234D681729596A1B3AFFB9D4802F6EC7BEC11A1D5E77`. Core: 132 document checks and 10 orientation checks. October 5 renderer/removal/clipboard and asset-preview evidence is in the current report; earlier UI evidence retains its original binary/date scope.
- Fixed stale missing-model UI warnings: the banner derives from current document records instead of persisting in the operation-status message. Live captures verify model 29999 warns, Undo clears it, and rollback clears it. Structured diagnostics also pass Undo/Redo checks. Complete snapshot restored; all 217 baseline world-file hashes unchanged.
- Current Python agent suite: 122 tests, one Unix-only transport skip; all other tests pass. Focused authoring/shared-service parity coverage: 30 tests. SA-MP MCP was dropped by the user on 2026-10-03; unrelated MCP support remains.
- October 3 evidence remains applicable to unchanged core/rotation code: 105 document checks, 10 orientation checks, generated Pawn fixtures and four sample compilations. The October 4 D3D9 B3 probe passed 300 atomic recreations, 150 model-3120 clump recreations and 150 replacement/delete/Undo cycles. Materials and renderer caches survived repeated address reuse; all compared non-frame resource counters balanced.
- Native UI checks on the latest test binary verified placement, row selection versus checks, Duplicate/Undo, texture search/material application/Undo, file-dialog Cancel, bulk-delete Cancel/safe Enter, and stale-delete rejection after a concurrent document change. The stale dialog showed a changed-document warning and disabled Delete.
- Selected vanilla `mall_laW` (instance 8816/model 6048) by double-clicking its Selection row. IPC confirmed SA-MP mode active; Tools showed Gizmo enabled and Translate selected. The user's screenshot shows the same mall selected with a translation gizmo visible. The user separately confirms that with an SA-MP document enabled, gizmo transforms, clipboard copy and Delete operations are disabled. Record those as user-reported guard passes; the screenshot confirms visual gizmo presence, not whether dragging is enabled in that state. The earlier off-center no-gizmo observation is not a suppression result. Cut/Paste behavior and a transform drag were not separately confirmed. Rollback left the scratch document empty and inactive; all 217 baseline world-file hashes match. D3D9 reset, allocation-failure injection, client render parity, non-Windows compilation and clean-install verification remain open.
- User-reported September 28 manual checks remain historical: large clipboard/import, preview invalidation, row selection versus checks, Cancel/safe Enter, bulk deletion/Undo and scrolling passed. Do not repeat without relevant changes.
- Development checkpoints through `752e9a9a5f924c2591109b129d15a0610e47daf6` are pushed to `justintroy/ariane`; the final documentation checkpoint is recorded in Git history. All four CI jobs pass at 752e9a9. No release is published; never push to Dryxio upstream.
- Exact evidence, runtime state, package results and next actions are recorded in the newest `docs/samp-handoff.md` entry. Older batch evidence below retains its original scope and date.

## Repository and references

- Working checkout: `C:/Users/PC/Documents/ariane`, beside `Roleplay-Project-v2`.
- Upstream: https://github.com/Dryxio/ariane
- Intended fork: https://github.com/justintroy/ariane
- Branch: `codex/samp-support`.
- Base tag: `v1.40.9-agent-alpha.1`.
- Continuation base checkpoint: `22dcc42f685566f8313c79299e84ba33ece6e81e`; use Git history for the latest source commit.
- Pinned librw: `15ffa585216a9a7573ecc597b19ce2fde9b935f2`, local sibling `ariane-librw`.
- Behavior reference: https://github.com/openmultiplayer/open.mp ; verify native signatures and client behavior against primary references when implementing remaining details.
- Workspace guidance: `../Roleplay-Project-v2/AGENTS.md`, `../Roleplay-Project-v2/docs/spec-driven-development.md`, and `../Roleplay-Project-v2/tools/gta3dai/AGENTS.md`.
- Verified user fork access on 2026-10-04: `fork` points to `git@github.com:justintroy/ariane.git`. Existing `origin` remains Dryxio for upstream fetching; never push there. `remote.pushDefault` and `branch.codex/samp-support.pushRemote` both select `fork`. The recorded October 4 checkpoint is pushed; later evidence belongs to its own source checkpoint.

## Scope and non-goals

Windows is the first supported material-text renderer. Use stock GTA and locally supplied SA-MP assets. Import static map code only. Never execute Pawn or download custom models. Attachments, moving/animated objects, runtime condition evaluation, and streamer simulation are out of scope. Preserve unsupported input through diagnostics and unchanged source files, not by inventing equivalent behavior.

Do not rewrite imported scripts implicitly. Do not bundle game assets, local catalogs, authentication tokens, or generated texture caches. SA-MP saves must never use IPL/IMG world-save paths. Vanilla world objects remain independent of the SA-MP document.

## Required behavior

### R1: Document and editing

Maintain a separate document with stable record IDs, source-file groups, model IDs, XYZ positions, equivalent rotations, creation arguments, material slots, and removal records. Reuse selection, placement, gizmos, duplication, deletion, and undo/redo. Each completed edit gesture is one undoable action. Movement changes generated creation coordinates; deletion removes generated creation code. Duplicate objects retain material overrides without sharing mutable material ownership.

Store streamer fields: world, interior, player, stream distance, draw distance, area, and priority. World/interior filters affect preview only. Do not simulate streaming or silently discard metadata. Stable IDs must survive save/reload and remain immutable during updates. Validate the entire proposed mutation before installing it.

### R2: SA-MP window

Use one window named **SA-MP**, with these tabs:

1. **Files & Objects**: multiple file paths, pasted code, import preview and diagnostics, apply supported records, project open/save, export destination/group, object selection and transforms, world/interior preview filters.
2. **Textures**: locally indexed model/TXD/texture search, thumbnails, material slot selection, ARGB tint, live application. Incremental indexing must not freeze the editor. Search every model sharing a TXD, not only a representative model.
3. **Material Text**: text, material size, font, font size, bold, foreground/background ARGB, alignment, multiline and supported inline color previews.
4. **Removals**: model/XYZ/spherical radius, create from a selected world instance, affected-instance preview, edit/delete/undo and restoration.
5. **Code & History**: current normalized exportable code and undoable action history with code snapshots. Clearly label snapshots as historical state, not executable incremental patches.

The window must remain reopenable. UI actions use the same validated document service as CLI. Display missing assets/fonts and unresolved import diagnostics without dropping original parameters.

### R3: Rendering and material ownership

Material overrides belong to an object instance, never to the shared model. Clone or otherwise isolate mutable geometry/material state safely. Keep originals intact. Reapply overrides after duplication, undo/redo, reload, model replacement, and renderer resource recreation. Replace slots cleanly and release generated resources without leaks or stale pointer cache hits.

Use Windows font rasterization for text. Preserve text settings even if a font is unavailable. Support explicit newlines and inline colors; validate alignment, wrapping, alpha, size and font behavior against actual SA-MP/open.mp client output. Record visual differences rather than claiming exact compatibility without evidence. Non-Windows builds must compile with explicit unsupported-preview diagnostics where necessary.

Convert Pawn rotations to/from Ariane quaternions with orientation equivalence, including mixed-axis and near-singularity cases. Numeric Euler equality is not required when orientations are equivalent.

### R4: Building removals

Preview removals by model ID, XYZ and spherical radius, including wildcard model IDs and overlapping records. Use vertical separation in the distance test. Undoing one overlapping removal must leave an instance hidden if another still affects it. Distinguish normal and LOD instances. Do not silently create/export extra LOD removals. Preserve vanilla files and transforms throughout selection, removal preview, undo and export.

### R5: Static Pawn import

Accept multiple `.pwn` files and pasted code. Support:

- `CreateDynamicObject`, `SetDynamicObjectMaterial`, `SetDynamicObjectMaterialText`, `RemoveBuildingForPlayer`.
- Native equivalents `CreateObject`, `SetObjectMaterial`, `SetObjectMaterialText`.
- Multiline calls, line/block comments, escaped strings, numeric literals, constant definitions, simple constant expressions, variable assignments, and constant-index array references.
- Resolve materials by object reference and source order, including reused temporary variables and aliases. Keep source scopes/files from accidentally sharing references.
- Parse native and dynamic material-text argument order separately. Apply correct API defaults and preserve creation metadata.

Do not execute Pawn. Report file/line diagnostics for malformed input, unresolved expressions, runtime control flow, ambiguous references and unsupported map operations. Define and document the exact accepted expression operators and directives from the implementation and tests. Prevent numeric overflow/invalid casts and partial mutation on errors.

Preview the proposed document and diagnostics without changing state. Apply explicitly accepted supported records in one undoable transaction. Reimport is additive by design; a repeated development session must not blindly replay an import. Use inspect/revision checks or open a saved project to resume.

### R6: Persistence and export

Save versioned `.samp.json` data containing document records, groups, asset paths and preview settings. Exclude generated caches and game assets. Reject unknown versions with a clear error. Validate before replacing current state. Use atomic file replacement where supported.

Export normalized Pawn as a combined file or per-group files, and support clipboard copying. Combined output contains `SAMP_CreateMap()` and `SAMP_RemoveBuildings(playerid)` with callback integration instructions. Per-group output must avoid function-name collisions or explicitly provide a safe integration strategy.

Emit dynamic object/material calls and `RemoveBuildingForPlayer`. Use stable ordering, deterministic variable names, escaped strings and explicit nondefault arguments. Preserve all supported semantics across import, save/reload, export and reimport. Never imply that server streamer includes are unnecessary: the editor does not need a streamer runtime, but dynamic-call exports need streamer includes/plugin in the consuming server.

### R7: Shared API, sessions and agents

One C++ document service owns validation, mutation, history, persistence and code generation. UI, `arianectl samp`, and Python service forward to that service (SA-MP MCP support is dropped; CLI and shared service are sufficient, while unrelated existing MCP tools remain intact). Provide structured results, stable IDs, source diagnostics and document revisions. Reject stale `expected_revision` without mutation.

Operations: import preview/apply, inspect, place, update, duplicate, delete, replace, clear, material, removal, textures, open, save, export, undo, redo, patch, preview, and window.
- `preview`: document preview filter mutation (modifies document preview filter state such as virtual world and interior, requiring an active session when changing preview state).
- `window`: editor GUI window visibility inspection and toggling (e.g. querying state or toggling ImGui SA-MP editor window visibility; UI presentation state only, non-document-mutating).

Require existing agent sessions for mutations. Session commit accepts changes; it does not save a project or export Pawn. Saving/exporting remains explicit and is currently disallowed while a session is active. Rollback restores objects, materials, removals, code and undo/history state consistently. Checkpoints include SA-MP state. Failed patches/checkpoint restoration must not leave partial state. Handle retries deliberately: replaying a mutation is not automatically idempotent just because it is atomic. Integrate with existing patch replay/receipt behavior or document a safe revision-based retry protocol.

Do not let generic agent scene commands bypass SA-MP ownership or invalidate rollback. Verify switching scene/session mode, generic place/transform/delete, and simultaneous UI changes while an agent session exists.

### R8: Agent-authored maps and interiors

Agents must be able to discover locally available objects and textures, inspect their rendered appearance and dimensions, compose an interior or exterior map, apply instance materials, inspect the result from useful views, revise it, and save/export it through the CLI. Adopt the scriptable build/inspect/render/iterate workflow that motivates blender-cli; do not add a Blender dependency or execute arbitrary agent code inside the editor.

Here, custom maps/interiors mean compositions of stock GTA and locally supplied SA-MP objects with texture, tint and material-text overrides. New DFF/TXD geometry, custom-model downloads/registration, mesh editing and arbitrary UV editing remain outside this batch. Preserve world/interior/player/stream/draw/area/priority metadata. No operation may route SA-MP records through generic IPL scene mutations.

Existing discovery, asset previews, camera/capture, document patches and material setters are foundations, not missing features to rebuild. Add the SA-MP composition and inspection adapters described in B9, with CLI/shared-service parity, stable IDs, atomic mutation and explicit persistence.

## Progress and batch acceptance

Each batch is independently resumable. Start by inspecting existing code and evidence; do not recreate working features. Mark a batch complete only after its acceptance checks pass. Keep failed/unavailable checks explicit.

### B0 — Baseline, fork and reproducible workspace (COMPLETE)

- [x] Separate checkout and requested branch/base.
- [x] Pinned librw checkout.
- [x] Local Windows toolchain builds the editor.
- [x] Verify fork exists; configure upstream/user remotes without losing existing settings. SSH access to `justintroy/ariane` verified; `fork` is the default push destination, with existing `origin` retained.
- [x] Convert necessary local build knowledge into portable tracked instructions/scripts. `tools/build/build_windows.ps1`, `run_samp_core_test.ps1` and the usage guide record dependency/toolchain detection and explicit path overrides; both scripts run successfully locally. Validation CI is tracked without changing the pinned dependency.
- Acceptance: base/dependency SHAs recorded; clean reproducible build procedure; no game assets committed; publishing targets only `justintroy/ariane`.
- Handoff: exact toolchain commands, paths, remote status and dependency pins.

### B1 — Document, parser and normalized code (COMPLETE)

- [x] Initial shared document, parser, validation, export and standalone tests exist.
- [x] Initial multi-file, native/dynamic, reused reference and expression fixtures pass.
- [x] Audit scopes, ambiguous references, malformed input, numeric overflow and unsupported runtime behavior within the documented static subset. Unsupported runtime constructs produce diagnostics; nested runtime semantics remain out of scope.
- [x] Verify supported call signatures/defaults against open.mp and streamer primary references. Client rendering parity remains in B4/B7.
- [x] Ensure diagnostics and exported records preserve source association through file/line diagnostics, record groups and generated source comments.
- [x] Fix per-group integration collisions. Group functions use stable project-group index suffixes; combined two-group fixture compiles and output remains stable after save/reload.
- [x] Reviewed split need. Current document source remains one compilation unit while interfaces and 80-check standalone suite stay small enough to maintain; split if future changes grow it.
- Acceptance: fixtures cover both API families, defaults, constants, arrays, aliases/reuse, interleaved materials, multiple files, escaping, malformed input and runtime logic; rejected input leaves document unchanged.
- Handoff: exact supported subset and diagnostics; fixture commands/results; any intentionally unsupported syntax.
- Evidence: 80 core checks and Windows x64 D3D9 build passed; combined and two-group Pawn fixtures compile with only the local legacy `a_samp` include warning.

### B2 — Persistence, history and atomic transactions (COMPLETE)

- [x] Version 1 projects, atomic writes, undo/redo, snapshots and staged patches exist.
- [x] Test invalid version/schema examples, stale revisions, ID exhaustion and failed multi-operation patches; all leave document unchanged.
- [x] Stage and validate `restoreSnapshot` before installing data/history; a malformed undo snapshot now leaves the document and revision unchanged.
- [x] Bound commit/undo/redo history to 64 actions and include affected-record before/after snapshots in API and UI.
- [x] Define revision-based retry protocol; replay of a successful request with the same `expected_revision` is rejected. Inspect and compare state after an uncertain response.
- [x] Verify save/reload for all stored streamer fields, texture/text materials, removals, asset paths and preview filters.
- Acceptance: semantic round trips; no partial mutation; all IDs/materials/removals stable after reload/rollback; project paths cannot enter world save paths.
- Handoff: schema contract, retry contract, regression evidence.
- Evidence: 103 core checks, Windows x64 D3D9 build, 92 agent tests with one skip. Project save uses `.samp.json`; Pawn export uses `.pwn`; neither writes IPL/IMG.

### B3 — Viewport object integration and ownership (IMPLEMENTED; VALIDATION PARTIAL)

- [x] SA-MP instances, selection, placement, transforms, duplication/deletion hooks and renderer guards exist.
- [x] Text renders on a live SA-MP object; a shared-model geometry reset bug was fixed.
- [x] Verify different text materials on identical models in a live editor screenshot. Material clearing and undo also passed in the live smoke.
- [x] Audit cut/copy/paste routing, duplication, model changes, material clearing and geometry cache invalidation in code. Clipboard copies now snapshot document rows so a rebuilt source pointer cannot become a paste source.
- [x] Test quaternion equivalence with mixed-axis rotations and singularities (10 checks).
- [x] Guard SA-MP transform and delete paths against vanilla instances in code; a representative loaded LA IPL hash was unchanged after live smoke.
- [x] Verify slot isolation and restoration through atomic resource recreation. The current-build live probe passed 300 recreation cycles across two instances and text/tint/Undo states, with balanced allocation counts.
- [x] Run document-driven atomic model replacement, deletion and Undo with resource accounting. The current-build probe passed 150 cycles with stable instance identities, released deleted resources and balanced counts.
- [x] Exercise a real clump model through destroy/recreate and immediate renderer-address reuse. Model 3120 passed 150 live cycles across import, tint and Undo checkpoints; cloned atomics retained topology and independent geometry/materials, source signatures were restored, and non-frame resource counters balanced. Renderer addresses were reused in 242/300 atomic and 23/150 clump cycles; the material cache cleared on destruction and rebuilt after each clone. The pinned librw frame counter rises by 150 per 50-cycle run because `Frame::destroyHierarchy` frees frames without decrementing that counter.
- [x] Exercise native UI placement, row/check behavior, Duplicate/Undo, texture search/material/Undo, file-dialog cancellation, bulk-delete cancellation and stale-delete rejection after concurrent document mutation; see the 2026-10-04 UI handoff.
- [x] User confirms vanilla gizmo transforms, clipboard copy and Delete are disabled while an SA-MP document is enabled; recorded as a user-reported guard pass. The screenshot separately shows the gizmo visible, but does not establish whether dragging is enabled in that state.
- [ ] Confirm viewport Cut/Paste behavior while an SA-MP document is enabled; the user confirmation covered clipboard copy and Delete, not Cut/Paste.
- [ ] Exercise D3D9 resize/device reset and internal renderer allocation/clone failures. October 5 live probes pass caller-side `atomic_clone`, `atomic_frame`, `clump_clone` and `clump_root` null-return injection with cleanup and recovery. Real device reset and allocations inside pinned librw's `Clump::clone` remain unverified.
- Renderer recreation evidence: `tests/samp/renderer_validation.inl`, enabled by `ARIANE_SAMP_VALIDATION=1` and a scratch session, checks atomic material isolation/restoration and clump topology/ownership. Current ignored outputs are `local-build/b3-renderer-followup-results-r2-20261004.json` and `local-build/b3-clump-renderer-results-r3-20261004.json`.
- Acceptance: ownership isolation and restoration across undo/reload/recreation; no world file changes; no leaked or stale renderer resources.
- Handoff: screenshot/test evidence, ownership lifecycle and remaining renderer differences.
- Evidence: Windows x64 build; 103 core checks; 10 orientation checks; live RED/BLUE shared-model screenshot, duplicate/model-change/material-clear/undo/rollback smoke; `LAe2.ipl` SHA-256 unchanged. These checks do not establish full acceptance.
- Follow-up: Windows ImGui text boxes now use the Unicode system clipboard, including SA-MP Pawn and material-text inputs. Build passed; interactive copy/paste verification remains open.
- User follow-up: Files & Objects input now grows beyond 64 KiB and offers an explicit Paste clipboard button. Build passed; direct UI behavior remains unverified.

### B4 — Texture browser and material-text editor (PARTIAL)

- [x] Incremental texture indexing, thumbnail UI, tint/slot controls and GDI text rasterization exist.
- [x] Structured `samp inspect` asset diagnostics exist for missing models, TXDs, textures and font preview substitution/unavailability.
- [x] API texture search covers shared TXDs: full index completed across 40,000 models and 2,704 dictionaries; models 19379 and 19353 returned matching 34-texture results from all_walls.
- [x] API inspect reported model_missing, txd_missing, texture_missing and font_substituted. Missing model/material parameters survived .samp.json save/open.
- [x] API document matrix preserved texture/text ARGB values, every material size from 10 through 140, font names and sizes, both bold values, multiline and inline-color text, and all three alignments. A live Ariane capture shows multiline inline colors.
- [ ] Verify indexing completion, shared-TXD search, missing-asset diagnostics and selected-instance application through direct UI interaction.
- [ ] Verify rendered alpha blending and the full font/size/bold/alignment matrix; API value checks and one live preview do not establish pixel parity.
- [ ] Compare representative text/texture renders with an actual SA-MP/open.mp client.
- Acceptance: controls change only the selected instance; parameters survive missing assets and reload; documented client comparison results.
- Handoff: reproducible visual fixtures and explicit differences/unavailable checks.

### B5 — Removals and complete window workflow (PARTIAL)

- [x] All five tabs, editable removals and spherical preview exist.
- [x] Files & Objects includes per-object checks, select/deselect all, delete checked and delete all; bulk deletes preserve non-object document state and use one undoable replace action. Windows build and 103 core checks passed; direct UI interaction remains unverified.
- [x] Test boundary/vertical/wildcard/LOD/overlap cases and undo restoration. October 5 direct renderer probe passes 12 checks at each of three document checkpoints; all 217 world-file hashes match.
- [ ] Route preview filters and every UI mutation through the shared service.
- [ ] Verify reopen behavior, clipboard code copy, multi-file preview/apply and per-group export UI.
- [ ] Perform UI layout and interaction checks; optional GUI-inclusive capture is built but untested.
- [ ] Verify large Pawn paste by Ctrl+V and the explicit replacement button, empty/non-text clipboard feedback, preview invalidation, and checkbox/select-all/delete/undo behavior with multi-object maps. Clarify checked rows versus viewport selection; avoid accidental bulk deletion.
- Acceptance: end-to-end window workflow, correct overlapping removal visibility, unchanged world files, history clearly distinguished from current code.
- Handoff: UI verification evidence and any environment blockers.

### B6 — CLI and shared service, sessions, checkpoints and parity (PARTIAL)

- [x] CLI operations, session snapshots and checkpoint fields exist (SA-MP MCP dropped).
- [x] Initial live API import/rollback smoke test passes.
- [x] Add explicit CLI/shared-service parity tests for every operation and structured error.
- Forwarding tests cover all 21 operations through CLI and shared service dispatch and pass in the October 4 run (97 tests, one Unix-only skip). Live engine session, stale-revision, patch, save/export, scene-switch and ownership rejections passed on the October 3 build; see the handoff for exact evidence.
- [x] Test full transaction rollback/checkpoint restoration with objects, texture/text materials, removals, code, undo/redo/history and active state. Checkpoint restore preflights corrupt SA-MP snapshots before mutation.
- [x] Audit generic agent scene operations and session switching while SA-MP mode is active. Active-session scene switching rejects; generic place/transform/delete and related generic mutations reject while SA-MP editing is active.
- [x] Test patch retries and failure recovery; ensure commit never saves implicitly.
- [ ] Test direct UI mutation while an agent session is active. Computer-use remains deferred; do not repeat the eight passed manual checks.
- Acceptance: existing agent tests plus SA-MP parity/transaction/checkpoint tests pass; no session bypass; save/export explicitly separated.
- Handoff: exact automation usage, request schemas, session lifecycle and test evidence.

### B7 — Regression, compatibility and client verification (PARTIAL)

- [ ] Complete integration with existing vanilla editor selection, gizmos, shortcuts and shared viewport workflow while preserving separate SA-MP document ownership and world-save guards. This is required before release; schedule after SA-MP UX gates.
- [x] Run complete parser/core suite and compile exported fixtures with Pawn and streamer includes. October 5: 132 core/10 orientation checks, four samples and two round-trip fixtures pass.
- [x] Run existing agent suite and new SA-MP integration suite after final changes. October 5: 122 tests, one Unix-only skip; all others pass. Both generated authoring exports also compile.
- [x] Build existing non-Windows targets; add/update CI without changing the pinned dependency. Ubuntu 24.04 OpenGL and document core checks pass in run 37241887826 at source 752e9a9. Initial runs exposed the Premake glibc requirement and a private POSIX `sync()` name collision; both are fixed without changing librw.
- [ ] Run complete rendering/removal matrix and compare representative client renders.
- [x] Check world-file hashes before/after tests, resource recreation and clean launch. All 217 baseline files match in the latest renderer run.
- Acceptance: evidence includes commands, revisions, toolchain, pass/fail/skip counts and screenshots; unavailable checks are release limitations, not passes.
- Handoff: verification report and release risks, with remaining failures assigned to a batch.

### B8 — Documentation, package and publication (PARTIAL)

- [x] Include the B9 authoring workflow and its verification artifacts in documentation/package. The audited development candidate includes the authoring/assets guides, both plans, CLI wheel and verification report; verified release delivery still requires B9.4 client acceptance.
- [x] Finalize root Ariane `AGENTS.md` and `docs/samp-usage.md` with tested setup/UI/CLI examples and supported Pawn subset. Authoring and asset guides include live schemas, runnable recipes, tested limits and explicit remaining acceptance gates.
- [x] Keep navigation links in Roleplay root and gta3dai `AGENTS.md`; label experimental until release acceptance passes.
- [x] Add small sample maps exercising all requested calls, with no game assets. Existing Pawn fixtures plus both declarative plans are tracked sources.
- [x] Package Windows x64 editor, necessary fonts/runtime/tool scripts, dependency notices, setup guide and verification report. Development candidate `local-build/package-check-20261005.zip` passes CRC, all 24 checksums and forbidden-content audit (25 entries); clean-install and publication remain unchecked below.
- [ ] Verify package from a clean local installation and record SHA-256.
- [ ] Commit reviewed changes, push `codex/samp-support` to the user's fork and publish the Windows build.
- Acceptance: working download/package and branch URLs, complete usage instructions, no secrets/game assets, precise remaining visual limitations. Do not claim release completion if publication or required verification remains blocked.
- Handoff: published commit, artifact URL/hash and final known limitations.

### B9 — Agent map/interior authoring through the CLI (IMPLEMENTED; ACCEPTANCE PARTIAL)

**Outcome:** An agent can build and refine a complete SA-MP interior and exterior map using available objects and material textures, without manual UI editing or generic scene ownership workarounds. Visual quality requires inspection and revision; a successful export alone is insufficient.

**Dependencies and order:** Reuse B1/B2 document semantics and B6 session safeguards. Build on B3 material ownership and B4 local indexing/diagnostics; their remaining acceptance gates still apply. Implement the bounded units below in order, then rerun B7 and include the workflow in B8. Do not mark any earlier batch complete from B9 evidence alone.

**Code audit baseline (2026-10-05):** `arianectl samp` exposes 21 document/editor operations, including `place`, `material`, `duplicate`, `patch`, `textures`, `preview`, `inspect`, `save` and `export`. Generic `assets`, `discovery` and `catalogue.*` provide model search, previews and candidate boards. Camera commands and captures already exist. `samp textures` returns names/model associations and indexing progress, but no CLI texture image export, pagination or slot geometry inspection. `asset_detail` provides collision bounds when present; that is not a complete visual geometry/material-slot contract. Generic scene groups, relative placement, support snapping, bounds and composition validation use generic scene identities; their mutations reject active SA-MP editing. Raw document patches can already construct maps, but provide no symbolic references for newly placed records or reusable SA-MP layout workflow. These are audit findings, not newly validated runtime defects.

#### B9.1 — Discover and inspect usable assets/materials (COMPLETE)

- [x] Reuse existing asset/discovery/catalogue commands. Expose source provenance, installed/defined/renderable status and indexing coverage for locally supplied SA-MP model IDs as well as stock GTA models. Missing semantic descriptions must not hide usable assets or imply exhaustive semantic coverage.
- [x] Add read-only `samp model_info` for a model or document object ID: model name/TXD, visual local bounds, collision bounds separately, origin/axes, material slots and their original texture references. Identify atomic/clump submeshes sharing each slot. Report absent collision, unavailable geometry and unsupported slot mapping explicitly; do not invent dimensions or claim that all slots 0–15 exist on every model.
- [x] Extend `samp textures` with deterministic pagination and structured model/TXD/name filters while preserving existing query behavior and incremental progress. Results identify valid material source model/TXD/texture tuples, dimensions and availability. Reject stale cursors if the indexed result set changes.
- [x] Add read-only `samp texture_preview` to export one texture or a labeled candidate board as PNG under an explicit output directory, showing alpha over a checkerboard. Reuse model preview/contact-sheet facilities for objects and provide material-on-object comparison views using isolated scratch state. Preserve the current document, history, session and camera after previews; surface unavailable assets instead of downloading them.
- [x] Cache thumbnails only in ignored local directories. Preserve legal material sentinel values supported by the existing API; do not confuse a material source model ID with the destination object model.

#### B9.2 — Resolve layouts and apply one atomic document edit (COMPLETE)

- [x] Add `samp resolve_plan` and `samp apply_plan` through CLI and shared Python service. A declarative plan contains document revision, existing stable IDs, new object keys, explicit model/creation fields, groups, transforms, material palettes and overrides. Resolution is read-only and returns expanded operations, computed bounds, diagnostics and a key-to-record reference plan. Application returns the actual key-to-stable-ID mapping after one successful commit.
- [x] Support references to newly placed objects in the same plan so materials, duplicates and relative placement need no intermediate ID polling. Compile to the shared C++ document transaction; install no partial state. Do not allocate durable IDs or consume `next_id` during resolution. Require an active session and matching `expected_revision` at application; reuse the B2 uncertain-response retry protocol.
- [x] Add bounded declarative layout primitives for rows, rectangular grids and perimeter walls with explicit openings. Supply spacing/count, origin, orientation, floor elevation and separately specified support objects; reject inferred support/snap fields. Use true model origins and rotated bounds; support mixed-axis rotations with the existing quaternion conversion. Reject non-finite values, excessive expansion, unresolved references/cycles and unsupported geometry operations before mutation.
- [x] Add SA-MP group inspect/transform/clone/delete and bulk material application to selected stable IDs/groups. Preserve existing source/export groups; distinguish layout membership from source groups if separate membership is needed, with a documented backward-compatible schema strategy. Transform around an explicit pivot, clone with new IDs and independent materials, preserve creation metadata, and make each action one undo entry.
- [x] Named material palettes resolve to validated texture/text/tint settings for explicit slots. Slot targeting is inspectable and never silently applies one material to every surface. Repeated props and module clones retain overrides without mutating shared models.

#### B9.3 — Inspect, validate and revise the composed map (COMPLETE)

- [x] Add read-only `samp bounds`, `samp validate_composition` and `samp capture_views`, accepting stable IDs/groups and explicit world/interior preview filters. Use SA-MP document records; never derive an empty map from generic `scene_bounds` or generic group state.
- [x] Report visual and collision bounds separately; flag unresolved assets/material slots, near-coplanar overlapping surfaces, unsupported placements, possible wall/floor gaps and specified entrance/walkway clearances. Make overlap/gap checks configurable so intentional intersections remain possible. Geometry heuristics are warnings, not proof of collision correctness or client walkability.
- [x] Frame actual transformed SA-MP bounds for overview, plan and interior eye-level captures, with explicit pose overrides for occluded rooms. Record camera, document revision, filters and asset diagnostics alongside images. Preserve camera/environment state unless the caller explicitly requests a change; stale revisions reject mixed-state reviews.
- [x] Reuse existing raycast/camera/capture facilities only after auditing that they observe SA-MP instances correctly. Add an SA-MP adapter where generic facilities omit them. Make unsupported collision/raycast results explicit rather than reporting successful clearance.
- [x] Document machine-readable request/response schemas and runnable JSON plans with a discovery, preview, resolve, apply, capture, revise, commit, save and export recipe. Commands named above are proposed B9 APIs until implemented and tested; publish capability/schema discovery for their supported fields and limits.

#### B9.4 — End-to-end acceptance and delivery evidence

- [x] Build two tracked, asset-free sample plans: a furnished multi-room interior with openings, repeated modules, texture palettes and a material-text sign; an exterior entrance/courtyard with props and an explicit building removal. Include at least two differently textured instances of the same model and nondefault world/interior metadata. Local renders/assets remain ignored.
- [x] Run both recipes entirely through CLI: discover/render candidates, inspect slots/bounds, resolve/apply layout, capture useful views, revise one layout and one material, commit, save `.samp.json`, export Pawn, reopen and compare semantic state. Record model choices and inspect renders for seams, openings, scale and palette coherence; document unresolved visual issues. Do not substitute autogenerated “beauty” scores for visual review. Final recipes preserve both text signs; collision and client pixel parity remain separate gates.
- [x] Tests cover CLI/shared-service forwarding, new-object references, rotated/pivot layouts, material ownership, metadata retention, invalid slots/assets, stale revisions, expansion limits, failed plans, retry recovery, one-action Undo/Redo, full rollback and save/reload. Read-only inspection/previews must not alter document/history or generic scene/world state. Validate missing-collision behavior and indexing pagination.
- [ ] Compile both exports with the Pawn compiler and streamer includes. Compare representative texture/text/slot results and entrance/collision behavior in a controlled SA-MP/open.mp client; record renderer/client differences and unavailable checks. Reuse B4/B7 evidence where unchanged behavior applies.
- [ ] Record exact commands, schemas, revisions, counts, captures, test results and unchanged world-file hashes. Update usage, handoff and package content. Do not bundle local game assets or mark B9 complete until both CLI recipes and required validation pass.

**Implementation map:** Extend `tools/agent/arianectl.py` and `service.py`; split SA-MP planning, layout and review helpers into focused modules rather than expanding generic scene methods. Add engine inspection/preview adapters in focused files beside `samp_editor.cpp`; keep mutation/history/export in `samp_document.*`. Add focused `tools/agent/tests/`, `tests/samp/` and sample plan fixtures. Update build inputs when adding C++ files. Any persistence change must preserve B2 schema/version guarantees.

**Stopping point:** B9 asset inspection, symbolic plans, layouts/groups/palettes, composition diagnostics and revision-pinned captures are implemented. Core/Python/live asset checks and both semantic CLI recipes pass. The final Windows build and all 33 metadata/visibility probes pass; corrected captures show both sample compositions and retain both text signs. Windows/Linux/Python/package CI passes at 752e9a9. Client rendering/collision, real device reset, internal librw allocation failure, direct UI shortcuts and clean-install/publication gates remain open. See the current verification report and newest handoff for exact commands and artifact scope.

## Idempotent session protocol

1. Read guidance, this specification and the handoff. Inspect `git status`, branch, HEAD and remotes. Preserve pre-existing modifications.
2. Compare current files/results with the recorded handoff. If they differ, update the handoff before continuing. Never reset/clean the working tree to match a stale note.
3. Choose the first incomplete dependency for the current batch. Finish one bounded work unit, run relevant checks and record evidence.
4. Never clone/recreate an existing checkout or upgrade librw merely because starting a new session. Never blindly repeat import, place, duplicate, removal or export actions.
5. For live tests, inspect session/document first. Use an isolated test document, capture a snapshot and roll back. Read tokens only into process environment, never output or commit them. Verify PID and executable path before stopping a test process.
6. Save/export only to explicit test/output destinations. Check source/world files remain unchanged. Do not overwrite the user's original Ariane executable.
7. At each batch boundary, update checkboxes, evidence, known issues and exact next step together. Commit coherent tested work when appropriate; record the commit. If checks fail, leave the batch incomplete and record the failure.
8. End every development session with a handoff that includes branch/HEAD, modified files, test results, running processes, local-only dependencies, pending decisions and the next command/action. A transcript is not required to resume.

## Definition of done

All required behavior above is implemented and validated; every batch acceptance gate is satisfied or an explicit user-approved scope change is recorded. Fork branch and Windows package are published. Usage guidance and links are current. Validation distinguishes actual client comparison from editor-only screenshots. No original scripts, world files, game assets or secrets are modified/published unintentionally.
