# SA-MP automated continuation verification — 2026-10-05

This report records the GPT-6 Luna orchestration requested by the user, with three agents running at Max reasoning. Computer use is excluded for this continuation. Automated builds, core tests, authenticated engine IPC and generated captures are separate from direct UI interaction and client verification.

## Source and environment

- Checkout: `codex/samp-support`, continuing development checkpoint `22dcc42f685566f8313c79299e84ba33ece6e81e` and preserving its existing uncommitted changes.
- RenderWare dependency remains pinned to `15ffa585216a9a7573ecc597b19ce2fde9b935f2`.
- Windows compiler: MSVC 14.50.35717, Visual Studio Build Tools 18.1.1, Windows x64 D3D9.
- Pawn compiler: 3.10.11, using the local qawno include tree with streamer declarations.
- Runtime validation uses a separately named development executable and scratch agent sessions. Authentication metadata and generated artifacts remain under ignored `local-build/`.

## Completed checks

| Check | Evidence | Result |
| --- | --- | --- |
| Document and symbolic patch core | `tools/build/run_samp_core_test.ps1 -OutDir local-build/20261005-core` | 132 checks pass |
| Rotation equivalence | Same tracked core runner | 10 orientation checks pass |
| Windows x64 D3D9 editor | `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build/build_windows.ps1 -SkipLibrw` | Build passes; `local-build/20261005-build-portable.log` |
| Package manifest and wheel validation | `python -m unittest discover -s tests/release -p 'test_*.py' -v` | 14 checks pass |
| Full Python agent suite | MSVC-initialized Python 3.13 environment, `python -m unittest discover -s tools/agent/tests -v` | 122 tests, one Unix-only transport skip; all others pass |
| Focused authoring and parity suite | `test_samp_authoring.py` and `test_samp_parity.py` | 30 tests pass |
| Existing Pawn samples | `native_map.pwn`, `streamer_map.pwn`, `multi_group/structures.pwn`, `multi_group/amenities.pwn` | All compile |
| Generated Pawn round trips | `roundtrip-compile.pwn`, `roundtrip-groups-compile.pwn` under the runner output directory | Both compile |

Symbolic patch checks cover new-object references, cloned material independence, nondefault world/interior retention, one-action Undo/Redo, stale retry rejection, invalid/duplicate/forward references, invalid material rejection, expansion limits and unchanged complete snapshots after failure. Resolution does not allocate IDs in the installed document. The outer patch commits once through the existing C++ document service.

The native sample now passes actual booleans for `SetObjectMaterialText` and no longer emits its two bool-tag warnings. All six Pawn fixtures still emit the local legacy `a_samp` include warning. This is a compiler warning, not a failure.

Final Windows binary SHA-256: `314D1CDE66BD1E685C05234D681729596A1B3AFFB9D4802F6EC7BEC11A1D5E77`. It includes the visibility fixes and the private helper rename needed to avoid POSIX `sync()` on Linux. The development runtime was stopped only after verifying its identity and inactive, empty document; the new binary was copied only to the separately named development test path. Final renderer and recipe runs reference this hash. Asset-preview matrix and tint pixel results below were established on the earlier `3C1A43FD6CBC3CE54BEA6C4F569D60808D8A8049524D6DD635CD5EBF342F197F` build; their inspection/PNG/material-comparison implementation is unchanged by the visibility correction and private helper rename.

The initial live asset matrix found a concrete material-preview failure: RenderWare's generic image reader appended another `.png` extension. Direct PNG readback replaces it in the final build. A subsequent pixel audit caught loss of the original texture during tint-only cloning; the preview now loads and selects the destination model's TXD before cloning. The neutral-white tint comparison matches the normal original-texture comparison exactly: 5,480 distinct RGBA values, zero changed pixels and zero mean difference. Initial failure evidence is retained separately; final evidence is `local-build/20261005-assets/asset-api-results.json` and `tint-pixel-audit.json`.

The asset matrix verifies model/slot/submesh provenance, separate visual/collision bounds, missing-model diagnostics, shared TXDs, numeric query limits, deterministic pagination, filter-bound stale cursors, singleton/checkerboard/candidate PNGs, material comparison and rejection before output creation for a malformed late candidate. Full document snapshot, history, revision, session and camera remain unchanged. Model 3120 is a real clump with one atomic submesh; a clump does not necessarily contain multiple submeshes.

The final renderer probe ran three checkpoints (import, tint and Undo): 300 atomic recreations, 150 clump clone/release cycles and 150 model-replacement/delete/Undo cycles. Each checkpoint passed 11 metadata/visibility checks, 12 removal checks, eight direct clipboard checks and all four caller-side failure hooks (`atomic_clone`, `atomic_frame`, `clump_clone`, `clump_root`). Metadata checks verify universal native preview area 13 while retaining document world/interior/streamer-area fields; per-instance draw distances 750/425, default and non-owned fallbacks; and temporary capture filters without document/revision changes. Removal probes cover inclusive sphere boundaries, vertical separation, exact/wildcard IDs, LOD handling, overlapping removals and Undo/Redo. Clipboard probes call the actual copy/cut/paste functions with mixed vanilla/document selections, cloned material data, and cut/paste stable-ID restoration. They do not prove keyboard shortcut delivery.

Rollback restored the complete document/history/Undo/Redo snapshot and active state. All 217 baseline world `.ipl`, `.ide` and `.dat` hashes match before and after. Latest evidence: `local-build/renderer-portable-results.json` and the reproducible ignored driver `local-build/20261005-renderer-validation.py`; earlier renderer evidence remains separately retained. Pinned librw's known frame-counter accounting defect remains excluded from balance claims; all compared non-frame counters balance.

Python authoring tests cover symbolic references and independent cloned materials, immutable base hashes for existing-record edits, SA-MP mixed-axis/gimbal rotation, full 3D perimeter placement and both wall axes, source group names with spaces/backslashes, wildcard/global preview filters, unavailable assets/slots/textures, strict numeric fields, expansion budgets, explicit unsupported geometry rejection, and bounded diagnostic/pair-scan coverage. Retry tests distinguish committed-but-lost responses, dropped precommit responses and unrelated concurrent edits; they verify recovered mappings, bounded retries and no unsafe replay. Valid object keys or palette names such as `mesh` and `support` are preserved; unsupported geometry is rejected as a record field rather than by banning arbitrary identifiers. Missing or unknown adapter arguments return structured errors at signature binding; exceptions originating inside a helper are not mislabeled as user input. Final suite log: `local-build/agent-tests-authoring-final2-20261005.log`.

Both sample plans have passed CLI resolution/application, one-action Undo/Redo, bounds/composition captures, a props-layout revision, an independent material revision, explicit commit/save/export, and complete semantic save/reopen comparison. Pawn compilation passes for the 40-object interior and the 24-object courtyard with one explicit stock-building removal. Each emits only the existing legacy `a_samp` include warning. Initial images exposed native area culling of nondefault-interior objects and stock-model draw-distance culling of valid props. The final build corrects both: overview/plan/doorway captures now show the rooms, furniture, planters and bollards. The doorway camera is explicitly aligned at `[0,-8,Z+1.6]` looking toward `[0,0,Z+1.7]`, FOV 70, rather than across an occluding perimeter wall. Recipe evidence and full CLI arguments/results are in `local-build/20261005-recipes-visible/recipe-results.json` and `cli-records.json`. Focused sign captures exposed a recipe error: the tint step selected the first model-19353 record, which was the sign. The corrected CLI and documented recipe select the Walls group, preserving both text materials. Reception uses font size 20 and sits 0.20 units inward from the north wall; service uses size 24. The labels now fit one line. The service panel intentionally fills the left south opening; the central 5-unit opening remains the entrance. Full-panel black backgrounds and text near the top remain visible in the editor; reverse-side views mirror the text. These are recorded editor results, not client parity or collision proof.

The final Python wheel passed safety validation and an isolated prefix installation: installed CLI help, imports of all seven authoring modules from the prefix, and the exact 13-operation authoring schema passed without engine calls. Wheel: `local-build/20261005-agent-wheel-final4/ariane_agent-0.1.0a1-py3-none-any.whl`, SHA-256 `D1492E32F8D36BD5B44247D6E20BB2687210847A2184D8C369EE6AA503A90D1E`.

## CI and recipe evidence

GitHub Actions run [37241887826](https://github.com/justintroy/ariane/actions/runs/37241887826) passed all four jobs at source commit `752e9a9a5f924c2591109b129d15a0610e47daf6`: Windows D3D9 build/core/rotation, Linux OpenGL build/core, Python agent/wheel and package safety tests. Linux explicitly reported 132 SA-MP core checks passed. Librw remained pinned. The initial Ubuntu 22.04 runner could not start pinned Premake because its binary requires `GLIBC_2.38`; moving the Linux build job to Ubuntu 24.04 exposed a private `sync()`/POSIX declaration collision. Renaming the private editor helper to `syncSampDocument()` fixed that compile error without removing POSIX APIs or changing the dependency.

Final recipe commands use the configured Python 3.13 environment:

```powershell
python local-build/20261005-cli-recipes.py candidates
python local-build/20261005-cli-recipes.py recipes
python local-build/verify-final-recipes.py
python local-build/final-world-hashes.py
```

These drivers and their generated artifacts are ignored local evidence; the portable user recipe is in `docs/samp-authoring.md`. Every engine action in the recipe driver is an `arianectl.py` subprocess; exact CLI arguments and structured results are retained in `cli-records.json`. Final portable-binary recipe results: interior 40 objects/0 removals/43 patch operations, capture revisions 683 then 685; exterior 24 objects/1 removal/27 patch operations, revisions 691 then 693. Save/reload equality and one-action Undo/Redo pass. Offline saved-project checks confirm the sign slots are text materials at font sizes 20 and 24. Both exports compile with only the legacy `a_samp` warning. The driver restores the complete initial document/history snapshot and active state; final session is inactive with zero live instances, objects and removals at revision 696. Independent final hashing reports all 217 world files unchanged, none missing.

## Acceptance limits

No direct UI checks are claimed for this continuation. Existing historical UI evidence remains limited to the unchanged behavior and binaries recorded in the handoff.

D3D9 device reset is not equivalent to object destroy/recreate. The pinned renderer reaches reset through real window-size or device-state changes and exposes no safe reset IPC API. That gate remains open under the current computer-use exclusion.

Caller-side renderer failure injection does not establish recovery from every allocation failure inside the pinned `Clump::clone` implementation. Internal dependency allocation failures remain a separate gate.

Actual SA-MP/open.mp client texture/text parity and player entrance/collision checks require controlled target-client evidence. Editor captures and geometry warnings alone cannot establish client rendering or walkability.

A bounded attempt to start the existing Docker Desktop runtime through its CLI did not produce an available engine; no image was pulled and no container was started. The later hosted Linux CI build/core pass supplies the non-Windows compile evidence. It does not establish Linux runtime visual parity. A package archive does not establish clean-install acceptance or publication. The fork remains a development build until all required gates pass.
