#!/usr/bin/env python3
"""Audit direct QoreValue string-node extraction sites.

Short strings can be stored inline in QoreValue, so logical string type
(`NT_STRING`) does not imply that `QoreValue::get<QoreStringNode>()` is safe.
This tool finds direct extraction idioms that need review and classifies the
line with a coarse risk signal.
"""

from __future__ import annotations

import argparse
import fnmatch
import re
from collections import Counter
from pathlib import Path


SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".qpp",
}

SKIP_DIRS = {
    ".git",
    ".hg",
    ".svn",
    "build",
    "container-build",
    "debug",
    "release",
    "RelWithDebInfo",
    "CMakeFiles",
    "__pycache__",
}

PATTERNS = [
    ("direct-get", re.compile(r"(?:\.|->)get<\s*(?:const\s+)?QoreStringNode\s*\*?\s*>\s*\(")),
    ("hard-macro", re.compile(r"\bHARD_QORE_VALUE_STRING\b|\bHARD_QORE_VALUE_PARAM\b.*QoreStringNode")),
    ("temp-encoding", re.compile(r"\bTempEncodingHelper\s*\([^;\n]*(?:QoreStringNode|getKeyValue|retrieveEntry|\.get<)")),
]

HIGH_RISK_HINTS = [
    "getKeyValue",
    "retrieveEntry",
    "li.getValue",
    "hi.get",
    "getExceptionErr",
    "getExceptionDesc",
    "getExceptionArg",
    "get_param_value",
    "get_hard_value_param",
    "HARD_QORE_VALUE_STRING",
    "HARD_QORE_VALUE_PARAM",
    "ValueHolder",
]

MUTABLE_HINTS = [
    "getValue().get<",
    "lvh.getValue().get<",
    "val.getValue().get<",
    "v.getValue().get<",
]

ALLOW_PATTERNS = [
    re.compile(r"QoreStringValueHelper"),
    re.compile(r"QoreStringNodeValueHelper"),
]


def is_materialized_string_get(path: Path, line: str, context: str) -> bool:
    """Return true for direct gets guarded by explicit short-string materialization."""
    if path.name == "QoreStringNode.cpp" and "n.get<const QoreStringNode>" in line:
        # QoreString*ValueHelper::setup() handles short strings before this
        # optimized node fast path.
        return True

    if (".get<" not in line and "->get<" not in line) or "QoreStringNode" not in line:
        return False

    materializers = (
        "ensure_unique(v",
        "ensure_unique(hi.get()",
        "lvh.ensureUnique()",
        "val.ensureUnique()",
        "v.ensureUnique()",
    )
    if any(m in context for m in materializers):
        return True

    near_context = "\n".join(context.splitlines()[-20:])
    if "isShortString()" in near_context and "getShortString(" in near_context:
        return True

    return False


def iter_roots(args: argparse.Namespace) -> list[Path]:
    roots = [Path(p).expanduser().resolve() for p in args.paths]
    if args.sibling_modules:
        base = Path(args.sibling_modules_base).expanduser().resolve()
        roots.extend(sorted(p.resolve() for p in base.glob("module-*") if p.is_dir()))
    return roots


def should_skip_dir(path: Path) -> bool:
    return path.name in SKIP_DIRS or path.name.startswith("build-")


def iter_source_files(root: Path):
    if root.is_file():
        if root.suffix in SOURCE_SUFFIXES:
            yield root
        return
    for path in root.rglob("*"):
        if any(should_skip_dir(parent) for parent in path.parents):
            continue
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            yield path


def classify(line: str, kind: str) -> str:
    if any(p.search(line) for p in ALLOW_PATTERNS):
        return "skip-helper"
    if kind in {"hard-macro", "temp-encoding"}:
        return "high"
    if any(h in line for h in MUTABLE_HINTS):
        return "mutable-review"
    if any(h in line for h in HIGH_RISK_HINTS):
        return "high"
    if "getType() == NT_STRING" in line or "getType()!= NT_STRING" in line or "getType() != NT_STRING" in line:
        return "high"
    if kind == "direct-get":
        return "high"
    return "review"


def matches_file(path: Path, include_globs: list[str], exclude_globs: list[str]) -> bool:
    text = str(path)
    if include_globs and not any(fnmatch.fnmatch(text, pat) for pat in include_globs):
        return False
    return not any(fnmatch.fnmatch(text, pat) for pat in exclude_globs)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", default=["."], help="paths to scan")
    parser.add_argument("--sibling-modules", action="store_true", help="also scan sibling module-* repos")
    parser.add_argument(
        "--sibling-modules-base",
        default="~/src/qore/git",
        help="directory containing sibling module-* repos",
    )
    parser.add_argument("--include", action="append", default=[], help="fnmatch path glob to include")
    parser.add_argument("--exclude", action="append", default=[], help="fnmatch path glob to exclude")
    parser.add_argument("--summary-only", action="store_true", help="only print aggregate counts")
    parser.add_argument(
        "--risk",
        action="append",
        choices=["high", "mutable-review", "review", "skip-helper"],
        help="only print matching risk classes; may be repeated",
    )
    args = parser.parse_args()

    risk_filter = set(args.risk or [])
    rows = []
    counters = Counter()
    repo_counters = Counter()

    for root in iter_roots(args):
        repo_name = root.name
        for path in iter_source_files(root):
            if not matches_file(path, args.include, args.exclude):
                continue
            try:
                lines = path.read_text(errors="replace").splitlines()
            except OSError as exc:
                print(f"warn: cannot read {path}: {exc}")
                continue
            for lineno, line in enumerate(lines, 1):
                for kind, regex in PATTERNS:
                    if not regex.search(line):
                        continue
                    context = "\n".join(lines[max(0, lineno - 80):lineno])
                    if kind == "direct-get" and is_materialized_string_get(path, line, context):
                        continue
                    risk = classify(line, kind)
                    if risk == "skip-helper":
                        continue
                    counters[(risk, kind)] += 1
                    repo_counters[(repo_name, risk)] += 1
                    if not risk_filter or risk in risk_filter:
                        rows.append((repo_name, risk, kind, path, lineno, line.strip()))

    if not args.summary_only:
        for repo_name, risk, kind, path, lineno, line in rows:
            print(f"{repo_name}\t{risk}\t{kind}\t{path}:{lineno}\t{line}")

    print("\nsummary by risk/kind:")
    for (risk, kind), count in sorted(counters.items()):
        print(f"{risk}\t{kind}\t{count}")

    print("\nsummary by repo/risk:")
    for (repo_name, risk), count in sorted(repo_counters.items()):
        print(f"{repo_name}\t{risk}\t{count}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
