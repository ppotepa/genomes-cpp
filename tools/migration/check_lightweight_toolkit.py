"""Static boundary checks for the lightweight-toolkit migration.

This checks active source, build configuration, scripts and public headers.
Historical migration notes and preserved JS/reference fixtures are excluded;
the guard never modifies the tree.
"""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

_LEGACY = "three" + "pp"
_GL = "GL"
FORBIDDEN = (_LEGACY, _LEGACY.upper(), _GL + "Renderer", _GL + "FW")
PUBLIC_EXTERNAL = ("Diligent::", "fastgltf::", "manifold::", "meshopt_")


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
        cache = args.build / "CMakeCache.txt"
        if cache.exists():
            cache_text = cache.read_text(encoding="utf-8", errors="replace")
            if ("GENOMES_ENABLE_" + _LEGACY.upper() + ":BOOL=ON") in cache_text:
                failures.append("build cache enables the legacy CPU provider")
    if failures:
        print("lightweight-toolkit static guard: FAIL")
        print("\n".join(sorted(set(failures))))
        return 1
    print("lightweight-toolkit static guard: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
