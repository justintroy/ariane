# SA-MP editor usage — development build

This guide describes the current interface, not a verified release. See [the specification](../SPEC-samp-support.md) for acceptance criteria and [the handoff](samp-handoff.md) for known gaps. Stable Ariane does not supply these commands; use this Agent Alpha-based fork.

Testing update (2026-10-03): current Windows build, core/rotation/agent suites and live session/lifecycle/texture/window/filter checks pass. Direct UI interaction, native dialogs, clumps/device reset, non-Windows builds and target-client rendering remain unverified. See newest [handoff evidence](samp-handoff.md). Packaging execution produced an internal candidate only; no release is published.

October 4 update: the current Windows build and 97-test agent suite pass (one Unix-only skip). Missing-model warnings now follow current document state; live captures verified that Undo and rollback clear the warning while preserving the document. This does not replace direct UI interaction testing.

The latest source releases render resources for deleted objects and reuses runtime instances when changing models. Missing-model records remain editable document data. The user reported all eight supplied manual workflow checks passed on 2026-09-28. On 2026-10-03, 300 atomic recreation cycles and 150 replacement/delete/Undo cycles passed; clumps, device reset and stale-confirmation rejection remain pending. B6 live API session-safety checks pass. During an active scratch agent session, project saving and Pawn export are intentionally blocked until commit or rollback.

## Editor workflow

Run the build with a locally installed classic GTA San Andreas and the required locally supplied SA-MP assets. Windows resolves the game directory from the executable directory. The current test setup uses a separately named executable; do not overwrite an existing installation binary. Missing models/fonts/textures require diagnostics, not asset downloads.

Open **SA-MP**. In **Files & Objects**, enter Pawn paths or paste code, preview diagnostics, then apply supported records. **Replace input from clipboard** replaces the entire Pawn/file-path field; Ctrl+V pastes at the caret. A 65,999-byte, 1,200-object fixture was verified by copying it back byte-for-byte after the modifier-event fix. Input edits invalidate the previous preview. Import adds records; importing the same source again creates another set. Use project open to resume a saved document.

The object list has one checkbox per record. **Select all objects** checks every row; **Deselect all** clears the checks. Checks choose bulk deletion; clicking a row selects and frames that object in the viewport. **Delete checked** targets checked records; **Delete all objects** targets every object while keeping removals and project settings. The confirmation shows the count and defaults to Cancel; a document change prevents confirming an outdated selection. Each deletion is one undoable document action. The user verified Cancel, safe Enter behavior, checked/all deletion and one-action Undo with 1,200 objects, plus row selection versus checks and list scrolling. Rejection of an outdated confirmation still needs a separate interaction check.

Select a document object and edit its transform, material slots or text. Use **Textures** for local searches and tint. Use **Material Text** for font, size, colors, alignment and multiline content. Use **Removals** to preview world-building removals; world assets are not deleted from disk. **Code & History** separates current generated code from historical state snapshots.

Save editable state explicitly to `.samp.json`. Export normalized code explicitly to `.pwn`. Original imports remain unchanged. Dynamic exports need streamer includes and the streamer plugin in the consuming server; the editor itself does not embed that runtime. Combined exports use `SAMP_CreateMap()` and `SAMP_RemoveBuildings(playerid)`. Per-group exports use `SAMP_CreateMap_G1()`, `SAMP_RemoveBuildings_G1(playerid)`, etc., where number follows the group's index in the saved project. Call each function from the appropriate callback. A two-group combined Pawn fixture compiles; broader export validation remains pending.

## CLI and shared service

From the repository root, use the Python environment containing the agent dependencies. Launch/configure the editor's existing authenticated local IPC first. On Windows, editor and client share `ARIANE_ENGINE_TCP_PORT` and `ARIANE_ENGINE_TOKEN`. Do not publish the token. Use the upstream agent setup in `README.md` plus the local environment notes in the handoff.

```powershell
python tools/agent/arianectl.py capabilities
python tools/agent/arianectl.py samp inspect
python tools/agent/arianectl.py session begin samp-edit
python tools/agent/arianectl.py samp preview_import --file import-request.json
python tools/agent/arianectl.py samp import --file import-request.json
python tools/agent/arianectl.py samp code
python tools/agent/arianectl.py session commit
python tools/agent/arianectl.py samp save --file save-request.json
python tools/agent/arianectl.py samp export --file export-request.json
```

These examples assume an agent scene is already configured as required by the existing session API. Use `session rollback` instead of commit to discard the complete edit session. Save/export is rejected during an active session. Commit alone does not write a project or Pawn file.

SA-MP mutations require an active scratch session. `inspect`, `code`, `preview_import`, `window` and texture search remain available without mutation. Rollback restores document records, generated code, undo/redo/history and SA-MP active state, then leaves the session inactive. Document revision remains monotonic and advances after restoration; inspect the returned revision before the next mutation.

Named checkpoints include the complete SA-MP document snapshot, undo/redo/history and active state. Restore requires an active scratch session and preflights SA-MP data before mutation. Checkpoints with no generic scene objects can restore while SA-MP editing is active. A checkpoint containing generic agent-scene objects must be restored outside SA-MP editing; mixed ownership is rejected before mutation.

Do not switch agent scenes during an active session. Commit or rollback first. While SA-MP editing is active, use `samp place`, `samp update`, `samp delete` and other SA-MP document operations. Generic agent `place`, `batch`, `transform`, `delete`, `clear`, suppression and terrain-fit commands reject to prevent bypassing stable IDs, revisions and rollback ownership.

`samp inspect` also returns `asset_diagnostics`. Each entry has a stable `code`, `severity`, record `kind` and `id`, plus relevant model/material fields. Current codes are `model_missing`, `txd_missing`, `txd_unavailable`, `texture_missing`, `font_unavailable`, `font_substituted`, and non-Windows `text_preview_unsupported`. Missing assets do not remove document records. Live inspection verified `model_missing` against an unavailable model; other diagnostics still need the rendering matrix.

`import-request.json`:

```json
{"files":[{"name":"example.pwn","source":"new obj = CreateDynamicObject(19379, 2490.0, -1665.0, 18.0, 0.0, 0.0, 90.0);"}]}
```

Omit `source` to read the path in `name`. Preview diagnostics first. Only set `accept_diagnostics: true` after deliberately accepting the supported subset. For automation, inspect the document and send its `revision` as `expected_revision` with every mutation. If a response is lost, inspect again. A changed revision means the request may have succeeded; compare IDs and fields before any new action. An unchanged revision permits retry with the same expected revision. Replaying a successful request returns `stale document revision`; this is a rejection, not a replay receipt. Do not blindly retry imports, placement, duplication, removals or patches.

`save-request.json`: `{"path":"C:/maps/example.samp.json"}`

`export-request.json`: `{"path":"C:/maps/example.pwn"}`

## Current static Pawn subset

The importer reads source text only. Supported calls: `CreateObject`, `CreateDynamicObject`, both native and dynamic material/MaterialText setters, and `RemoveBuildingForPlayer`. It resolves numeric literals, prior `#define`/`const` values, simple assignments and array indexes. Expression operators: unary `+`, `-`, `~`; binary `*`, `/`, `%`, `+`, `-`, `<<`, `>>`, `&`, `^`, `|`; parentheses; and simple Pawn tags followed by `:`. Bitwise operands must fit a 32-bit Pawn cell; shifts must be 0–31. Strings support `\n`, `\r`, `\t`, escaped quotes/backslashes and line continuation.

The parser reports unsupported preprocessor directives, runtime control flow, unresolved expressions and object references. Map calls require `;`. Preview diagnostics before applying supported records. It does not execute callbacks, conditional preprocessor branches, macros beyond simple numeric `#define`, runtime variables or custom map functions. Source files remain untouched. Function blocks scope object references and `const` values. Generated code includes source group comments. Streamer defaults and dynamic material-text argument order follow [streamer.inc](https://github.com/samp-incognito/samp-streamer-plugin/blob/master/streamer.inc); native object/material signatures follow [open.mp CreateObject](https://open.mp/docs/scripting/functions/CreateObject), [SetObjectMaterial](https://open.mp/docs/scripting/functions/SetObjectMaterial) and [SetObjectMaterialText](https://open.mp/docs/scripting/functions/SetObjectMaterialText).

Current operations: `inspect`, `code`, `preview`, `preview_import`, `import`, `place`, `update`, `delete`, `duplicate`, `material`, `removal`, `textures`, `open`, `save`, `export`, `clear`, `undo`, `redo`, `patch`, `replace`, `window`.

- `preview`: `world`, `interior` filter fields; mutates document preview filter state (requires active session when mutating document preview settings).
- `window`: optional `show` boolean; queries or toggles SA-MP editor ImGui window visibility (UI presentation state only; non-document-mutating).
- `place`: `object` containing model, position/rotation and optional creation fields.
- `update`: `id`, `changes`; optional `kind: "removals"` for removal records.
- `material`: `id`, `slot`, `material`; `null` removes an override. Texture material fields are `type`, `model`, `txd`, `texture`, `color`. Text fields are `type`, `text`, `size`, `font`, `font_size`, `bold`, `foreground`, `background`, `align`.
- `removal`: `removal` containing model, position, radius and optional group.
- `patch`: `operations` array; stages supported operations and commits atomically. Retry receipt semantics remain under review.
- `inspect`: optional `include_history: true`.
- History retains at most 64 actions, including undo and redo. Each entry has `label`, `revision`, full generated `code` snapshot and `affected` records with before/after values. Snapshots are historical document states, not executable patches.
- `code`/`export`: optional `group`.

SA-MP MCP support has been dropped in favor of CLI (`arianectl samp <operation>`) and shared Python service dispatch (`samp.<operation>`). Both forward to the shared C++ document service under identical validation and session rules. Forwarding tests cover 21 operations and pass in the 97-test run (one Windows skip). Current live B6 tests verify session/checkpoint behavior.

## Before release

Replace provisional setup notes with a tested clean-install procedure and published package links. Document the exact static Pawn subset and diagnostics, verified UI interactions, all request/response schemas, checkpoint usage and remaining client-render differences. Keep [Ariane AGENTS.md](../AGENTS.md), Roleplay root guidance and gta3dai guidance linked to these instructions.
