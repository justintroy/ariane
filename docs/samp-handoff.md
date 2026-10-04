# SA-MP implementation handoff

Updated: 2026-10-04. Read [AGENTS.md](../AGENTS.md) and [SPEC-samp-support.md](../SPEC-samp-support.md) first. This handoff is the resumption record, not a declaration of completion.

## Source checkpoint — 2026-10-04

The user requested committing and pushing the available development work to their fork. The checkpoint includes the SA-MP document/editor integration, CLI/session support, tests, samples, build/package helpers and development documentation. Generated builds, game assets, credentials and local evidence are excluded; `local-build/` is now ignored by the repository itself. This is a development branch checkpoint, not a release or completion of B3–B8. Use Git history for the resulting checkpoint commit; earlier no-commit/no-push statements below describe their historical stopping points. Remaining acceptance gates are unchanged.

## Luna-orchestrated continuation — 2026-10-04

Fork follow-up: the user created `git@github.com:justintroy/ariane.git`. SSH `git ls-remote` succeeded (remote HEAD `2fd965f18fd76800d81c0b677ebb5bce30adba1c`). Added remote `fork`; preserved `origin` at Dryxio. Set `remote.pushDefault` and `branch.codex/samp-support.pushRemote` to `fork`. B0 remote setup is complete. No commit or push occurred; release verification gates remain open.

The user selected the Ariane queue and requested multiple Luna agents. Work was split between the missing-model warning defect, package tooling audit, and validation triage. Existing uncommitted changes were preserved; branch remains `codex/samp-support`, HEAD `1a99d14b24b24b7cac238f2afbea3571b92518fc`.

`tools/euryopa/samp_editor.cpp` now derives missing-model warnings from current document records when drawing the window. Runtime synchronization no longer stores those warnings in the unrelated operation-status message. This preserves other status/errors and clears the warning when Undo, deletion, replacement or rollback removes the missing model.

Verified current Windows x64 D3D9 build with `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build/build_windows.ps1 -SkipLibrw`; log: `local-build/october4-build.log`. Binary SHA-256: `35301F0824E3A0AE42E60E4F9E8EE8073CC5CD9DDD3C79E07A73B782D159159B`. Agent suite via `cmd /c local-build/agent-tests.cmd`: 97 tests, one Unix-only transport skip; all five SA-MP parity tests pass. A Pillow deprecation warning remains.

Focused live regression used only the dedicated `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, PID 20180. Initial session creation correctly rejected because no scratch scene was configured; the retry explicitly configured `ariane\\october4_warning.ipl` backed by ignored `local-build/october4-tests/scratch.ipl`. Model 29999 produced both the UI banner and structured missing-model diagnostic. Undo cleared both, Redo restored the diagnostic, and rollback restored the complete initial snapshot. Captures `missing.png`, `undo.png` and `rollback.png` were visually inspected; banner is present only in the missing-model capture. Evidence: `local-build/october4-warning.py` and `local-build/october4-tests/warning-results.json`. All 217 baseline world-file hashes match. No project/Pawn export or world save occurred. The scratch session ended inactive and empty; the dedicated process was stopped after rollback. `runtime.pid` is a stale record, not a running-process assertion.

Native UI interaction remains unavailable. These captures verify presentation after IPC operations; they do not verify button clicks, stale bulk-delete confirmation, native dialogs, UI/session concurrency or vanilla gizmo/clipboard interactions. Clumps/device reset, non-Windows compilation and target-client render comparison remain open. Do not advance B3–B8 acceptance solely from this fix. The October 3 core/rotation/Pawn and broader live results below remain historical evidence for unchanged components.

Package audit fixed destructive reruns in `tools/release/package_samp_windows.py`: existing staging directories and ZIPs are refused, destinations are resolved before containment checks, and ZIP creation is exclusive. It also rejects symlink source files and sensitive path components, rejects an explicitly missing wheel, normalizes the engine destination to `ariane.exe`, and generates installation/scope guidance. `tests/release/test_package_samp_windows.py` adds seven passing checks. Python byte-compilation and strict dry-run pass with the pinned dependency.

Internal artifact: `local-build/package-audit-20261004/ariane-samp-audit-20261004.zip`, SHA-256 `6c15340674624947caf087b1aee35f28357767f92fea72a6c68d1bae7a2efd2c`. ZIP integrity passed, 19 entries, 18 listed-file checksums verified, no forbidden game asset extensions. A repeated package invocation exited 2 and preserved both staged executable and ZIP hashes. This candidate uses the October 4 binary but its install note predates the final preserve/rename-existing-executable and font-backup wording; retain it as audit evidence. The source now generates that guidance, and a future candidate must use a fresh output name. The archive is editor-only, has no Python CLI wheel, and has not passed clean-install validation. Source usage/build docs and specification were updated; bundled docs are a prior snapshot.

Next actions: native UI checks in an enabled environment, renderer/client comparisons and clean-install validation using a newly named candidate with current documentation. All Ariane processes were absent after the test process stopped. No commit, push or publication occurred.

## Testing resumed — 2026-10-03

The user lifted the testing deferral. The current Windows x64 D3D9 source builds with MSVC 14.50.35717/v145 and pinned librw `15ffa585216a9a7573ecc597b19ce2fde9b935f2`. Binary SHA-256: `6DC73C79432EF0CB91F1DA2DEF76182225B32ADBA67362C762172FC7D53F208D`.

Concrete failures fixed during execution: vcvars command quoting and unquoted semicolon-separated MSBuild targets in `tools/build/build_windows.ps1`; vcvars quoting and missing librw include/library dependencies in `run_samp_core_test.ps1`; missing braces around the generic paste clipboard loop in `objectinst.cpp`; an embedded NUL character literal and missing texture-cache model cursor update in `samp_editor.cpp`; `.samp.json` sample allowlisting and staging-path containment in `package_samp_windows.py`; missing Pawn entrypoints in both multi-group samples. Rotation uses the pinned librw headers/library; it is not dependency-free.

Verified: 105 standalone document checks, 10 rotation orientation checks, 97 Python agent tests (one Windows transport skip), current Release editor build, two generated Pawn roundtrip fixtures and all four sample Pawn files. Compiler warnings remain: legacy `a_samp` wrapper in Pawn and two bool-tag warnings in native sample; existing C++ conversion warnings. `git diff --check` passes with line-ending notices.

Dedicated live executable: `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, PID **16728** at stopping point (verify current path/PID before subsequent action). Original `ariane.exe` was not replaced. The current live B6 suite passes session guards, complete rollback/checkpoints, corrupt-checkpoint preflight, failed patch/stale revision atomicity, retry recovery, commit/no implicit write and generic ownership/scene-switch guards. Renderer evidence passes 300 atomic recreation cycles plus 150 model replacement/delete/Undo cycles, with independent materials, released deletion resources, stable instance identity and balanced resource counters. Clumps/device reset remain unverified.

Additional live evidence: window query/toggle leaves document unchanged; preview mutation requires a session and Undo restores filters; sample project opens with three objects; unavailable model 29999 remains in document across idle frames, reports structured `model_missing`, and Undo restores prior data. Texture indexing completes across 40,000 model slots and 2,704 TXDs; numeric 19379 search returns 34 texture results. Live CLI inspect/window pass. GUI-inclusive capture followed by default capture both succeed; screenshots are `local-build/october-tests/editor-ui.png` and `editor-world.png`. Screenshot inspection confirms current Files & Objects UI renders; this is not interaction evidence. A transient “Missing model: 29999” message persists after Undo/rollback although structured diagnostics clear; Known UI defect: the stale banner can imply a current missing asset after restoration; structured diagnostics are the current-state evidence. Fix and interaction verification remain open.

All four Pawn sample import previews run. They retain explicit unsupported-helper diagnostics for `Remove*Buildings` function definitions/calls; previews do not execute helpers. Review diagnostics and deliberately accept supported records. Evidence: `local-build/october-tests/live-results.json`, `local-build/b6-session-results.json`, `local-build/b3-renderer-results.json`. Final document objects/removals/undo/redo/history are empty; SA-MP inactive; session inactive; zero generic instances; revision 629. All 217 baseline world-file hashes remain unchanged.

Packaging strict dry-run and internal archive creation pass. Archive checksums verify for 17 entries. This candidate is under ignored `local-build/october-package/ariane-samp-internal-test.zip`, not published and not a validated clean installation. Its bundled docs precede this evidence update. No commit/push/publication occurred.

Remaining practical boundary: native computer APIs are disabled in this environment. Direct UI session concurrency, stale bulk-delete confirmation, file-dialog interaction, new texture UI interactions and remaining vanilla gizmo/clipboard checks are unverified. Only Docker Desktop WSL is installed; no normal Linux compiler environment or target SA-MP/open.mp client parity harness is available. Do not advance B3–B8 acceptance from IPC/build evidence alone. Next action: run those UI checks in an enabled native computer-use environment, then clump/device reset/client rendering and a clean installation/package audit.

## Historical stopping points

**Final stopping point (2026-10-03): implementation paused at the user-requested testing boundary.** Gemini finished the bounded integration audit (existing selection/gizmo/clipboard/shortcut hooks found; no new integration edits), CLI/service parity for `preview` and `window`, SA-MP MCP removal, standalone rotation-runner correction, and final editor source review. Final review fixed texture cache invalidation and filter termination, added `OFN_NOCHANGEDIR` to native dialogs, and made group export failures report partial progress. No builds, tests, editor launches, package execution, commits or pushes were performed in this continuation. Process inspection found no Ariane editor running; no runtime/session state was changed.

Changed areas: `samp_editor.cpp/.h`, `samp_document.cpp`, `agentbridge.cpp`, `arianectl.py`, `test_samp_parity.py`, draft build/release tooling, samples, notices, specification and usage docs. `ariane_mcp.py` now has no SA-MP delta against HEAD; unrelated MCP remains. Local reports: `gemini-integration-report.md`, `gemini-parity-report.md`, `gemini-runner-report.md`, `gemini-cli-only-report.md`, `gemini-final-source-report.md`, plus the prior delivery reports. Reports are source-review records, not test evidence.

**Next action, only when testing resumes:** compile current source and run core/rotation/CLI-service suites; exercise new texture/file-dialog/group-export/window/filter behavior, then remaining B3/B6 lifecycle and session UI checks. Validate draft build/package scripts and samples before relying on them. Preserve September evidence without treating it as verification of October edits. Non-Windows/client comparison, clean-install packaging and publication remain open. All acceptance gates previously incomplete remain incomplete.

**Scope change (2026-10-03):** User no longer requires SA-MP MCP support. Keep CLI and the shared service. Remove the SA-MP MCP tool and its dedicated parity checks/documentation; preserve unrelated existing MCP features. Historical MCP evidence below is retained as history, not a current requirement.

**Implementation-only continuation (2026-10-03):** The user requested Codex orchestration of Gemini 3.8 Flash agents through the installed Antigravity CLI, stopping before testing. All builds, automated/manual tests, editor launches, packaging execution and publication remain deferred. Previous verification below does not cover the new source. No batch acceptance is advanced by these edits.

On 2026-10-02, Gemini prepared editor texture indexing/search changes, a validated `preview` document operation, window visibility controls and export/file-dialog UX in `samp_editor.cpp/.h` and `samp_document.cpp`. It also created draft `tools/build/` scripts, `tools/release/package_samp_windows.*`, `docs/samp-build.md`, `docs/NOTICES-samp.md` and `samples/samp/`. Delivery review corrections include strict dependency-pin checks, toolset discovery, sample filtering and corrected librw MIT attribution. Reports are local-only under `local-build/gemini-*-report.md`; their claims require source review and later execution verification.

The first integration agent timed out after 20 minutes and returned an empty partial response; its `SUCCESS` envelope is not completion evidence. On resumption no `agy` process remained. Bounded follow-up agents are auditing integration, completing new operation CLI/MCP parity and removing unnecessary librw linkage from the standalone rotation runner. Preserve all existing edits. Do not repeat the eight previously passed manual checks unless changed behavior warrants it. Branch remains `codex/samp-support`, HEAD `1a99d14b24b24b7cac238f2afbea3571b92518fc`; nothing committed or published.

**Latest B6 live validation (2026-09-28):** `local-build/b6-session-validation.py` passed every requested API/CLI session-safety case against only `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, SHA-256 `4EED594A81B1B62A12DD5AD3AB1BACFD88A07C921CA5307CCA0FDFD3DE3AC47E`. Evidence is `local-build/b6-session-results.json`: mutations require a session; reads are non-mutating; commit writes no project/Pawn file; save/export reject during a session; rollback restores objects, texture/text materials, removals, generated code, undo/redo/history and active state; checkpoints restore the same complete SA-MP snapshot; corrupt checkpoint data fails preflight without mutation; stale revisions and failed multi-operation patches leave state unchanged; uncertain responses use inspect/compare before retry; and active scene switching rejects without losing the session.

The first live run found two ownership defects. `scene` could clear an active session without SA-MP rollback, and generic `place` while SA-MP editing was active entered the editor hook and added a document object without `expected_revision`. `agentbridge.cpp` now rejects active-session scene switching and rejects generic place/batch/transform/delete/clear/suppression/terrain-fit mutations while SA-MP editing owns the scene. Checkpoint persistence now carries full document undo/redo/history plus active state; restore preflights the snapshot. A checkpoint containing generic scene objects now rejects while SA-MP editing is active instead of crossing ownership domains. Empty-generic-scene SA-MP checkpoints restore normally.

Final live state: PID 6360, exact executable path `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`; scratch scene `ariane\\b6_session_a.ipl`; document objects/removals/history empty; SA-MP active false; agent session inactive; zero live generic instances; document revision 16. Inactive scene switching also preserved the complete SA-MP snapshot. All 217 recorded world-file hashes match. No save/export, commit to Git or push occurred. Core suite: 105 checks passed. Agent suite: 97 tests passed with one Windows skip. Windows x64 D3D9 build passed. Both generated Pawn fixtures compiled with only the existing legacy `a_samp` warning. `git diff --check` passed with line-ending notices only.

**Next action:** keep B6 partial until computer-use is re-enabled for the remaining direct UI mutation-during-session check. Do not repeat the eight passed manual checks. Then return to B3 extended ownership/resource lifecycle validation. Preserve current process unless replacement is needed; verify PID and executable path first.

**Latest B6 work (2026-09-28):** added `tools/agent/tests/test_samp_parity.py`. Five tests cover all 19 SA-MP operations through CLI, `ArianeService.samp`/`dispatch`, and MCP forwarding; file payload precedence; and rejection of non-object CLI parameters before transport. Focused tests passed. Full agent suite now ran 97 tests, OK with one Windows skip. Live engine structured errors, session rules, rollback/checkpoint restoration and retry recovery remain open. No editor launch, import, deployment, session mutation, commit, or push occurred.

**Latest bounded implementation (2026-09-28):** `samp inspect` now adds structured `asset_diagnostics` without changing document state. It reports missing models, TXDs, textures, unavailable fonts, Windows font substitution, and unsupported non-Windows text preview. Missing model records remain preserved. `cmd /c local-build/test.cmd` passed 103 checks and `cmd /c local-build/build.cmd` passed. Live agent response verification remains pending because the editor stays closed per current testing schedule. No import, session mutation, deployment, commit, or push occurred.

**Manual results (2026-09-28):** user reported “All pass” for all eight supplied checks on build SHA-256 `1C158C35DDBF9A7A1E6C65CEBE9E2F8B89B0A32BA9A5D47FA388B57A174DC521`: 1,200-object clipboard paste/import, preview invalidation after editing, row selection versus checkbox state, Cancel, delete-checked and one-action Undo, safe Enter behavior in delete-all confirmation, delete-all and one-action Undo, list scrolling, and clipboard replacement reporting 65,999 bytes. This is user-reported manual evidence, not automated observation. Stale-document rejection was not included and remains pending.

The scratch session was `manual-ui-validation`, started from an empty document. After receiving the results, IPC inspection returned connection refused and process enumeration found no Ariane process. The editor exited before rollback could be requested; **rollback was not verified**. `runtime.pid` records stale PID 32380. All 217 world-file hashes still match the baseline. No restart, import, save, export, commit or push was performed in this results-recording turn. Inspect fresh state before future tests. Keep computer-use deferred; the user is performing manual UI tests.

**Current scheduling:** the user stopped computer-use and deferred it to the testing phase. Do not resume UI automation during implementation. Scratch session `b3-ui-validation` was rolled back successfully (zero live scratch instances). Process 32580 subsequently exited; no stop command was issued. `runtime.pid` still records that stale PID. The later IPC inspection returned connection refused. Keep the editor closed until live testing resumes.

**Latest source changes:** `ObjectInst::DestroyRwObject` in `objectinst.cpp`/`euryopa.h` releases animation state and atomic/clump resources. `samp_editor.cpp` retires render resources on deletion/missing models, reuses the same instance on model replacement, recreates resources on Undo, and skips viewport polling for missing models so they remain in the document. Viewport changes now synchronize after their document patch. Runtime object shells remain retained for stable editor pointers; this does not claim all instance memory is reclaimed.

`tests/samp/renderer_validation.inl` now uses the real destruction path and adds 50 model-replacement/delete/Undo cycles with resource and stable-instance checks. **This extended probe is built but has not run.** Earlier 300-cycle results apply only to the earlier binary/probe. Clump recreation and device reset remain unverified; the pinned librw `Frame::destroyHierarchy` also lacks the allocation-counter decrement present in `Frame::destroy`, so clump frame counts require careful interpretation. Do not modify the dependency pin or claim a clump leak solely from that counter.

Latest implementation verification: Windows x64 D3D9 build passed; 103 core checks and 10 rotation checks passed; agent suite ran 92 tests, OK with one skip; both Pawn fixtures compiled (one legacy `a_samp` warning each); all 217 world-file hashes matched the prior baseline. No commits or pushes. Manual-test deployment and session state below supersede earlier runtime notes.

Next implementation work: finish ownership and structured asset diagnostics, then B4/B5 UX and B6 parity. At the testing phase, use only `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, inspect session/document state, run the extended probe in scratch state, verify missing-model persistence across idle frames and Undo, and coordinate stale-document confirmation rejection with the user. The eight manual checks above need not be repeated unless relevant behavior changes. Roll back scratch state before closing. B3 acceptance remains open.

### Earlier evidence (superseded runtime and UI notes are historical)

Continuation in progress (2026-09-27): direct Windows UI automation now works through the computer-use skill. `main.cpp` supplies aggregate modifier events omitted by the pinned RW backend and queues mouse transitions so short clicks are not lost between frames. Before the fixes, Ctrl+V produced no text and repeated checkbox clicks failed. After fixes, a 65,999-byte input copied back exactly, imported 1,200 objects, and checkbox/select-all/delete-checked/delete-all/Undo/Undo produced 1200/1199/0/1199/1200 objects. The scratch session was rolled back. A vanilla LOD building (model 6153, runtime instance 8863) remained selected and unchanged after Delete. Hashes of 217 data `.ipl`/`.ide`/`.dat` files remained unchanged. This is not full vanilla gizmo/shortcut acceptance.

`tests/samp/renderer_validation.inl` provides an environment-gated live probe (`ARIANE_SAMP_VALIDATION=1`, existing session, internal `__validate_renderer` request). `local-build/renderer-check.py` passed 300 atomic recreation cycles across two text objects, tint replacement and Undo; material pointers were independent, source signatures unchanged, overrides restored and RW allocation counts balanced. Evidence: `local-build/b3-renderer-results.json`. Clumps, device reset, deleted-instance retention and repeated model-change lifecycle remain unverified. The test session rolled back completely.

Latest edits in `samp_editor.cpp`: revision-indexed row lookup; viewport polling avoids copying unchanged documents; clipped object list; checked-vs-viewport explanation; revision-bound bulk-delete confirmation with Cancel default; input edits invalidate import preview; preview filters use document replace/history; material text refreshes after document revisions. Core suite: 103 passed. These newest UI changes still need a rebuilt-binary interaction pass. Preserve all earlier edits. No publication or batch completion claimed.

B1 and B2 passed their defined gates. The B3 implementation pass is finished; its acceptance remains partial. The live smoke showed distinct RED/BLUE text materials on two identical models, then duplicate, model replacement, material clear/undo and full session rollback. Rotation equivalence passed 10 mixed-axis/singularity checks. Cut/copy/paste now route through document operations; copy snapshots rows so rebuilt source pointers are not used for paste. Transform and delete code rejects vanilla instances while SA-MP editing is active. A loaded LA world IPL hash stayed unchanged. Changes remain uncommitted.

Next B3 work: perform actual UI cut/copy/paste and vanilla-object interaction checks, test slot replacement after renderer recreation, and account for renderer resources across repeated recreate/material cycles. Do not mark B3 acceptance complete until those pass. B4 texture indexing was modified and builds, but its live behavior is unverified. Do not reimport live test data blindly.

Ad hoc UX audit: Windows clipboard callbacks and a growing Files & Objects Pawn input are implemented. The field has **Replace input from clipboard** and **Clear input** buttons. Per-object checkboxes, **Select all objects**, **Deselect all**, **Delete checked**, and **Delete all objects** exist. Bulk delete uses one undoable document `replace` action and retains removals/project settings. Checked rows are separate from viewport selection; UI labels now say **checked**, and bulk delete reports the removed count. Windows x64 D3D9 build and 103 core checks passed. Ctrl+V/button behavior, large pasted maps, checkbox interactions and UI undo still need direct tests. **Delete all objects** currently acts immediately without confirmation; assess that UX while testing. The original `ariane.exe` was preserved. The two older SA-MP test builds were deleted. Current and only SA-MP test build: `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, SHA-256 `E15F7BABE8C4D8C906A86B431D0AA9D1BE4D1D5279C6D9DD30FA953DF3A424F1`. No SA-MP test editor process was running at the last check.

User priority for continuation: improve SA-MP UX first. Full integration with vanilla editor selection/gizmos/shortcuts/view workflow is mandatory before release, but lower priority than current SA-MP UX gates. Finish B3 validation, then B4/B5 UX, then B6 service parity, B7 vanilla integration/regression, and B8 delivery. Preserve separate document/world ownership. Do not claim interaction tests passed from build success.

## Checkout state

- Repository: `C:/Users/PC/Documents/ariane`.
- Branch: `codex/samp-support`.
- HEAD: `1a99d14b24b24b7cac238f2afbea3571b92518fc` (Agent Alpha base).
- Implementation and these documents are uncommitted. Do not reset, clean or overwrite them.
- `origin` currently points to `https://github.com/Dryxio/ariane.git`.
- User will create `justintroy/ariane` themselves. Fork existence/push authentication is not verified. This session's unauthenticated `git ls-remote` requested credentials and the public GitHub URL returned 404; a private fork remains possible. Verify authenticated access before configuring/pushing; never push upstream.
- Pinned librw sibling: `C:/Users/PC/Documents/ariane-librw`, SHA `15ffa585216a9a7573ecc597b19ce2fde9b935f2`.
- No release package, implementation commit or publication exists yet.
- Test editor: `D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`, current build; no running SA-MP test process at last check. Confirm PID and executable path before stopping or replacing any process. `local-build/runtime-env.json` contains the private agent token; do not print it.

## Implementation map

### New C++ files — implemented, incomplete validation

- `tools/euryopa/samp_document.h/.cpp`: standalone C++14 JSON document; Pawn tokenizer/static expression parser; defaults and native/dynamic signatures; document validation; stable IDs; import preview/apply; normalized export; versioned project IO; history/undo/redo; snapshots; atomic staged patches.
- `tools/euryopa/samp_editor.h/.cpp`: five-tab SA-MP window; document-to-instance synchronization; quaternion conversion; per-instance geometry/materials; incremental texture indexing; GDI text rasterizer; removal visibility; editor operations and agent snapshots.
- `tools/euryopa/samp_rotation.h/.cpp`: standalone Euler/quaternion and render-matrix conversion used by the editor; `tests/samp/rotation_test.cpp` checks orientation equivalence.
- `tools/euryopa/vendor/json.hpp`: nlohmann JSON v3.11.3 single header with embedded MIT notice. Review dependency attribution for packaging.
- `tests/samp/core_test.cpp`: standalone semantic/parser/transaction/round-trip tests, also generates Pawn compilation fixtures.

### Existing C++ integration files — modified

- `gui.cpp`: window/tick calls and guard against world save while SA-MP editing.
- `objectinst.cpp`: dedicated SA-MP runtime instances; selection/placement/paste/undo hooks.
- `mapdocument.cpp`: SA-MP document ownership checks.
- `renderer.cpp`: removal filtering; override application; guards preventing visibility paths from replacing cloned geometry with shared model geometry.
- `fileloader.cpp`: SA-MP/world save separation guards.
- `agentbridge.cpp/.h`: `samp` dispatch, sessions/rollback snapshot, capabilities, session-state getter and optional GUI capture.
- `euryopa.cpp`: invokes GUI-inclusive capture after drawing ImGui.

### Python files — modified

- `tools/agent/arianectl.py`: `samp` operations with JSON parameters/file and `capture --include-ui`.
- `tools/agent/ariane_ipc.py`: optional GUI-inclusive capture argument.
- `tools/agent/ariane_mcp.py`: `samp_document` tool.
- `tools/agent/service.py`: shared dispatch, checkpoint SA-MP data and Windows directory-fsync handling.
- `tools/agent/simulation_harness.py`: authenticated loopback TCP test fallback when Unix sockets are unavailable.
- `tools/agent/tests/test_ipc_transport.py`: standalone C++ extraction excludes the added SA-MP headers.

### Documentation added at this stopping point

- `SPEC-samp-support.md`: authoritative scope and B0–B8 acceptance gates.
- `AGENTS.md`: development invariants, usage links and handoff rules.
- `docs/samp-usage.md`: provisional current usage and final documentation requirements.
- This file: resumption state and evidence.
- Navigation links added to Roleplay root `AGENTS.md` and `tools/gta3dai/AGENTS.md`. Those changes belong to the separate Roleplay repository; do not accidentally stage unrelated Roleplay changes.

## Current service contract

`Document::request` accepts an `op` and optional `expected_revision`. Operations are `inspect`, `code`, `preview_import`, `import`, `save`, `export`, `open`, `undo`, `redo`, `replace`, `clear`, `update`, `delete`, `material`, `duplicate`, `removal`, `place`, `patch`. Editor-level `textures` is implemented outside the pure document core.

Schema version 1 fields: `version`, `next_id`, `objects`, `removals`, `groups`, `asset_paths`, `preview` (`world`, `interior`). Object fields include `id`, `group`, `model`, `position`, `rotation`, `world`, `interior`, `player`, `stream`, `draw`, `area`, `priority`, `materials`. Materials use slots `0`–`15` as JSON string keys. IDs are shared across object/removal records. See the usage guide and source for payload examples.

Imports allocate IDs and append records. They are not safe to repeat blindly. Preview is read-only. Imports with diagnostics require explicit `accept_diagnostics`. Atomic patches stage supported operations and commit once; replay receipt handling is still outstanding. Inspect omits history unless requested. Save requires `.samp.json`; export requires `.pwn`.

Read operations do not require agent mutation sessions. Agent changes do; save/export is denied during an active session. UI/project persistence must remain explicit. Rollback restores SA-MP snapshot data, undo/redo and history. Existing generic scene/session behavior needs the audit listed below.

## Verified evidence and limits

These are results observed during implementation before the documentation handoff. Do not reinterpret them as covering newer changes or untested combinations.

| Check | Observed result | Limit |
| --- | --- | --- |
| Windows x64 D3D9 build | Passed after B3 clipboard snapshot fix on 2026-09-27 | Current binary includes B4 indexing edits that are not live verified |
| Standalone SA-MP core | 103 checks passed on 2026-09-27 | Covers B1 and B2 contracts; renderer ownership remains B3 |
| Rotation equivalence | 10 mixed-axis and singularity checks passed on 2026-09-27 | Quaternion/matrix equivalence only; direct viewport interaction remains open |
| Generated Pawn fixture | Compiled successfully with local Pawn compiler/streamer includes | One upstream warning recommending open.mp instead of legacy a_samp |
| Existing agent suite | Ran 92; OK, skipped 1 (91 passed) on 2026-09-27 after B2 changes | Unix-specific transport skipped on Windows; SA-MP live parity suite remains B6 |
| Two-group Pawn fixture | Compiled on 2026-09-27 | One warning: legacy `a_samp` wrapper |
| Live agent import/rollback | Passed; imported object then restored empty document | Narrow smoke test, not full checkpoint/CLI/MCP parity |
| Live material text render | Visible on model 19379 after geometry-reset fix | Editor screenshot only; repeated text follows object UVs |
| B3 ownership smoke | Live latest build: RED/BLUE on identical 19379 objects; duplicate, model replacement, material clear/undo and rollback passed | `local-build/b3-ownership.png`; direct cut/paste UI and resource accounting unverified |
| World-file hash | `LAe2.ipl` SHA-256 unchanged at `0E0B143E3FD217B41DC79CB390627EBA2E8D508054D10BC14D38B499A1DDD818` before/after B3 smoke | One representative loaded IPL, not whole game tree |
| SA-MP/open.mp client comparison | Not performed | Cannot claim exact font/material/removal parity |
| Non-Windows build | Not performed | Cross-platform regression gate remains open |
| Direct UI button testing | Blocked by native UI helper initialization failure | Screenshot capture can inspect layout but does not prove interactions |

Useful screenshot: `local-build/first-preview.png` shows working material text. Earlier `material-live.png` and `text-preview-v2.png` show stale pre-fix wood and must not be presented as current results.

## Build and test environment

All paths below are local observations, not portable defaults. Revalidate if moved to a new machine.

- VS Build Tools: `C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat`, toolset v145.
- Premake beta8: `local-build/premake/premake5.exe`. Use its full path; librw has an older executable that cannot generate vs2022 correctly.
- Python with agent dependencies: `C:/Users/PC/Documents/Roleplay-Project-v2/tools/ariane/.venv-agent/Scripts/python.exe`.
- Default Python 3.14 lacks MCP dependencies; do not interpret its import errors as product failures.
- Pawn compiler: `C:/Users/PC/Documents/Roleplay-Project-v2/qawno/pawncc.exe`.
- Pawn includes: `C:/Users/PC/Documents/Roleplay-Project-v2/qawno/include`.
- Current local game root: `D:/Grand Theft Auto San Andreas`, discovered through gta3dai's source report. Do not assume this path for users.
- Build output: `bin/win-amd64-d3d9/Release/ariane.exe`.

Run from the Ariane repository root:

```powershell
cmd /c local-build\test.cmd
cmd /c local-build\agent-tests.cmd
cmd /c local-build\build.cmd
& 'C:/Users/PC/Documents/Roleplay-Project-v2/qawno/pawncc.exe' local-build/roundtrip-compile.pwn '-iC:/Users/PC/Documents/Roleplay-Project-v2/qawno/include' '-olocal-build/roundtrip.amx'
git diff --check
```

The local wrappers are ignored and must not be the sole final build documentation. Their essential commands are:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /EHsc /std:c++14 /Itools\euryopa tests\samp\core_test.cpp tools\euryopa\samp_document.cpp /Fe:local-build\samp-core-test.exe /Fo:local-build\
local-build\samp-core-test.exe local-build\roundtrip
```

For full build, set `LIBRW` to the pinned checkout, run the full premake beta8 path with `vs2022` in librw, build `build/librw.sln` target `librw`, then run premake `vs2022 --channel=master` in Ariane and build `build/librwgta.sln` targets `librwgta;euryopa`. Both use Release, platform `win-amd64-d3d9`, toolset v145. Track a portable equivalent before release.

## Runtime state and safe restart

The current test binary is **`D:/Grand Theft Auto San Andreas/ariane-samp-objects-dev.exe`**. The original `ariane.exe` was not replaced. Windows requires the executable in the game root in this setup.

`local-build/runtime.pid` records the last test process. The process may still be running. Read the current PID and verify its executable path is the exact test binary before stopping it; do not use a stale PID from chat. Wait for process exit before copying a rebuilt binary.

`local-build/runtime-env.json` contains the test port and random authentication token. Never print, commit or include it in a package. Load it only into environment variables for the test client. Port was 48739; read current metadata instead of assuming it is unchanged.

`local-build/launch.py` discovers the game path, copies the test binary with bounded retries, launches it, writes runtime metadata and waits for IPC. It does not itself establish that another running process is safe to replace. Inspect process/session state first. `local-build/smoke.py` configures a scratch agent scene, starts a session, imports a text object, captures it and rolls back. It assumes no conflicting active session; inspect before running. Its output must not expose the token.

No world-save operation was intentionally performed. Core test outputs and scratch IPL paths are under `local-build`. Verify unchanged world files in the final validation batch.

On 2026-09-27, PID 14664 from `runtime.pid` was no longer running. No live editor test or game file write occurred in this session. Current Windows binary was rebuilt in the repository output path only; it was not copied into the game root.

## Latest session evidence

- Modified this session: `tools/euryopa/samp_document.cpp`, `tests/samp/core_test.cpp`, `SPEC-samp-support.md`, `docs/samp-handoff.md`, and `docs/samp-usage.md`. Earlier uncommitted files remain intact.
- `cmd /c local-build/test.cmd`: 80 SA-MP core checks passed after B1.
- After B2, `cmd /c local-build/test.cmd`: 103 checks passed; `cmd /c local-build/build.cmd`: passed; `cmd /c local-build/agent-tests.cmd`: 92 run, 1 skipped, OK.
- B2 changed `samp_document.cpp`, `samp_editor.cpp`, `core_test.cpp`, the specification, this handoff and `docs/samp-usage.md`. History entries now include affected record snapshots and remain bounded through undo/redo. Invalid snapshots and project schemas are rejected before state replacement.
- `cmd /c local-build/build.cmd`: Windows x64 D3D9 build passed after the C++ document changes.
- `roundtrip-groups-compile.pwn`: Pawn compiler exit 0 with one warning (legacy `a_samp` wrapper). Empty removal function now marks `playerid` unused.
- Final generated combined and two-group fixtures both compile with the same single legacy-wrapper warning.
- `cmd /c local-build/agent-tests.cmd`: 92 tests ran, 1 skipped, before the B1 C++ changes. Repeat after integration changes.
- `git diff --check`: no whitespace errors; Git reports an existing renderer.cpp line-ending notice.
- No new commit, package or push. Branch and pinned dependency SHAs remain as recorded above.
- Primary reference spot-check: [streamer.inc](https://github.com/samp-incognito/samp-streamer-plugin/blob/master/streamer.inc) confirms dynamic creation and material signatures/defaults; [open.mp SetObjectMaterialText](https://open.mp/docs/scripting/functions/SetObjectMaterialText) confirms native text order. Remaining signature and client behavior checks stay open.

## Known gaps and audit targets

1. Parser function/block scopes are simplistic. Audit aliases, repeated local names, control-flow skipping and cross-file separation. Numeric bitwise/cast overflow needs hardening.
2. Per-group exports now use `_G<project-group-index>` function suffixes. Combined two-group fixture compiled; more ordering and reload checks remain.
3. Texture search indexes one representative model per TXD; numeric queries for other models sharing that TXD may miss results.
4. Preview world/interior controls currently mutate data directly. Route changes through the shared validated service/history path.
5. Geometry/material cache uses instance/pointer-based state. Audit resource destruction/recreation, pointer reuse and clone failure caching. Latest clone code pushes the model TXD during stream cloning.
6. Standard paste duplication has a simple X offset. Cut/paste can lose its source document row; complete clipboard ownership behavior.
7. Runtime missing-asset/font messages are mostly UI messages. Expose meaningful structured diagnostics to agents as required.
8. History currently stores full-document code snapshots and has uneven undo/redo bounds. Confirm required affected-object presentation and bounded retention.
9. Checkpoint SA-MP restore follows existing scene restore; malformed checkpoint data can risk partial scene changes. Validate/stage complete restoration.
10. Generic agent place/transform/scene paths assume agent IPL ownership and may conflict with SA-MP mode. Scene switching can clear a session without restoring SA-MP snapshot. Audit and test.
11. Optional GUI capture flag is newly added. Check `capture_pose`/error paths cannot retain a stale include-UI flag and default capture remains unchanged.
12. Window close currently leaves a small reopen window. Verify intended one-window UX and clipboard export behavior.
13. Mixed-axis rotations, identical-model material isolation, tint/alpha, overlapping removals/LODs and client text parity need the full matrix.
14. Add SA-MP CLI/MCP parity, checkpoint, rollback and patch-retry tests. Existing suite alone is insufficient.
15. Portable build/CI, samples, final usage guide, dependency attribution, clean-install package and publication are unfinished.

Treat these as audit targets, not all as proven defects. Close each with code/test evidence or an explicit documented scope decision.

## Fresh-session prompt

> Continue the Ariane SA-MP editor implementation in `C:/Users/PC/Documents/ariane`. Read `AGENTS.md`, `SPEC-samp-support.md` and `docs/samp-handoff.md`, plus the linked Roleplay/gta3dai guidance. Preserve the uncommitted implementation on `codex/samp-support` and pinned librw. Follow batch acceptance gates; begin with the recorded next step. Do not reimport test data or recreate the checkout blindly. Update the specification and handoff after each completed batch. User is creating the GitHub fork; verify it before publishing. Finish remaining implementation, validation, usage documentation and Windows delivery without claiming unverified tests passed.
