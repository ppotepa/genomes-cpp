"""Static boundary checks for the lightweight-toolkit migration.

This checks active source, build configuration, scripts and public headers.
Historical migration notes and preserved JS/reference fixtures are excluded;
the guard never modifies the tree.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys

_LEGACY = "three" + "pp"
_GL = "GL"
FORBIDDEN = (_LEGACY, _LEGACY.upper(), _GL + "Renderer", _GL + "FW")
PUBLIC_EXTERNAL = ("Diligent::", "fastgltf::", "manifold::", "meshopt_")
_TRACKER_STATUSES = ("TODO", "DOING", "CODE_READY", "WAIT_USER", "VERIFIED", "BLOCKED")


def check_tracker(root: pathlib.Path, failures: list[str]) -> None:
    tracker = root / "docs/migration/lightweight-toolkit/POSTEP.txt"
    if not tracker.is_file():
        failures.append("lightweight-toolkit tracker is missing")
        return
    text = tracker.read_text(encoding="utf-8", errors="replace")
    denominator_match = re.search(r"^Denominator:\s*(\d+)\s+distinct paths$", text, re.MULTILINE)
    if denominator_match is None or int(denominator_match.group(1)) != 167:
        failures.append("tracker denominator must be 167")
        return
    row_pattern = re.compile(
        r"^LT-(\d{3}) \| (" + "|".join(_TRACKER_STATUSES) + r") \|",
        re.MULTILINE,
    )
    rows = row_pattern.findall(text)
    ids = [int(identifier) for identifier, _ in rows]
    if len(rows) != 167 or sorted(ids) != list(range(1, 168)):
        failures.append("tracker must contain exactly one row for every LT-001..LT-167")
        return
    counts = {status: sum(row_status == status for _, row_status in rows)
              for status in _TRACKER_STATUSES}
    summary_pattern = re.compile(
        r"^TODO=(\d+) DOING=(\d+) CODE_READY=(\d+) WAIT_USER=(\d+) "
        r"VERIFIED=(\d+) BLOCKED=(\d+)$", re.MULTILINE,
    )
    summary = summary_pattern.search(text)
    if summary is None or any(int(value) != counts[status]
                              for value, status in zip(summary.groups(), _TRACKER_STATUSES)):
        failures.append("tracker status summary does not match its LT rows")
        return
    code_ready = counts["CODE_READY"] + counts["WAIT_USER"] + counts["VERIFIED"]
    verified = counts["VERIFIED"]
    progress_pattern = re.compile(
        r"^(Code|Acceptance) progress: (\d+)/167 = ([0-9]+\.[0-9]{2})%; remaining ([0-9]+\.[0-9]{2})%$",
        re.MULTILINE,
    )
    progress = progress_pattern.findall(text)
    expected = {"Code": code_ready, "Acceptance": verified}
    if len(progress) != 2 or any(
        int(numerator) != expected[kind]
        or abs(float(percent) - round(100.0 * expected[kind] / 167.0, 2)) > 0.001
        or abs(float(remaining) - round(100.0 - 100.0 * expected[kind] / 167.0, 2)) > 0.001
        for kind, numerator, percent, remaining in progress
    ):
        failures.append("tracker progress arithmetic does not match its LT rows")


def cache_bool(cache_text: str, name: str) -> bool | None:
    match = re.search(rf"^{re.escape(name)}:BOOL=(ON|OFF)$", cache_text, re.MULTILINE)
    return None if match is None else match.group(1) == "ON"


def check_build_boundaries(build: pathlib.Path, failures: list[str]) -> None:
    cache = build / "CMakeCache.txt"
    if not cache.exists():
        return
    cache_text = cache.read_text(encoding="utf-8", errors="replace")
    if cache_bool(cache_text, "GENOMES_ENABLE_" + _LEGACY.upper()):
        failures.append("build cache enables the legacy CPU provider")

    backend_match = re.search(
        r"^GENOMES_RENDER_BACKEND:STRING=([^\r\n]+)$", cache_text, re.MULTILINE,
    )
    backend = backend_match.group(1) if backend_match else None
    if backend == "HEADLESS":
        if cache_bool(cache_text, "GENOMES_ENABLE_DILIGENT") is True:
            failures.append("HEADLESS build enables Diligent")
        if cache_bool(cache_text, "GENOMES_ENABLE_SDL") is True:
            failures.append("HEADLESS build enables SDL")

    optional_off = (
        cache_bool(cache_text, "GENOMES_ENABLE_ASSETS") is False
        and cache_bool(cache_text, "GENOMES_ENABLE_CSG") is False
    )
    game_link_files = [
        path for path in build.rglob("link.txt")
        if "genomes_game" in path.as_posix().lower()
    ]
    for link_file in game_link_files:
        link_text = link_file.read_text(encoding="utf-8", errors="replace").lower()
        if backend == "HEADLESS" and any(token in link_text for token in ("diligent", "sdl")):
            failures.append(f"HEADLESS game link contains graphics dependency: {link_file.relative_to(build)}")
        if optional_off and any(token in link_text for token in ("fastgltf", "manifold")):
            failures.append(f"game link contains optional toolkit dependency while assets/CSG are OFF: {link_file.relative_to(build)}")


def check_presets(root: pathlib.Path, failures: list[str]) -> None:
    presets_path = root / "CMakePresets.json"
    try:
        presets = json.loads(presets_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"CMakePresets.json is not valid JSON: {error}")
        return
    required = {
        "dev-debug", "dev-release",
        "headless-core-debug", "headless-core-release",
        "toolkit-full-debug", "toolkit-full-release",
    }
    for kind in ("configurePresets", "buildPresets", "testPresets"):
        names = {entry.get("name") for entry in presets.get(kind, [])}
        missing = sorted(required - names)
        if missing:
            failures.append(f"CMakePresets.json is missing {kind}: {', '.join(missing)}")


def tracked_files(root: pathlib.Path) -> list[pathlib.Path]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=True, stdout=subprocess.PIPE, text=False,
    )
    return [root / pathlib.Path(name) for name in result.stdout.decode().split("\0") if name]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--build", type=pathlib.Path)
    args = parser.parse_args()
    root = args.root.resolve()
    failures: list[str] = []
    check_tracker(root, failures)
    check_presets(root, failures)
    for path in tracked_files(root):
        relative = path.relative_to(root)
        if relative.name == "concat.txt" or (
            relative.parts and relative.parts[0] in {"docs", "reference"}
        ):
            continue
        if not path.is_file() or path.suffix.lower() not in {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".cmake", ".txt", ".ps1", ".cmd", ".py"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for token in FORBIDDEN:
            if token in text:
                failures.append(f"{path.relative_to(root)}: forbidden token {token}")
        if "include/" in path.as_posix() and any(token in text for token in PUBLIC_EXTERNAL):
            failures.append(f"{path.relative_to(root)}: third-party type in public header")
    if args.build:
        check_build_boundaries(args.build.resolve(), failures)
    if failures:
        print("lightweight-toolkit static guard: FAIL")
        print("\n".join(sorted(set(failures))))
        return 1
    print("lightweight-toolkit static guard: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
