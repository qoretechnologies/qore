#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Checks that a change to the public Qore headers comes with a decision about the binary module API.

A binary module carries the inline code and the type layouts of the public headers that it was compiled with, so a
change to them that a module built with the previous headers would get wrong must increment QORE_MODULE_API_MINOR
(design/module-api-version.md).  This script hashes the installed header set (the files that "install(DIRECTORY
include/qore ...)" in CMakeLists.txt installs, with comments and whitespace removed; in a git checkout only the files
that git tracks, so that untracked leftovers do not count) and compares the hash with the one recorded with the module
API version in cmake/module-api-headers.txt:

- the hash differs and the API is the same: the headers changed without a decision; either increment the API (a
  change that a compiled module would get wrong) or, for a change that cannot affect a compiled module (a new
  function, a comment the normalization keeps), record the new hash with --update and give the reason in the commit
  message;
- the API differs from the recorded one: the API was incremented; record the new hash with --update.

Usage: qore-module-api-headers.py [--root <source dir>] [--update]
"""
import argparse
import fnmatch
import hashlib
import importlib.util
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

RECORD = "cmake/module-api-headers.txt"

# comments, and the string and character literals that can contain comment markers
_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


def _module_api(root):
    spec = importlib.util.spec_from_file_location("qore_module_api", root / "debian/module-api.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.module_api((root / "include/qore/ModuleManager.h").read_text(encoding="utf-8"))


def install_excludes(cmake_text):
    """Returns the PATTERN ... EXCLUDE patterns of the install(DIRECTORY include/qore ...) rule."""
    rule = re.search(r"install\(DIRECTORY include/qore\b(.*?)\)", cmake_text, re.DOTALL)
    if not rule:
        raise ValueError("CMakeLists.txt has no install(DIRECTORY include/qore ...) rule")
    return re.findall(r"PATTERN\s+(\S+)\s+EXCLUDE", rule.group(1))


def _git_files(root):
    """Returns the files of include/qore that git tracks (relative to include/qore), or None if root is not the top
    of a git checkout (a source tarball) or git is not available."""
    try:
        top = subprocess.run(["git", "-C", str(root), "rev-parse", "--show-toplevel"], capture_output=True,
                             text=True)
        if top.returncode or Path(top.stdout.strip()).resolve() != root.resolve():
            return None
        out = subprocess.run(["git", "-C", str(root), "ls-files", "-z", "--", "include/qore"], capture_output=True,
                             check=True)
    except FileNotFoundError:
        return None
    prefix = "include/qore/"
    return [name[len(prefix):] for name in out.stdout.decode("utf-8", errors="surrogateescape").split("\0")
            if name.startswith(prefix)]


def installed_headers(root):
    """Returns the paths (relative to include/qore) of the installed public headers, sorted.

    In a git checkout, these are the files that git tracks, so that files that are not part of the sources (editor
    and merge leftovers, .DS_Store) do not count; in a source tarball, all files of the directory.
    """
    excludes = install_excludes((root / "CMakeLists.txt").read_text(encoding="utf-8"))
    base = root / "include/qore"
    tracked = _git_files(root)
    if tracked is None:
        candidates = [path.relative_to(base) for path in base.rglob("*")]
    else:
        candidates = [PurePosixPath(name) for name in tracked]
    result = []
    for rel in candidates:
        # a CMake PATTERN matches the name of any file or directory in the tree
        if any(fnmatch.fnmatchcase(part, pattern) for part in rel.parts for pattern in excludes):
            continue
        # a tracked file deleted in the working tree is not installed
        if (base / rel).is_file():
            result.append(rel.as_posix())
    return sorted(result)


_SPACE = re.compile(r"\s+")
_WORD_SPACE = re.compile(r"(?<=\w) (?=\w)")


def _code(text):
    """Removes the whitespace of code, except one space between two word characters ("unsigned int")."""
    text = _SPACE.sub(" ", text)
    text = _WORD_SPACE.sub("\0", text)
    return text.replace(" ", "").replace("\0", " ")


def normalize(text):
    """Removes comments and whitespace; string and character literals are kept as they are.

    Whitespace is kept only as one space where it separates two word characters, so that a change of indentation or
    of the spaces around punctuation does not change the result.
    """
    parts = []
    code = []
    pos = 0
    for m in _TOKEN.finditer(text):
        code.append(text[pos:m.start()])
        token = m.group(0)
        pos = m.end()
        if token[0] == "/":
            # a comment separates tokens like whitespace
            code.append(" ")
            continue
        parts.append(_code("".join(code)))
        code = []
        parts.append(token)
    code.append(text[pos:])
    parts.append(_code("".join(code)))
    return "".join(parts)


def header_hash(root):
    digest = hashlib.sha256()
    for rel in installed_headers(root):
        content = normalize((root / "include/qore" / rel).read_text(encoding="utf-8", errors="surrogateescape"))
        for part in (rel, content):
            data = part.encode("utf-8", errors="surrogateescape")
            digest.update(len(data).to_bytes(8, "big"))
            digest.update(data)
    return digest.hexdigest()


def read_record(root):
    """Returns (api, hash) from the record file."""
    values = {}
    for line in (root / RECORD).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, value = line.partition(" ")
        values[key] = value.strip()
    if set(values) != {"api", "sha256"}:
        raise ValueError(f"{RECORD} must hold exactly the 'api' and 'sha256' lines")
    return values["api"], values["sha256"]


def write_record(root, api, digest):
    (root / RECORD).write_text(
        "# The binary module API (include/qore/ModuleManager.h) and the hash of the installed public headers that it\n"
        "# was recorded for: tools/qore-module-api-headers.py checks them (design/module-api-version.md); after a\n"
        "# decision about a header change, record them with: tools/qore-module-api-headers.py --update\n"
        f"api {api}\nsha256 {digest}\n", encoding="utf-8")


def check(root):
    """Returns an error message, or None if the record matches the headers."""
    api = _module_api(root)
    digest = header_hash(root)
    recorded_api, recorded_hash = read_record(root)
    if recorded_api != api:
        return (f"the module API is {api}, but {RECORD} was recorded for module API {recorded_api}: record the "
                "headers of the new module API with tools/qore-module-api-headers.py --update")
    if recorded_hash != digest:
        return (f"the public headers changed without a change of the module API {api}: increment "
                "QORE_MODULE_API_MINOR in include/qore/ModuleManager.h if a binary module built with the previous "
                "headers would get the change wrong (inline code, a type layout, an exported signature); otherwise "
                "record the headers with tools/qore-module-api-headers.py --update and give the reason in the commit "
                "message (see design/module-api-version.md)")
    return None


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--update", action="store_true", help="record the module API and the hash of the headers")
    args = parser.parse_args(argv[1:])
    if args.update:
        write_record(args.root, _module_api(args.root), header_hash(args.root))
        return 0
    error = check(args.root)
    if error:
        print(f"{argv[0]}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
