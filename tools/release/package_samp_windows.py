#!/usr/bin/env python3
"""Package preparation tool for Ariane SA-MP editor Windows distribution.

This script inspects and stages the Windows x64 distribution bundle:
- Executable (bin/win-amd64-d3d9/Release/ariane.exe)
- UI and icon fonts (fonts/Inter-Regular.ttf, fonts/fa-solid-900.ttf)
- Documentation and dependency notices (docs/samp-usage.md, docs/samp-build.md, docs/NOTICES-samp.md, etc.)
- Small sample maps (samples/samp/)
- Python agent wheel (optional)
- Provenance manifest (BUILD_INFO.json) and SHA-256 checksums (SHA256SUMS)

SAFETY AND ISOLATION ENFORCEMENT:
- By default, runs in dry-run mode (--dry-run).
- Excludes all GTA game assets (*.dff, *.txd, *.col, *.ipl, *.ide, *.dat, *.img).
- Excludes credentials, tokens, and runtime metadata (*runtime-env.json, *token*, *.key).
- Excludes build intermediates and caches (__pycache__, *.obj, *.pdb, .vs, local-build/).
- Requires explicit flags to create an archive and guards against packaging unverified builds.
- Never downloads game assets or changes git/dependency pins.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import zipfile

PINNED_LIBRW_COMMIT = "15ffa585216a9a7573ecc597b19ce2fde9b935f2"
TARGET_PUBLISHING_FORK = "https://github.com/justintroy/ariane"
UPSTREAM_REPO = "https://github.com/Dryxio/ariane"
ENGINE_PACKAGE_NAME = "ariane.exe"

FORBIDDEN_EXTENSIONS = {
    ".dff", ".txd", ".col", ".ipl", ".ide", ".dat", ".img",
    ".gta3", ".rpf", ".raw", ".obj", ".pdb", ".ilk", ".exp",
    ".pyc", ".tmp"
}

FORBIDDEN_NAME_PATTERNS = [
    re.compile(r"runtime-env\.json", re.IGNORECASE),
    re.compile(r".*token.*", re.IGNORECASE),
    re.compile(r".*\.key$", re.IGNORECASE),
    re.compile(r".*\.secret$", re.IGNORECASE),
    re.compile(r"__pycache__", re.IGNORECASE),
]


def check_forbidden_file(path: Path) -> str | None:
    """Return reason string if file violates exclusion rules, or None if clean."""
    if path.suffix.lower() in FORBIDDEN_EXTENSIONS:
        return f"Forbidden file extension: {path.suffix}"
    for part in path.parts:
        for pat in FORBIDDEN_NAME_PATTERNS:
            if pat.search(part):
                return f"Forbidden file pattern match: {pat.pattern}"
    return None


def check_output_targets(output_dir: Path, package_name: str) -> tuple[Path, Path]:
    """Resolve package destinations and refuse to overwrite existing user data."""
    output_root = output_dir.resolve()
    stage_dir = output_root / package_name
    zip_path = output_root / f"{package_name}.zip"
    if stage_dir.is_symlink() or stage_dir.exists():
        raise FileExistsError(f"staging directory already exists: {stage_dir}")
    if zip_path.is_symlink() or zip_path.exists():
        raise FileExistsError(f"archive already exists: {zip_path}")
    if stage_dir.resolve().parent != output_root:
        raise ValueError("staging directory must stay directly within the output directory")
    return stage_dir, zip_path


def engine_candidate(engine_path: Path) -> tuple[Path, str]:
    """Normalize development/test binary names to the documented install name."""
    return engine_path, ENGINE_PACKAGE_NAME


def write_install_note(path: Path, has_wheel: bool) -> None:
    """Explain how to use the extracted editor bundle without implying assets ship."""
    cli_note = (
        "Optional Python agent wheel is in tools/. Install it with Python 3.10+ and pip; "
        "it connects only to a matching Ariane editor build.\n"
        if has_wheel else
        "Python agent CLI is not included in this editor-only package.\n"
    )
    path.write_text(
        "Ariane SA-MP editor for Windows x64 — internal development candidate\n\n"
        "This package is not release-validated. Extract it, then copy the executable and "
        "fonts folder into the root of your own supported GTA San Andreas installation. "
        "If ariane.exe already exists, preserve it and rename this candidate to "
        "ariane-samp-test.exe before copying. Back up existing same-name font files "
        "before replacing them. Launch the candidate from the game root; do not launch "
        "it from a nested package folder.\n\n"
        "This package contains no GTA game files or SA-MP client assets.\n"
        "The package has not been validated by installing it into a clean game copy.\n"
        + cli_note,
        encoding="utf-8",
    )


def get_git_info(repo_root: Path) -> dict[str, str]:
    """Retrieve git branch and commit information safely."""
    info = {"commit": "unknown", "branch": "unknown", "dirty": "unknown"}
    try:
        commit = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        info["commit"] = commit
    except Exception:
        pass

    try:
        branch = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "--abbrev-ref", "HEAD"],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        info["branch"] = branch
    except Exception:
        pass

    try:
        status = subprocess.check_output(
            ["git", "-C", str(repo_root), "status", "--porcelain"],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        info["dirty"] = "true" if status else "false"
    except Exception:
        pass

    return info


def verify_librw_pin(librw_dir: Path | None) -> str:
    """Check whether librw checkout matches the required pinned commit."""
    if not librw_dir or not librw_dir.exists():
        return "not_found"
    try:
        head = subprocess.check_output(
            ["git", "-C", str(librw_dir), "rev-parse", "HEAD"],
            text=True, stderr=subprocess.DEVNULL
        ).strip()
        if head == PINNED_LIBRW_COMMIT:
            return "matches_pin"
        return f"mismatch_{head}"
    except Exception:
        return "unverified"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare and validate Ariane SA-MP Windows distribution package."
    )
    root = Path(__file__).resolve().parents[2]

    default_engine = root / "bin" / "win-amd64-d3d9" / "Release" / "ariane.exe"
    default_output = root / "dist"

    parser.add_argument(
        "--engine", type=Path, default=default_engine,
        help=f"Path to ariane.exe (default: {default_engine})"
    )
    parser.add_argument(
        "--output", type=Path, default=default_output,
        help=f"Output directory for staging and packages (default: {default_output})"
    )
    parser.add_argument(
        "--name", type=str, default="ariane-samp-v1.40.9-windows-x64",
        help="Package base directory name"
    )
    parser.add_argument(
        "--wheel", type=Path, default=None,
        help="Path to pre-built Python agent .whl (optional)"
    )
    parser.add_argument(
        "--librw-dir", type=Path, default=None,
        help="Path to librw directory for commit validation"
    )
    parser.add_argument(
        "--dry-run", action="store_true", default=True,
        help="Inspect layout and validate rules without creating package zip (default: True)"
    )
    parser.add_argument(
        "--create-archive", action="store_true", default=False,
        help="Explicitly create the distribution .zip archive (disables --dry-run)"
    )
    parser.add_argument(
        "--allow-unverified", action="store_true", default=False,
        help="Acknowledge that editor source tests/acceptance are deferred/unverified"
    )
    parser.add_argument(
        "--strict", action="store_true", default=False,
        help="Fail with non-zero exit code if prerequisites are missing even in dry-run mode"
    )

    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.name) or args.name in {".", ".."}:
        parser.error("--name must be a single safe directory name")
    if args.wheel and not args.wheel.is_file():
        parser.error(f"--wheel does not name an existing file: {args.wheel}")

    is_dry_run = args.dry_run and not args.create_archive

    print("=" * 60)
    print(" Ariane SA-MP Windows Distribution Package Preparation")
    print(" [NOTE] In-development delivery preparation draft (untested)")
    print("=" * 60)
    print(f"Repository Root:    {root}")
    print(f"Execution Mode:     {'DRY-RUN (Inspection Only)' if is_dry_run else 'STAGING & PACKAGING'}")
    print(f"Target Binary:      {args.engine}")
    print(f"Output Directory:   {args.output}")
    print(f"Package Name:       {args.name}")

    # 1. Inspect Git and dependency pin
    git_info = get_git_info(root)
    print(f"Git Commit:         {git_info['commit']}")
    print(f"Git Branch:         {git_info['branch']}")
    print(f"Working Tree Dirty: {git_info['dirty']}")

    librw_path = args.librw_dir or root.parent / "ariane-librw"
    librw_status = verify_librw_pin(librw_path)
    print(f"librw Status:       {librw_status} (Expected: {PINNED_LIBRW_COMMIT})")

    # Enforce exact dependency pin when packaging
    if args.create_archive:
        if librw_status != "matches_pin":
            print(f"\n[ERROR] librw dependency pin mismatch: status is '{librw_status}', expected '{PINNED_LIBRW_COMMIT}'.", file=sys.stderr)
            print("Exact dependency pin is required for actual build and packaging.", file=sys.stderr)
            return 1

    # 2. Safety check: Guard against accidental unverified release creation
    if args.create_archive and not args.allow_unverified:
        print("\n[ERROR] --create-archive was requested without --allow-unverified.")
        print("B0 through B8 acceptance gates have deferred testing validation.")
        print("Per project rules, unverified packages must not be published as releases.")
        print("Pass --allow-unverified if you are staging an internal pre-test candidate.")
        return 1

    # 3. Check required files
    missing_items: list[str] = []
    if not args.engine.exists():
        missing_items.append(f"Engine binary missing: {args.engine}")

    fonts_dir = root / "fonts"
    if not fonts_dir.exists():
        missing_items.append(f"Fonts directory missing: {fonts_dir}")
    else:
        for font in ["Inter-Regular.ttf", "fa-solid-900.ttf"]:
            if not (fonts_dir / font).exists():
                missing_items.append(f"Required font missing: fonts/{font}")

    docs_to_bundle = [
        (root / "docs" / "samp-usage.md", "docs/samp-usage.md"),
        (root / "docs" / "samp-build.md", "docs/samp-build.md"),
        (root / "docs" / "NOTICES-samp.md", "docs/NOTICES-samp.md"),
        (root / "tools" / "euryopa" / "minilzo" / "COPYING", "docs/COPYING-LZO"),
        (root / "tools" / "euryopa" / "minilzo" / "README.LZO", "docs/README.LZO"),
        (root / "README.md", "docs/README-upstream.md"),
    ]
    if librw_path.exists() and (librw_path / "LICENSE").exists():
        docs_to_bundle.append((librw_path / "LICENSE", "docs/LICENSE-librw.txt"))

    for src, _ in docs_to_bundle:
        if not src.exists():
            missing_items.append(f"Documentation/notice missing: {src}")

    samples_dir = root / "samples" / "samp"
    if not samples_dir.exists():
        missing_items.append(f"Sample maps directory missing: {samples_dir}")

    if missing_items:
        print("\n[WARNING] Staging prerequisites missing:")
        for item in missing_items:
            print(f"  - {item}")
        print("\n[NOTE] Package manifest is incomplete. Binary and required items must be built before release packaging.")
        if not is_dry_run or args.strict:
            return 1

    # 4. Enforce security / game asset exclusion checks across candidates
    print("\n--- Validating Safety and Asset Exclusion Rules ---")
    staged_candidates: list[tuple[Path, str]] = []

    if args.engine.exists():
        staged_candidates.append(engine_candidate(args.engine))

    if fonts_dir.exists():
        for f in sorted(fonts_dir.glob("*.ttf")):
            staged_candidates.append((f, f"fonts/{f.name}"))

    for src, dest_rel in docs_to_bundle:
        if src.exists():
            staged_candidates.append((src, dest_rel))

    # Positive allowlist for sample maps: only .pwn, .samp.json, .md
    ALLOWED_SAMPLE_EXTENSIONS = {".pwn", ".samp.json", ".md"}
    if samples_dir.exists():
        for f in sorted(samples_dir.rglob("*")):
            if f.is_file():
                if f.suffix.lower() not in ALLOWED_SAMPLE_EXTENSIONS and not f.name.lower().endswith(".samp.json"):
                    print(f"\n[ERROR] Stray file in samples directory not in allowlist: {f}", file=sys.stderr)
                    return 2
                staged_candidates.append((f, f"samples/{f.relative_to(samples_dir).as_posix()}"))

    if args.wheel:
        staged_candidates.append((args.wheel, f"tools/{args.wheel.name}"))

    violations: list[str] = []
    for src, dest in staged_candidates:
        reason = "Symbolic link sources are not permitted in a package" if src.is_symlink() else check_forbidden_file(src)
        if reason is None:
            reason = check_forbidden_file(Path(dest))
        if reason:
            violations.append(f"{src} -> {reason}")

    if violations:
        print("\n[SECURITY VIOLATION] Forbidden files detected in package manifest:")
        for v in violations:
            print(f"  - {v}")
        return 2

    print("All candidate files passed asset, credential, and allowlist checks.")
    print(f"Total files in package manifest: {len(staged_candidates)}")

    # 5. Manifest summary
    print("\n--- Proposed Package Manifest ---")
    for src, dest in staged_candidates[:15]:
        print(f"  {dest: <40} <- {src}")
    if len(staged_candidates) > 15:
        print(f"  ... and {len(staged_candidates) - 15} more files.")

    if is_dry_run:
        print("\n" + "=" * 60)
        print(" DRY-RUN COMPLETE. No files or archives were written.")
        print(" To perform actual staging, pass --create-archive --allow-unverified")
        print("=" * 60)
        return 0

    # 6. Actual Staging (if requested)
    try:
        stage_dir, zip_path = check_output_targets(args.output, args.name)
    except (FileExistsError, ValueError) as exc:
        parser.error(str(exc))
    output_root = args.output.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    stage_dir.mkdir(parents=False, exist_ok=False)

    print(f"\nStaging files to: {stage_dir}")
    for src, dest_rel in staged_candidates:
        dest_path = stage_dir / dest_rel
        dest_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest_path)

    write_install_note(stage_dir / "INSTALL.txt", bool(args.wheel and args.wheel.is_file()))

    # Write BUILD_INFO.json
    build_info = {
        "package_name": args.name,
        "platform": platform.platform(),
        "architecture": "x86_64",
        "target_binary": ENGINE_PACKAGE_NAME,
        "install_location": "GTA San Andreas installation root",
        "source_repository": TARGET_PUBLISHING_FORK,
        "upstream_repository": UPSTREAM_REPO,
        "source_branch": git_info["branch"],
        "source_commit": git_info["commit"],
        "working_tree_dirty": git_info["dirty"],
        "librw_pinned_commit": PINNED_LIBRW_COMMIT,
        "librw_status": librw_status,
        "release_status": "unverified_preparation_candidate",
        "game_assets_included": False,
        "credentials_included": False,
    }
    (stage_dir / "BUILD_INFO.json").write_text(json.dumps(build_info, indent=2) + "\n", encoding="utf-8")

    # Generate SHA256SUMS
    staged_files = sorted(f for f in stage_dir.rglob("*") if f.is_file())
    sums_lines = []
    for f in staged_files:
        rel = f.relative_to(stage_dir).as_posix()
        sums_lines.append(f"{sha256_file(f)}  {rel}\n")
    (stage_dir / "SHA256SUMS").write_text("".join(sums_lines), encoding="utf-8")

    # Create zip archive
    print(f"Creating zip archive: {zip_path}")
    with zipfile.ZipFile(zip_path, "x", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(stage_dir.rglob("*")):
            if f.is_file():
                z.write(f, Path(args.name) / f.relative_to(stage_dir))

    zip_sha = sha256_file(zip_path)
    print("\n" + "=" * 60)
    print(" Package created successfully!")
    print(f" Archive: {zip_path}")
    print(f" SHA-256: {zip_sha}")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
