# Ariane fork: agent guidance

This checkout contains an **in-development** SA-MP editor extension based on Ariane Agent Alpha. Do not describe it as a finished release.

## Start here

- Read [the feature specification](SPEC-samp-support.md) and [current handoff](docs/samp-handoff.md) before development. Continue incomplete batches; preserve existing changes.
- Read [SA-MP usage](docs/samp-usage.md) for UI, CLI and session rules, and [authoring](docs/samp-authoring.md) for declarative plans and review captures. SA-MP MCP is excluded; unrelated upstream MCP remains available.
- Related workspace guidance: [Roleplay root AGENTS.md](../Roleplay-Project-v2/AGENTS.md) and [gta3dai AGENTS.md](../Roleplay-Project-v2/tools/gta3dai/AGENTS.md). These sibling links assume the agreed checkout layout. They apply when working in those projects; do not impose Pawn-only memory constraints on Ariane's C++ implementation.
- Keep user-facing updates concise. Write durable documentation and code in clear English.

## Invariants

- Keep librw pinned to the version recorded in the specification.
- Use the shared SA-MP document service for UI, CLI and MCP changes. Stable IDs and document revisions are the API contract.
- Keep instance materials separate from shared model geometry. Test duplication, undo, reload and resource recreation when changing ownership.
- Keep vanilla world assets separate. Never save SA-MP state through IPL/IMG paths.
- Parse static Pawn; never execute imported code. Diagnose unsupported expressions/logic and preserve original files.
- Agent mutations require an existing session. Commit accepts edits; explicit save/export writes files. Rollback must restore the complete SA-MP state.
- Keep local game assets, caches, generated builds and tokens out of Git. Do not print authentication environment files.
- Do not push to Dryxio's upstream. The intended publishing fork is `justintroy/ariane`.

## Development and handoff

Use the commands and environment notes in the handoff. Validate changes with appropriate core, Pawn compilation, agent and rendering checks. Split large source files by responsibility without changing behavior, and rerun relevant tests after splitting.

Tracked Windows commands: `tools/build/build_windows.ps1` builds the pinned dependency and editor; `tools/build/run_samp_core_test.ps1` compiles and runs document/rotation checks. Both discover the installed MSVC environment and accept explicit dependency paths. Build output and evidence belong under ignored local directories. The read-only SA-MP validation CI includes Linux OpenGL; workflow presence alone is not a passing build.

Before handing off, update the specification's current batch and `docs/samp-handoff.md`: exact stopping point, modified files, evidence, failures, running test processes and next action. Do not mark a batch complete merely because its code exists. Final delivery must update this file and the usage guide to reflect tested behavior and published artifacts.
