# Ariane SA-MP editor fork

## Objective

Deliver a Windows x64 Ariane fork with a dedicated SA-MP editor window, live object/material/removal previews, static Pawn import/export, versioned project persistence, and equivalent CLI operations via the shared document service. Keep existing cross-platform functionality building. Use open.mp as a behavior reference; do not embed a server or streamer runtime.

This specification is self-contained and idempotent for development across fresh sessions. Read `AGENTS.md`, this file, and `docs/samp-handoff.md` before changing code. This file defines the target and batch acceptance criteria; the handoff records evidence and the exact stopping point. Implemented code is not automatically verified or release-ready.

## Status

- Updated 2026-10-04. B1 and B2 remain complete; B0 and B3–B8 remain incomplete. Current work is B3/B4 verification and B8 package preparation.
- Current Windows x64 D3D9 build passes. Binary SHA-256: `35301F0824E3A0AE42E60E4F9E8EE8073CC5CD9DDD3C79E07A73B782D159159B`.
- Fixed stale missing-model UI warnings: the banner derives from current document records instead of persisting in the operation-status message. Live captures verify model 29999 warns, Undo clears it, and rollback clears it. Structured diagnostics also pass Undo/Redo checks. Complete snapshot restored; all 217 baseline world-file hashes unchanged.
- Current Python agent suite: 97 tests, one Unix-only transport skip. The 21-operation CLI/shared-service forwarding coverage passes. SA-MP MCP was dropped by the user on 2026-10-03; unrelated MCP support remains.
- October 3 evidence remains applicable to unchanged core/rotation code: 105 document checks, 10 orientation checks, generated Pawn fixtures and four sample compilations. Live B6 session/checkpoint/ownership tests and 300 atomic recreation plus 150 model replacement/delete/Undo cycles passed on the recorded October 3 binary.
- Direct native UI interactions, clumps/device reset, target-client render parity, non-Windows compilation and clean-install verification remain open. Native computer APIs are unavailable here; screenshots and IPC checks do not close interaction gates.
- User-reported September 28 manual checks passed large clipboard/import, preview invalidation, row selection versus checks, Cancel/safe Enter, bulk deletion/Undo and scrolling. Do not repeat them without relevant changes. Stale confirmation and concurrent UI/session mutation remain open.
- Existing uncommitted work is preserved. Branch `codex/samp-support`, base HEAD `1a99d14b24b24b7cac238f2afbea3571b92518fc`. No release is published. User previously authorized publication to `justintroy/ariane`, but acceptance gates remain unmet; never push to Dryxio upstream.
- Exact evidence, runtime state, package results and next actions are recorded in the newest `docs/samp-handoff.md` entry. Older batch evidence below retains its original scope and date.

## Repository and references

- Working checkout: `C:/Users/PC/Documents/ariane`, beside `Roleplay-Project-v2`.
- Upstream: https://github.com/Dryxio/ariane
- Intended fork: https://github.com/justintroy/ariane
- Branch: `codex/samp-support`.
- Base tag: `v1.40.9-agent-alpha.1`.
- Current HEAD/base: `1a99d14b24b24b7cac238f2afbea3571b92518fc`.
- Pinned librw: `15ffa585216a9a7573ecc597b19ce2fde9b935f2`, local sibling `ariane-librw`.
- Behavior reference: https://github.com/openmultiplayer/open.mp ; verify native signatures and client behavior against primary references when implementing remaining details.
- Workspace guidance: `../Roleplay-Project-v2/AGENTS.md`, `../Roleplay-Project-v2/docs/spec-driven-development.md`, and `../Roleplay-Project-v2/tools/gta3dai/AGENTS.md`.
- Verified user fork access on 2026-10-04: `fork` points to `git@github.com:justintroy/ariane.git`. Existing `origin` remains Dryxio for upstream fetching; never push there. `remote.pushDefault` and `branch.codex/samp-support.pushRemote` both select `fork`. No push has been performed.

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

## Progress and batch acceptance

Each batch is independently resumable. Start by inspecting existing code and evidence; do not recreate working features. Mark a batch complete only after its acceptance checks pass. Keep failed/unavailable checks explicit.

### B0 — Baseline, fork and reproducible workspace (PARTIAL)

- [x] Separate checkout and requested branch/base.
- [x] Pinned librw checkout.
- [x] Local Windows toolchain builds the editor.
- [x] Verify fork exists; configure upstream/user remotes without losing existing settings. SSH access to `justintroy/ariane` verified; `fork` is the default push destination, with existing `origin` retained.
- [ ] Convert necessary local build knowledge into portable tracked instructions/scripts.
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
- [ ] Exercise cut/copy/paste and vanilla selection/transform/delete through the actual UI; check slot replacement after resource recreation.
- [ ] Run repeated recreate/material cycles with renderer resource accounting; no-leak acceptance remains unverified.
- Atomic-only recreation evidence: `tests/samp/renderer_validation.inl`, enabled by `ARIANE_SAMP_VALIDATION=1` and a scratch session, checks source material signatures, independent material pointers, restored overrides, and allocation counts. Live text, tint replacement and Undo passed 300 cycles. Remaining lifecycle and device-reset checks remain open.
- Acceptance: ownership isolation and restoration across undo/reload/recreation; no world file changes; no leaked or stale renderer resources.
- Handoff: screenshot/test evidence, ownership lifecycle and remaining renderer differences.
- Evidence: Windows x64 build; 103 core checks; 10 orientation checks; live RED/BLUE shared-model screenshot, duplicate/model-change/material-clear/undo/rollback smoke; `LAe2.ipl` SHA-256 unchanged. These checks do not establish full acceptance.
- Follow-up: Windows ImGui text boxes now use the Unicode system clipboard, including SA-MP Pawn and material-text inputs. Build passed; interactive copy/paste verification remains open.
- User follow-up: Files & Objects input now grows beyond 64 KiB and offers an explicit Paste clipboard button. Build passed; direct UI behavior remains unverified.

### B4 — Texture browser and material-text editor (PARTIAL)

- [x] Incremental texture indexing, thumbnail UI, tint/slot controls and GDI text rasterization exist.
- [x] Structured `samp inspect` asset diagnostics exist for missing models, TXDs, textures and font preview substitution/unavailability.
- [ ] Verify model searches across shared TXDs, indexing completion and missing-asset diagnostics through API as well as UI.
- [ ] Test ARGB/alpha, supported sizes, bold/fonts, multiline, inline colors and alignment.
- [ ] Compare representative text/texture renders with an actual SA-MP/open.mp client.
- Acceptance: controls change only the selected instance; parameters survive missing assets and reload; documented client comparison results.
- Handoff: reproducible visual fixtures and explicit differences/unavailable checks.

### B5 — Removals and complete window workflow (PARTIAL)

- [x] All five tabs, editable removals and spherical preview exist.
- [x] Files & Objects includes per-object checks, select/deselect all, delete checked and delete all; bulk deletes preserve non-object document state and use one undoable replace action. Windows build and 103 core checks passed; direct UI interaction remains unverified.
- [ ] Test boundary/vertical/wildcard/LOD/overlap cases and undo restoration.
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

### B7 — Regression, compatibility and client verification (TODO)

- [ ] Complete integration with existing vanilla editor selection, gizmos, shortcuts and shared viewport workflow while preserving separate SA-MP document ownership and world-save guards. This is required before release; schedule after SA-MP UX gates.
- [ ] Run complete parser/core suite and compile exported fixtures with Pawn and streamer includes.
- [ ] Run existing agent suite and new SA-MP integration suite after final changes.
- [ ] Build existing non-Windows targets; add/update CI without changing the pinned dependency.
- [ ] Run complete rendering/removal matrix and compare representative client renders.
- [ ] Check world-file hashes before/after tests, resource recreation and clean launch.
- Acceptance: evidence includes commands, revisions, toolchain, pass/fail/skip counts and screenshots; unavailable checks are release limitations, not passes.
- Handoff: verification report and release risks, with remaining failures assigned to a batch.

### B8 — Documentation, package and publication (TODO)

- [ ] Finalize root Ariane `AGENTS.md` and `docs/samp-usage.md` with tested setup/UI/CLI examples and supported Pawn subset.
- [ ] Keep navigation links in Roleplay root and gta3dai `AGENTS.md`; label experimental until release acceptance passes.
- [ ] Add small sample maps exercising all requested calls, with no game assets.
- [ ] Package Windows x64 editor, necessary fonts/runtime/tool scripts, dependency notices, setup guide and verification report.
- [ ] Verify package from a clean local installation and record SHA-256.
- [ ] Commit reviewed changes, push `codex/samp-support` to the user's fork and publish the Windows build.
- Acceptance: working download/package and branch URLs, complete usage instructions, no secrets/game assets, precise remaining visual limitations. Do not claim release completion if publication or required verification remains blocked.
- Handoff: published commit, artifact URL/hash and final known limitations.

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
