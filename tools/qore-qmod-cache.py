#!/usr/bin/env python3
"""qore-qmod-cache: a content-addressed cache for the AOT modules (qmods) that qcc compiles in a Qore build.

Copyright (C) 2026 Qore Technologies, s.r.o.

The design is in design/qmod-build-cache.md; this is a summary.

A build configured with QORE_QMOD_CACHE_DIR runs every "qcc -m" of an in-tree user module through "exec", which
looks the module up in the cache before it runs qcc, and stores what qcc wrote after a compile.  A module is reused
only when every input that can change what qcc writes, or that the module loader checks before it uses a module, is
the same:

- the build context ("context", written once per build before any qmod): the qcc format fingerprint (the AOT runtime
  identity's sources and the build configuration: build type, compiler and its version, compiler flags, system,
  processor, LLVM version), the identity that qcc reports, the content of every libqore and qcc source and of the
  generated headers and compile flags of libqore and qcc (qcc's output depends on the whole parser and runtime, not
  only on the identity's sources), the probe of optional external modules, the AOT link configuration, the link
  driver and linker versions, the operating system and its C library, and this script;
- the qcc command line, its working directory and the environment variables that qcc or libqore read;
- the content of the module's own sources (every file of a module directory);
- every file that the module's dependency file lists (the sources of the modules it requires and the binary modules
  it loads), by content, and for each in-tree module it requires, that module's own cache key, so the key covers its
  dependencies transitively (a Merkle construction);
- the %try-module requests that failed when the module was compiled (read from the module's metadata section): a
  module is reused only while each of them still has no candidate file in the module path, since the loader refuses
  a module compiled without an optional module that is available now (AOT-MODULE-STALE).

The dependency list of a module is only known after it is compiled, so a lookup is in two steps, as in ccache's
direct mode: the inputs known before the compile give the base key, under which each earlier compile left an entry
with the dependencies it found and their content; an entry whose dependencies all still have that content is a hit.

A hit is verified before it is used: the stored files must have their recorded SHA-256 digests, the dependency file
must name the module's path, and the module must carry the AOT runtime identity of the build.  Anything unexpected
makes the module a miss: qcc compiles it, as without the cache.

Commands:
- context: writes the build context (run by the build before any qmod)
- exec: compiles one module through the cache (the build's qmod rule)
- verify: the CI guard; checks every qmod of a build tree that the cache supplied, and the identity of every qmod
- stats: prints the hits, misses and cache size of a build
- prune: removes cache entries not used since a time, then the oldest ones above a size limit

Environment (read by "exec"; never part of a key):
- QORE_QMOD_CACHE_DISABLE=1: compile every module and store nothing
- QORE_QMOD_CACHE_READONLY=1: look modules up but store nothing
- QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]: on a hit, also compile a sample of the modules (a rate from 0 to 1 of
  them, chosen by their key and the seed) and fail the build unless qcc writes the same module byte for byte
"""

import argparse
import fcntl
import hashlib
import json
import os
import platform
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time

#: the cache layout and key format; a new value makes every earlier entry unreachable
SCHEMA = "qore-qmod-cache-1"

#: environment variables that qcc and libqore read (QORE_*, QCC_*) are part of the key, except these, which neither
#: reads (test/qmod-cache tests check that no libqore or qcc source names them) and whose values may differ between
#: builds that compile the same modules, or hold credentials
ENV_EXCLUDED_NAMES = frozenset((
    "QORE_EXCLUDE_PERF_TESTS",
    "QORE_PERF_TESTS_ONLY",
    "QORE_SRC_DIR",
    "QORE_TEST_BUILD_DIR",
    "QORE_TEST_DIRS",
    "QORE_TEST_EXTRA_DIRS",
    "QORE_TEST_OPTS",
    "QORE_THIRD_PARTY_MIRROR",
))
ENV_EXCLUDED_PREFIXES = ("QORE_DB_CONNSTR", "QORE_QMOD_CACHE_")
ENV_INCLUDED_PREFIXES = ("QORE_", "QCC_")
#: other environment variables that can change what qcc writes: the link driver, the character encoding and time zone
#: of parse-time values, the macOS deployment target and SDK, and the libraries qcc runs with
ENV_INCLUDED_NAMES = frozenset((
    "CC",
    "CXX",
    "DYLD_FALLBACK_LIBRARY_PATH",
    "DYLD_INSERT_LIBRARIES",
    "DYLD_LIBRARY_PATH",
    "LANG",
    "LC_ALL",
    "LC_CTYPE",
    "LD_LIBRARY_PATH",
    "LD_PRELOAD",
    "MACOSX_DEPLOYMENT_TARGET",
    "SDKROOT",
    "TZ",
))

#: the files beside a qmod that qcc writes and the cache stores
STORED_SUFFIXES = ("", ".d", ".d.fingerprints")
#: the sidecar the cache writes beside each qmod it compiled or supplied
SIDECAR_SUFFIX = ".qmod-cache.json"
#: the most entries kept under one base key
MAX_ENTRIES_PER_BASE = 8

#: AOT module metadata record (include/qore/intern/QoreAOTBinary.h, QoreAOTModuleFileInfo)
QAMI_MAGIC = b"QAMI"
QAMI_VERSION = 1
QAMI_MAX_PAYLOAD = 16 * 1024 * 1024

FINGERPRINT_HEADER = "# qore AOT module source fingerprints 1"

QORE_SOURCE_SUFFIXES = (".qm", ".qc")

#: a %include directive: qcc does not list the files a module includes in its depfile
INCLUDE_DIRECTIVE = re.compile(rb"^[ \t]*%include\b", re.M)


class CacheError(Exception):
    """An inconsistency that makes a module a miss, or makes "verify" fail"""


def log(msg):
    sys.stderr.write("qmod-cache: %s\n" % msg)
    sys.stderr.flush()


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            block = f.read(1 << 20)
            if not block:
                break
            h.update(block)
    return h.hexdigest()


def digest_items(items):
    """Returns the SHA-256 digest of a list of (name, value) pairs; each value is a string, or a list or dict of them"""
    return sha256_bytes(json.dumps(items, sort_keys=True, separators=(",", ":")).encode("utf-8"))


def file_record(path):
    """The content of a file for a key: its SHA-256 digest, or MISSING, or the target of a symbolic link"""
    try:
        st = os.lstat(path)
    except FileNotFoundError:
        return "MISSING"
    if stat.S_ISLNK(st.st_mode):
        return "LINK:" + os.readlink(path)
    if stat.S_ISDIR(st.st_mode):
        return "DIR:" + tree_digest(path)
    return sha256_file(path)


def tree_digest(root, exclude=None):
    """Returns the digest of a directory tree: the relative path and content of every file in it, in sorted order

    Symbolic links are recorded by their target and not followed; @p exclude is a predicate on a relative path.
    """
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames + [d for d in dirnames if os.path.islink(os.path.join(dirpath, d))]):
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root)
            if exclude and exclude(rel):
                continue
            entries.append([rel, file_record(path)])
        # do not descend into linked directories
        dirnames[:] = [d for d in dirnames if not os.path.islink(os.path.join(dirpath, d))]
    return digest_items(entries)


def atomic_write(path, data):
    """Writes a file by renaming a temporary file in its directory over it"""
    directory = os.path.dirname(path) or "."
    fd, tmp = tempfile.mkstemp(prefix=".qmod-cache.", dir=directory)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def atomic_copy(src, dst, mode=None):
    """Copies a file to a temporary file beside the destination and renames it over the destination"""
    directory = os.path.dirname(dst) or "."
    fd, tmp = tempfile.mkstemp(prefix=".qmod-cache.", dir=directory)
    try:
        with os.fdopen(fd, "wb") as out, open(src, "rb") as inp:
            shutil.copyfileobj(inp, out, 1 << 20)
        if mode is not None:
            os.chmod(tmp, mode)
        os.replace(tmp, dst)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


# ---------------------------------------------------------------------------------------------------------------------
# depfiles, fingerprints and module metadata

def parse_depfile_text(content):
    """Parses a make rule as the module loader does (qore_read_module_depfile() in lib/ModuleManager.cpp)

    @return (target, deps); raises CacheError for a file that is no rule or has no target
    """
    target = ""
    i = 0
    n = len(content)
    while i < n:
        c = content[i]
        if c == "\\" and i + 1 < n:
            nxt = content[i + 1]
            i += 1
            if nxt in "\r\n":
                raise CacheError("the target of the dependency file spans lines")
            target += nxt
        elif c == ":":
            break
        elif c in "\r\n":
            raise CacheError("the dependency file has no rule")
        elif c not in " \t":
            target += c
        elif target:
            j = i
            while j < n and content[j] in " \t":
                j += 1
            if j == n or content[j] != ":":
                raise CacheError("the dependency file has more than one target")
            i = j
            break
        i += 1
    if i >= n or not target:
        raise CacheError("the dependency file has no target")
    deps = []
    dep = ""
    i += 1
    while i < n:
        c = content[i]
        if c == "\\" and i + 1 < n:
            nxt = content[i + 1]
            if nxt in "\r\n":
                c = " "
            else:
                dep += nxt
                i += 2
                continue
        if c in " \t\r\n":
            if dep:
                deps.append(dep)
                dep = ""
        else:
            dep += c
        i += 1
    if dep:
        deps.append(dep)
    return target, deps


def read_depfile(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        return parse_depfile_text(f.read())


def read_fingerprints(path):
    """Reads the source fingerprints qcc wrote beside a depfile: {path: (size, mtime_ns, xxh64)}"""
    rv = {}
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        lines = f.read().split("\n")
    if not lines or lines[0] != FINGERPRINT_HEADER:
        raise CacheError("'%s' is no fingerprint file" % path)
    for line in lines[1:]:
        if not line:
            continue
        parts = line.split(" ", 3)
        if len(parts) != 4 or len(parts[2]) != 16:
            raise CacheError("'%s' has an invalid line" % path)
        rv[parts[3]] = (int(parts[0]), int(parts[1]), parts[2])
    return rv


def parse_qami_record(data, offset):
    """Parses an AOT module metadata record at @p offset; returns (name, dependencies, optional) or None"""
    if offset + 12 > len(data):
        return None
    magic, version, payload_size = struct.unpack_from("<4sII", data, offset)
    if magic != QAMI_MAGIC or version != QAMI_VERSION or payload_size > QAMI_MAX_PAYLOAD:
        return None
    pos = offset + 12
    end = pos + payload_size
    if end > len(data):
        return None

    def read_str(p):
        if p + 4 > end:
            raise ValueError
        (length,) = struct.unpack_from("<I", data, p)
        p += 4
        if not length or p + length > end:
            raise ValueError
        s = data[p:p + length]
        if b"\0" in s:
            raise ValueError
        return s.decode("utf-8"), p + length

    def read_list(p):
        if p + 4 > end:
            raise ValueError
        (count,) = struct.unpack_from("<I", data, p)
        p += 4
        if count > 65536:
            raise ValueError
        items = []
        for _ in range(count):
            s, p = read_str(p)
            items.append(s)
        return items, p

    try:
        name, pos = read_str(pos)
        deps, pos = read_list(pos)
        optional, pos = read_list(pos)
    except (ValueError, UnicodeDecodeError):
        return None
    if pos != end:
        return None
    return name, deps, optional


def read_module_info(path, name):
    """Returns (dependencies, unavailable optional modules) from the metadata record of the AOT module @p name

    The record is found by its magic and must parse completely and name the module; exactly one such record must be
    in the file (an ELF note or a Mach-O section holds it), or CacheError is raised.
    """
    with open(path, "rb") as f:
        data = f.read()
    found = []
    pos = data.find(QAMI_MAGIC)
    while pos >= 0:
        rec = parse_qami_record(data, pos)
        if rec and rec[0] == name:
            found.append(rec)
        pos = data.find(QAMI_MAGIC, pos + 1)
    if len(found) != 1:
        raise CacheError("module '%s' has %d metadata records for '%s'" % (path, len(found), name))
    return found[0][1], found[0][2]


def module_spec_name(spec):
    """Returns the module name of a %try-module request such as "xml >= 1.3" """
    m = re.match(r"\s*([^\s<>=!]+)", spec)
    if not m:
        raise CacheError("cannot parse the module request '%s'" % spec)
    return m.group(1)


def module_search_dirs(env, default_dirs):
    """The directories searched for the candidates of an optional module: those of QORE_MODULE_DIR and the loader's
    standard ones, even with QORE_MODULE_DIR_ONLY (a larger search can only make a module a miss)"""
    dirs = [d for d in env.get("QORE_MODULE_DIR", "").split(":") if d]
    dirs.extend(default_dirs)
    seen = []
    for d in dirs:
        if d not in seen:
            seen.append(d)
    return seen


_listdir_cache = {}


def cached_listdir(path):
    if path not in _listdir_cache:
        try:
            _listdir_cache[path] = sorted(os.listdir(path))
        except OSError:
            _listdir_cache[path] = None
    return _listdir_cache[path]


def module_candidates(name, dirs, content=True):
    """Returns every file in the module path that the loader could load for module @p name, in search order, with its
    content (or, without @p content, the file it resolves to)

    The forms are those of QoreModuleManager::getModuleCandidateFiles() and of user modules: in each directory and in
    a directory of the module's name, the binary module files <name>-api-<x>.<y>.qmod, <name>.qmod and the user module
    source <name>.qm.
    """
    rv = []
    for d in dirs:
        for base in (d, os.path.join(d, name)):
            names = cached_listdir(base)
            if not names:
                continue
            for entry in names:
                if (entry == name + ".qmod" or entry == name + ".qm"
                        or (entry.startswith(name + "-api-") and entry.endswith(".qmod"))):
                    path = os.path.join(base, entry)
                    if os.path.isfile(path):
                        rv.append([path, sha256_file(path) if content else os.path.realpath(path)])
    return rv


# ---------------------------------------------------------------------------------------------------------------------
# the build context

def run_capture(argv, env=None):
    try:
        res = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, timeout=120)
    except (OSError, subprocess.SubprocessError) as e:
        return "ERROR: %s" % e.__class__.__name__
    return res.stdout.decode("utf-8", "replace")


def first_lines(text, count=2):
    return "\n".join(text.strip().splitlines()[:count])


def os_release():
    info = {}
    try:
        with open("/etc/os-release", "r", encoding="utf-8") as f:
            for line in f:
                key, _, value = line.strip().partition("=")
                if key in ("ID", "VERSION_ID"):
                    info[key] = value.strip('"')
    except OSError:
        pass
    if sys.platform == "darwin":
        info["macos"] = platform.mac_ver()[0]
    return info


def libc_info():
    if sys.platform == "darwin":
        return "darwin"
    lib, version = platform.libc_ver()
    if lib:
        return "%s %s" % (lib, version)
    # musl: the dynamic loader prints its version
    for d in ("/lib", "/usr/lib"):
        try:
            names = sorted(os.listdir(d))
        except OSError:
            continue
        for n in names:
            if n.startswith("ld-musl-"):
                return "musl " + first_lines(run_capture([os.path.join(d, n), "--version"]), 2)
    return "unknown"


def link_driver_info(aot_link_conf):
    """The link driver that qcc runs to link a qmod (the "cxx" of aot-link.conf, else c++) and its linker"""
    cxx = "c++"
    try:
        with open(aot_link_conf, "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("cxx="):
                    cxx = line[4:].strip()
    except OSError:
        pass
    path = shutil.which(cxx) or cxx
    info = {"cxx": path, "cxx-version": first_lines(run_capture([path, "--version"]), 1)}
    ld = run_capture([path, "-print-prog-name=ld"]).strip() or "ld"
    ld_path = shutil.which(ld) or ld
    version = run_capture([ld_path, "--version"])
    if "ERROR" in version[:6] or not version.strip() or "unknown option" in version or "not supported" in version:
        version = run_capture([ld_path, "-v"])
    info["ld"] = ld_path
    info["ld-version"] = first_lines(version, 1)
    if sys.platform == "darwin":
        info["sdk"] = first_lines(run_capture(["xcrun", "--show-sdk-version"]), 1)
    return info


def cmake_cache_options(path):
    """The Qore options and build type of a CMake cache (not the cache directory, which is no input)"""
    rv = {}
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"^([A-Za-z0-9_]+):([A-Z]+)=(.*)$", line.rstrip("\n"))
                if not m or m.group(2) in ("INTERNAL", "STATIC"):
                    continue
                name = m.group(1)
                if name.startswith("QORE_QMOD_CACHE"):
                    continue
                if (name.startswith("QORE_") or name in ("SINGLE_COMPILATION_UNIT", "CMAKE_BUILD_TYPE",
                        "CMAKE_CXX_COMPILER", "CMAKE_C_COMPILER", "LLVM_DIR", "CMAKE_OSX_DEPLOYMENT_TARGET",
                        "CMAKE_OSX_SYSROOT", "CMAKE_OSX_ARCHITECTURES")):
                    rv[name] = m.group(3)
    except OSError:
        pass
    return rv


def qcc_identity(version_output):
    m = re.search(r"^AOT runtime identity: ([0-9a-f]{64})\s*$", version_output, re.M)
    return m.group(1) if m else None


def compute_context(source_dir, build_dir, qcc, inputs, env=None):
    """Computes the build context: everything that can change what qcc writes and is the same for every module"""
    env = os.environ if env is None else env
    version = run_capture([qcc, "--version"], env=dict(env))
    identity = qcc_identity(version)
    if not identity:
        raise CacheError("'%s --version' reports no AOT runtime identity" % qcc)
    qcc_lines = [line for line in version.splitlines()
                 if line.startswith(("qcc ", "Using Qore library", "AOT runtime identity"))]

    def exclude_source(rel):
        # editor and VCS files are no input
        base = os.path.basename(rel)
        return base.startswith(".#") or base.endswith("~") or base == ".DS_Store"

    compiler = []
    for rel in ("lib", "include"):
        compiler.append([rel, tree_digest(os.path.join(source_dir, rel), exclude_source)])
    for rel in ("qcc-main.cpp", "CMakeLists.txt", os.path.join("cmake", "third-party-sources.cmake")):
        compiler.append([rel, file_record(os.path.join(source_dir, rel))])
    # the git revision is not in any qmod, and recording it would make every commit a miss
    generated = [["include", tree_digest(os.path.join(build_dir, "include"),
                                         lambda rel: exclude_source(rel) or os.path.basename(rel) == "git-revision.h")]]
    for target in ("libqore", "qcc"):
        for name in ("flags.make", "link.txt"):
            rel = os.path.join("CMakeFiles", target + ".dir", name)
            generated.append([rel, file_record(os.path.join(build_dir, rel))])

    items = {
        "schema": SCHEMA,
        "tool": sha256_file(os.path.abspath(__file__)),
        "qcc": qcc_lines,
        "inputs": [[os.path.basename(p), file_record(p)] for p in inputs],
        "compiler-sources": compiler,
        "generated": generated,
        "cmake-options": cmake_cache_options(os.path.join(build_dir, "CMakeCache.txt")),
        "platform": {
            "sysname": os.uname().sysname,
            "machine": os.uname().machine,
            "os": os_release(),
            "libc": libc_info(),
        },
        "link": link_driver_info(os.path.join(build_dir, "aot-link.conf")),
    }
    return {"schema": SCHEMA, "digest": digest_items(items), "identity": identity, "items": items}


def cmd_context(args):
    ctx = compute_context(os.path.abspath(args.source_dir), os.path.abspath(args.build_dir), args.qcc,
                          [os.path.abspath(p) for p in args.input])
    data = (json.dumps(ctx, sort_keys=True, indent=1) + "\n").encode("utf-8")
    try:
        with open(args.output, "rb") as f:
            if f.read() == data:
                return 0
    except OSError:
        pass
    atomic_write(args.output, data)
    return 0


# ---------------------------------------------------------------------------------------------------------------------
# keys

def key_environment(env):
    """The environment variables that are part of a key, as a sorted list of [name, value]"""
    rv = []
    for name in sorted(env):
        if name in ENV_EXCLUDED_NAMES or name.startswith(ENV_EXCLUDED_PREFIXES):
            continue
        if name.startswith(ENV_INCLUDED_PREFIXES) or name in ENV_INCLUDED_NAMES:
            rv.append([name, env[name]])
    return rv


def is_module_artifact(path):
    return path.endswith(".qmod")


def own_sources_digest(source_root, extra_sources):
    """The content of a module's own sources: a module directory as a whole (but not the qmods linked into it), or a
    single-file module, and the sources the build lists for it"""
    if os.path.isdir(source_root):
        own = tree_digest(source_root, lambda rel: is_module_artifact(rel) or ".qmod." in os.path.basename(rel))
    else:
        own = file_record(source_root)
    extra = [[p, file_record(p)] for p in sorted(set(extra_sources))]
    return digest_items([["root", source_root, own], ["sources", extra]])


def base_key(context_digest, argv, cwd, env, source_root, extra_sources):
    """The key of the inputs known before a module is compiled"""
    return digest_items([
        ["schema", SCHEMA],
        ["context", context_digest],
        ["argv", list(argv)],
        ["cwd", cwd],
        ["env", key_environment(env)],
        ["own", own_sources_digest(source_root, extra_sources)],
    ])


def result_key(base, deps, optional, resolution):
    """The key of a compiled module: its base key, the dependencies and failed optional modules found by qcc, and the
    candidate files of the modules it requires"""
    return digest_items([["base", base], ["deps", deps], ["optional", optional], ["resolution", resolution]])


# ---------------------------------------------------------------------------------------------------------------------
# the build tree's module index: the key of every module the cache compiled or supplied, by source root

def index_dir(build_dir):
    return os.path.join(build_dir, "qmod-cache-index")


def index_path(build_dir, source_root):
    return os.path.join(index_dir(build_dir), sha256_bytes(os.path.realpath(source_root).encode("utf-8")) + ".json")


def write_index(build_dir, source_root, qmod, key):
    st = os.stat(qmod)
    rec = {"source_root": os.path.realpath(source_root), "qmod": qmod, "key": key,
           "qmod_size": st.st_size, "qmod_mtime_ns": st.st_mtime_ns, "qmod_ino": st.st_ino}
    os.makedirs(index_dir(build_dir), exist_ok=True)
    atomic_write(index_path(build_dir, source_root), json.dumps(rec, sort_keys=True).encode("utf-8"))


def remove_index(build_dir, source_root):
    try:
        os.unlink(index_path(build_dir, source_root))
    except OSError:
        pass


def lookup_module_key(build_dir, dep_path, own_root):
    """Returns (source root, key) of the in-tree module that a dependency source belongs to, or None

    The module is the nearest directory above the source, or the source itself, that the index names; its key is
    used only while its qmod is the one the index recorded.
    """
    path = os.path.realpath(dep_path)
    candidates = [path]
    parent = os.path.dirname(path)
    while parent and parent != os.path.dirname(parent):
        candidates.append(parent)
        parent = os.path.dirname(parent)
    for root in candidates:
        if own_root and root == own_root:
            return None
        ip = index_path(build_dir, root)
        try:
            with open(ip, "r", encoding="utf-8") as f:
                rec = json.load(f)
        except (OSError, ValueError):
            continue
        try:
            st = os.stat(rec["qmod"])
        except OSError:
            return None
        if (st.st_size, st.st_mtime_ns, st.st_ino) != (rec["qmod_size"], rec["qmod_mtime_ns"], rec["qmod_ino"]):
            return None
        return root, rec["key"]
    return None


def own_source_files(source_root):
    """The Qore sources of a module: the files of its directory, or its .qm file"""
    if not os.path.isdir(source_root):
        return [source_root]
    rv = []
    for dirpath, _, names in os.walk(source_root):
        rv += [os.path.join(dirpath, n) for n in sorted(names) if n.endswith((".qm", ".qc", ".ql", ".q"))]
    return rv


def check_no_include(source_root):
    """Raises CacheError for a module whose sources include other files: qcc's depfile does not list them, so their
    content could not be part of the key"""
    for path in own_source_files(source_root):
        with open(path, "rb") as f:
            if INCLUDE_DIRECTIVE.search(f.read()):
                raise CacheError("source '%s' has a %%include directive, and qcc does not list included files"
                                 % path)


def classify_dependencies(build_dir, source_root, deps):
    """Returns the dependency records of a module from its depfile's dependencies

    - a source of the module itself: its content ("own")
    - a source of another in-tree module compiled or supplied by the cache: its content and that module's key
      ("module"), which covers the module's own dependencies
    - any other file (a binary module, an include file): its content ("file")

    CacheError is raised for a dependency that the key cannot cover: a missing file, or a Qore module source or AOT
    module whose own dependencies are not known (a module from outside the build tree, or one not built through the
    cache).
    """
    own_root = os.path.realpath(source_root)
    own_is_dir = os.path.isdir(source_root)
    records = []
    for dep in deps:
        real = os.path.realpath(dep)
        if not os.path.isfile(real):
            raise CacheError("dependency '%s' is not a file" % dep)
        content = sha256_file(real)
        if real == own_root or (own_is_dir and real.startswith(own_root + os.sep)):
            records.append({"path": dep, "kind": "own", "sha256": content})
            continue
        mod = lookup_module_key(build_dir, real, own_root)
        if mod:
            records.append({"path": dep, "kind": "module", "sha256": content, "module_root": mod[0],
                            "module_key": mod[1]})
            continue
        if real.endswith(QORE_SOURCE_SUFFIXES):
            raise CacheError("dependency '%s' is a module source that is not built through the cache" % dep)
        if is_module_artifact(real):
            with open(real, "rb") as f:
                head = f.read()
            if QAMI_MAGIC in head:
                raise CacheError("dependency '%s' is an AOT module that is not built through the cache" % dep)
        records.append({"path": dep, "kind": "file", "sha256": content})
    return records


def check_dependencies(build_dir, source_root, records):
    """Returns None if every dependency record still holds, else the first one that does not, as a reason"""
    own_root = os.path.realpath(source_root)
    for rec in records:
        real = os.path.realpath(rec["path"])
        if not os.path.isfile(real):
            return "dependency '%s' is missing" % rec["path"]
        if sha256_file(real) != rec["sha256"]:
            return "dependency '%s' has changed" % rec["path"]
        if rec["kind"] == "module":
            mod = lookup_module_key(build_dir, real, own_root)
            if not mod or mod[1] != rec["module_key"]:
                return "module '%s' has changed" % rec["module_root"]
    return None


def check_optional(records, dirs):
    for rec in records:
        if module_candidates(rec["name"], dirs) != rec["candidates"]:
            return "optional module '%s' has another candidate file now" % rec["spec"]
    return None


def resolution_records(names, dirs):
    """The candidate files in the module path of each module a module requires (its metadata's dependencies), by
    path: a file that appears for one of them, as one in a directory searched earlier, would make qcc load another
    module (the content of the files it loaded is in the dependency records)"""
    rv = []
    for spec in names:
        name = module_spec_name(spec)
        rv.append({"name": name, "candidates": module_candidates(name, dirs, content=False)})
    return rv


def check_resolution(records, dirs):
    for rec in records:
        if module_candidates(rec["name"], dirs, content=False) != rec["candidates"]:
            return "module '%s' has other candidate files now" % rec["name"]
    return None


def contains_identity(path, identity):
    with open(path, "rb") as f:
        return identity.encode("ascii") in f.read()


# ---------------------------------------------------------------------------------------------------------------------
# the cache directory

class Cache:
    """The cache directory: <dir>/<SCHEMA>/entries/<base key>/<result key>.json and objects/<result key>/

    Several jobs can share the directory (a host directory mounted into each job).  Each object has a lock file,
    objects/<result key>.lock:
    - a job that reads or stores an entry holds a shared lock on it while it does;
    - an entry is removed only under an exclusive lock taken without waiting: an entry that a job is reading is
      skipped, never removed; the entry and its object are renamed out of the cache, into trash/, before they are
      deleted, so a reader never sees a partial one;
    - the lock file's modification time is the entry's last use: set when it is stored and on every hit (when the
      cache is writable; a hit from a cache that cannot be written still succeeds), and the least recently used
      entries are pruned first.
    Only one job prunes at a time (prune.lock, taken without waiting: a job finding another job pruning skips its
    own prune).
    """

    def __init__(self, directory):
        self.root = os.path.join(os.path.abspath(directory), SCHEMA)
        self.entries = os.path.join(self.root, "entries")
        self.objects = os.path.join(self.root, "objects")
        self.trash = os.path.join(self.root, "trash")

    def candidates(self, base):
        """Returns the entries stored under a base key, most recently stored first"""
        d = os.path.join(self.entries, base)
        try:
            names = [n for n in os.listdir(d) if n.endswith(".json")]
        except OSError:
            return []
        rv = []
        for n in names:
            p = os.path.join(d, n)
            try:
                with open(p, "r", encoding="utf-8") as f:
                    entry = json.load(f)
                rv.append((os.stat(p).st_mtime, p, entry))
            except (OSError, ValueError):
                continue
        rv.sort(key=lambda e: e[0], reverse=True)
        return [(p, e) for _, p, e in rv]

    def object_dir(self, key):
        return os.path.join(self.objects, key)

    def lock_path(self, key):
        return os.path.join(self.objects, key + ".lock")

    def open_lock(self, key, create=True):
        """Opens the lock file of an entry; returns None if it cannot be opened (a cache that cannot be written)"""
        path = self.lock_path(key)
        if create:
            try:
                os.makedirs(self.objects, exist_ok=True)
                return os.open(path, os.O_RDWR | os.O_CREAT, 0o666)
            except OSError:
                pass
        try:
            return os.open(path, os.O_RDONLY)
        except OSError:
            return None

    def mark_used(self, fd):
        """Sets the last use of the entry whose lock file is open on @p fd to now; best effort"""
        if fd is None:
            return
        try:
            os.utime(fd)
        except (OSError, NotImplementedError):
            pass

    def remove(self, entry_path, key):
        """Removes an entry and its object unless a job is reading it; returns False if it is in use"""
        fd = self.open_lock(key)
        if fd is None:
            return False
        try:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError:
                return False
            os.makedirs(self.trash, exist_ok=True)
            tag = "%s.%d.%d" % (key, os.getpid(), int(time.time() * 1e6))
            moved = []
            for src, dst in ((entry_path, os.path.join(self.trash, tag + ".json")),
                             (self.object_dir(key), os.path.join(self.trash, tag))):
                try:
                    os.rename(src, dst)
                    moved.append(dst)
                except FileNotFoundError:
                    pass
            # while the lock is held: a reader that opened the file before waits for it and then finds no object
            try:
                os.unlink(self.lock_path(key))
            except OSError:
                pass
        finally:
            os.close(fd)
        for p in moved:
            if os.path.isdir(p):
                shutil.rmtree(p, ignore_errors=True)
            else:
                try:
                    os.unlink(p)
                except OSError:
                    pass
        return True

    def store(self, base, key, entry, files):
        """Stores the files of a compiled module; another build that stores the same key first wins"""
        fd = self.open_lock(key)
        if fd is None:
            raise OSError("cannot create the lock file of entry %s" % key)
        try:
            # shared: a store does not conflict with a reader, but an entry being stored is not pruned
            fcntl.flock(fd, fcntl.LOCK_SH)
            obj = self.object_dir(key)
            if not os.path.isdir(obj):
                tmp = tempfile.mkdtemp(prefix=".tmp.", dir=self.objects)
                try:
                    for suffix, src in files.items():
                        shutil.copyfile(src, os.path.join(tmp, "qmod" + suffix))
                    try:
                        os.rename(tmp, obj)
                    except OSError:
                        if not os.path.isdir(obj):
                            raise
                finally:
                    shutil.rmtree(tmp, ignore_errors=True)
            d = os.path.join(self.entries, base)
            os.makedirs(d, exist_ok=True)
            atomic_write(os.path.join(d, key + ".json"),
                         (json.dumps(entry, sort_keys=True, indent=1) + "\n").encode())
            self.mark_used(fd)
        finally:
            os.close(fd)
        # keep the most recently stored entries of the base key
        cands = self.candidates(base)
        for p, e in cands[MAX_ENTRIES_PER_BASE:]:
            self.remove(p, e.get("key", ""))


# ---------------------------------------------------------------------------------------------------------------------
# exec

def parse_qcc_argv(argv):
    out = depfile = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o" and i + 1 < len(argv):
            out = argv[i + 1]
            i += 1
        elif a.startswith("--depfile="):
            depfile = a.split("=", 1)[1]
        i += 1
    if not out or not depfile:
        raise CacheError("the qcc command has no -o or --depfile")
    return os.path.abspath(out), os.path.abspath(depfile)


def audit_selected(key, setting):
    """Returns True if a hit is in the audit sample: QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]"""
    if not setting:
        return False
    rate, _, seed = setting.partition(":")
    try:
        rate = float(rate)
    except ValueError:
        raise CacheError("QORE_QMOD_CACHE_AUDIT must be <rate>[:<seed>], not '%s'" % setting)
    if rate <= 0:
        return False
    if rate >= 1:
        return True
    value = int(sha256_bytes((seed + ":" + key).encode("utf-8"))[:8], 16) / float(0xffffffff)
    return value < rate


class Module:
    """One module compile: the qcc command and everything the cache knows about it"""

    def __init__(self, args, env):
        self.argv = list(args.qcc_argv)
        self.env = env
        self.cwd = os.getcwd()
        self.name = args.name
        self.source_root = os.path.abspath(args.source_root)
        self.build_dir = os.path.abspath(args.build_dir)
        self.extra = [os.path.abspath(p) for p in args.source]
        self.out, self.depfile = parse_qcc_argv(self.argv)
        if os.path.basename(self.out) != self.name + ".qmod":
            raise CacheError("the qcc output '%s' is not module '%s'" % (self.out, self.name))
        with open(args.context, "r", encoding="utf-8") as f:
            self.context = json.load(f)
        if self.context.get("schema") != SCHEMA:
            raise CacheError("the build context '%s' has another schema" % args.context)
        self.identity = self.context["identity"]
        self.default_dirs = args.default_module_dir
        # a cache that this job does not write: no store, no last-use mark, no removal
        self.readonly = env.get("QORE_QMOD_CACHE_READONLY") in ("1", "true", "yes")
        self.base = base_key(self.context["digest"], self.argv, self.cwd, env, self.source_root, self.extra)

    def files(self):
        return {"": self.out, ".d": self.out + ".d", ".d.fingerprints": self.out + ".d.fingerprints"}

    def sidecar(self, origin, key, entry=None):
        rec = {"schema": SCHEMA, "name": self.name, "origin": origin, "base_key": self.base, "key": key,
               "context": self.context["digest"], "identity": self.identity}
        if os.path.isfile(self.out):
            rec["qmod_sha256"] = sha256_file(self.out)
        if entry is not None:
            rec["entry"] = entry
        atomic_write(self.out + SIDECAR_SUFFIX, (json.dumps(rec, sort_keys=True, indent=1) + "\n").encode())

    def search_dirs(self):
        return module_search_dirs(self.env, self.default_dirs)

    def try_hit(self, cache):
        """Restores the first entry whose dependencies still hold; returns (entry, reason for the last miss)"""
        cands = cache.candidates(self.base)
        if not cands:
            return None, "no entry"
        reason = None
        for path, entry in cands:
            key = entry.get("key")
            if entry.get("schema") != SCHEMA or entry.get("base_key") != self.base or not key:
                reason = "invalid entry"
                continue
            reason = check_dependencies(self.build_dir, self.source_root, entry["deps"]) \
                or check_optional(entry["optional"], self.search_dirs()) \
                or check_resolution(entry["resolution"], self.search_dirs())
            if reason:
                continue
            if result_key(self.base, entry["deps"], entry["optional"], entry["resolution"]) != key:
                reason = "entry key mismatch"
                if not self.readonly:
                    cache.remove(path, key)
                continue
            obj = cache.object_dir(key)
            # held while the entry is read, so that no job prunes it meanwhile
            fd = cache.open_lock(key, create=not self.readonly)
            damaged = None
            try:
                if fd is not None:
                    fcntl.flock(fd, fcntl.LOCK_SH)
                try:
                    if not os.path.isdir(obj):
                        # removed since it was listed
                        raise FileNotFoundError(obj)
                    for suffix in STORED_SUFFIXES:
                        if sha256_file(os.path.join(obj, "qmod" + suffix)) != entry["files"][suffix or "qmod"]:
                            raise CacheError("stored file 'qmod%s' is damaged" % suffix)
                    if not contains_identity(os.path.join(obj, "qmod"), self.identity):
                        raise CacheError("stored module lacks the AOT runtime identity %s" % self.identity)
                    target, _ = read_depfile(os.path.join(obj, "qmod.d"))
                    if os.path.abspath(target) != self.out:
                        raise CacheError("stored dependency file names '%s'" % target)
                except FileNotFoundError:
                    reason = "entry removed"
                    continue
                except (OSError, CacheError) as e:
                    reason = damaged = str(e)
                    continue
                # the dependency file and fingerprints first, the module last, each renamed into place, the module
                # with the mode qcc gave it
                for suffix in (".d", ".d.fingerprints", ""):
                    atomic_copy(os.path.join(obj, "qmod" + suffix), self.out + suffix,
                                None if suffix else int(entry["mode"]) & 0o777)
                for suffix, digest in (("", entry["files"]["qmod"]), (".d", entry["files"][".d"])):
                    if sha256_file(self.out + suffix) != digest:
                        raise CacheError("restored file '%s' differs from the cache" % (self.out + suffix))
                if not self.readonly:
                    cache.mark_used(fd)
                return entry, None
            finally:
                if fd is not None:
                    os.close(fd)
                # a damaged entry is removed once it is no longer read
                if damaged and not self.readonly:
                    cache.remove(path, key)
        return None, reason

    def run_qcc(self, argv=None):
        return subprocess.run(argv or self.argv, env=self.env).returncode

    def describe(self):
        """Returns (key, entry) of the module qcc has just written, or raises CacheError if it cannot be stored"""
        files = self.files()
        for p in files.values():
            if not os.path.isfile(p):
                raise CacheError("qcc did not write '%s'" % p)
        if not contains_identity(self.out, self.identity):
            raise CacheError("the module lacks the build's AOT runtime identity %s" % self.identity)
        target, deps = read_depfile(self.depfile)
        if os.path.abspath(target) != self.out:
            raise CacheError("the dependency file names '%s'" % target)
        fingerprints = read_fingerprints(self.out + ".d.fingerprints")
        for dep in deps:
            if dep not in fingerprints:
                raise CacheError("qcc recorded no fingerprint for dependency '%s'" % dep)
        # each source must still be as qcc recorded it: a source written during the compile is no stable input
        for path, (size, mtime_ns, _) in fingerprints.items():
            try:
                st = os.stat(path)
            except OSError:
                raise CacheError("source '%s' has gone" % path)
            if st.st_size != size or st.st_mtime_ns != mtime_ns:
                raise CacheError("source '%s' changed while the module was compiled" % path)
        check_no_include(self.source_root)
        records = classify_dependencies(self.build_dir, self.source_root, deps)
        required, optional_specs = read_module_info(self.out, self.name)
        # the compile may have written files in the module path since a lookup listed it
        _listdir_cache.clear()
        dirs = self.search_dirs()
        optional = []
        for spec in optional_specs:
            name = module_spec_name(spec)
            optional.append({"spec": spec, "name": name, "candidates": module_candidates(name, dirs)})
        resolution = resolution_records(required, dirs)
        key = result_key(self.base, records, optional, resolution)
        entry = {
            "schema": SCHEMA,
            "name": self.name,
            "base_key": self.base,
            "key": key,
            "deps": records,
            "optional": optional,
            "resolution": resolution,
            "env_names": [n for n, _ in key_environment(self.env)],
            "mode": os.stat(self.out).st_mode & 0o777,
            "files": {
                "qmod": sha256_file(self.out),
                ".d": sha256_file(self.out + ".d"),
                ".d.fingerprints": sha256_file(self.out + ".d.fingerprints"),
            },
            "created": time.time(),
        }
        return key, entry

    def audit(self, entry):
        """Compiles the module again beside the restored one and compares the two; raises CacheError on a mismatch"""
        # outside of every module directory, so no other compile can find the module of the audit
        audit_dir = os.path.join(self.build_dir, "qmod-cache-audit")
        os.makedirs(audit_dir, exist_ok=True)
        tmpdir = tempfile.mkdtemp(prefix=self.name + ".", dir=audit_dir)
        try:
            out = os.path.join(tmpdir, self.name + ".qmod")
            argv = []
            i = 0
            while i < len(self.argv):
                a = self.argv[i]
                if a == "-o":
                    argv += ["-o", out]
                    i += 2
                    continue
                if a.startswith("--depfile="):
                    a = "--depfile=" + out + ".d"
                argv.append(a)
                i += 1
            rc = self.run_qcc(argv)
            if rc:
                raise CacheError("the audit compile of '%s' failed with exit code %d" % (self.name, rc))
            if sha256_file(out) != entry["files"]["qmod"]:
                raise CacheError("the audit compile of '%s' differs from the module in the cache: an input of qcc "
                                 "is missing from the cache key, or qcc's output is not reproducible" % self.name)
            _, deps = read_depfile(out + ".d")
            if deps != [rec["path"] for rec in entry["deps"]]:
                raise CacheError("the audit compile of '%s' has other dependencies than the cache entry" % self.name)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


def cmd_exec(args):
    env = dict(os.environ)
    if not args.qcc_argv:
        log("no qcc command")
        return 2
    disabled = env.get("QORE_QMOD_CACHE_DISABLE") in ("1", "true", "yes")
    readonly = env.get("QORE_QMOD_CACHE_READONLY") in ("1", "true", "yes")
    try:
        mod = Module(args, env)
    except (OSError, ValueError, KeyError, CacheError) as e:
        # without a usable context the module is compiled as without the cache
        log("%s: not cached: %s" % (args.name, e))
        return subprocess.run(args.qcc_argv, env=env).returncode
    cache = Cache(args.cache_dir)
    reason = "cache disabled"
    if not disabled:
        try:
            entry, reason = mod.try_hit(cache)
        except (OSError, ValueError, KeyError, CacheError) as e:
            entry, reason = None, "lookup failed: %s" % e
        if entry:
            try:
                if audit_selected(entry["key"], env.get("QORE_QMOD_CACHE_AUDIT", "")):
                    mod.audit(entry)
                    log("hit %s (audited: qcc wrote the same module)" % mod.name)
                else:
                    log("hit %s" % mod.name)
            except CacheError as e:
                log("AUDIT FAILED: %s" % e)
                return 1
            mod.sidecar("hit", entry["key"], entry)
            write_index(mod.build_dir, mod.source_root, mod.out, entry["key"])
            return 0
    # a miss: the module's earlier key no longer describes what is in the build tree
    remove_index(mod.build_dir, mod.source_root)
    rc = mod.run_qcc()
    if rc:
        return rc
    try:
        key, entry = mod.describe()
    except (OSError, ValueError, KeyError, CacheError) as e:
        log("miss %s (%s); not stored: %s" % (mod.name, reason, e))
        try:
            mod.sidecar("uncacheable", None)
        except OSError:
            pass
        return 0
    stored = ""
    if not disabled and not readonly:
        try:
            cache.store(mod.base, key, entry, mod.files())
            stored = "; stored"
        except OSError as e:
            stored = "; not stored: %s" % e
    log("miss %s (%s)%s" % (mod.name, reason, stored))
    mod.sidecar("miss", key, entry)
    write_index(mod.build_dir, mod.source_root, mod.out, key)
    return 0


# ---------------------------------------------------------------------------------------------------------------------
# verify, stats, prune

def find_sidecars(build_dir):
    rv = []
    for dirpath, _, filenames in os.walk(build_dir):
        for n in filenames:
            if n.endswith(".qmod" + SIDECAR_SUFFIX):
                rv.append(os.path.join(dirpath, n))
    return sorted(rv)


def build_identity(build_dir, qcc):
    """Returns the AOT runtime identity of a build: the identity libqore was compiled with (the generated header of a
    Qore build tree), the one qcc reports and the one of the build context must all be the same; at least one of them
    must be there"""
    found = []
    header = os.path.join(build_dir, "include", "qore", "intern", "qore_aot_runtime_identity.h")
    if os.path.isfile(header):
        with open(header, "r", encoding="utf-8") as f:
            m = re.search(r'#define QORE_AOT_RUNTIME_IDENTITY "([0-9a-f]{64})"', f.read())
        if not m:
            raise CacheError("'%s' defines no AOT runtime identity" % header)
        found.append(("libqore", m.group(1)))
    if qcc:
        found.append(("qcc", qcc_identity(run_capture([qcc, "--version"]))))
    context = os.path.join(build_dir, "qmod-cache-context.json")
    if os.path.isfile(context):
        with open(context, "r", encoding="utf-8") as f:
            found.append(("the build context", json.load(f).get("identity")))
    if not found:
        raise CacheError("the AOT runtime identity of the build is unknown")
    if len(set(i for _, i in found)) != 1 or not found[0][1]:
        raise CacheError("the AOT runtime identities of the build differ: %s"
                         % ", ".join("%s %s" % (w, i) for w, i in found))
    return found[0][1]


def cmd_verify(args):
    """The CI guard: every qmod the cache supplied must still be what the cache verified, and every qmod of the build
    must carry the build's AOT runtime identity"""
    build_dir = os.path.abspath(args.build_dir)
    try:
        identity = build_identity(build_dir, args.qcc)
    except (OSError, ValueError, CacheError) as e:
        log("FAIL: %s" % e)
        return 1
    failures = []
    counts = {"hit": 0, "miss": 0, "uncacheable": 0}
    sidecars = find_sidecars(build_dir)
    for sc in sidecars:
        qmod = sc[:-len(SIDECAR_SUFFIX)]
        try:
            with open(sc, "r", encoding="utf-8") as f:
                rec = json.load(f)
            counts[rec["origin"]] = counts.get(rec["origin"], 0) + 1
            if not os.path.isfile(qmod):
                raise CacheError("the module is missing")
            if rec.get("identity") != identity:
                raise CacheError("it was built for the AOT runtime identity %s" % rec.get("identity"))
            if not contains_identity(qmod, identity):
                raise CacheError("it does not carry the build's AOT runtime identity %s" % identity)
            if rec["origin"] != "hit":
                continue
            entry = rec["entry"]
            if sha256_file(qmod) != entry["files"]["qmod"]:
                raise CacheError("it is not the module the cache supplied")
            target, deps = read_depfile(qmod + ".d")
            if os.path.abspath(target) != os.path.abspath(qmod):
                raise CacheError("its dependency file names '%s'" % target)
            if deps != [d["path"] for d in entry["deps"]]:
                raise CacheError("its dependency file lists other dependencies than the cache entry")
            for d in entry["deps"]:
                if sha256_file(os.path.realpath(d["path"])) != d["sha256"]:
                    raise CacheError("dependency '%s' has changed since the module was supplied" % d["path"])
            required, optional = read_module_info(qmod, rec["name"])
            if optional != [o["spec"] for o in entry["optional"]]:
                raise CacheError("its unavailable optional modules differ from the cache entry")
            if [module_spec_name(r) for r in required] != [r["name"] for r in entry["resolution"]]:
                raise CacheError("the modules it requires differ from the cache entry")
        except (OSError, ValueError, KeyError, CacheError) as e:
            failures.append("%s: %s" % (qmod, e))
    # a qmod of the build that the cache did not see is checked for the identity too
    seen = set(sc[:-len(SIDECAR_SUFFIX)] for sc in sidecars)
    qmod_dir = os.path.join(build_dir, "qlib-qmod")
    for dirpath, _, filenames in os.walk(qmod_dir):
        for n in filenames:
            p = os.path.join(dirpath, n)
            if n.endswith(".qmod") and p not in seen and not os.path.islink(p):
                if not contains_identity(p, identity):
                    failures.append("%s: it does not carry the build's AOT runtime identity %s" % (p, identity))
    if failures:
        for f in failures:
            log("FAIL: %s" % f)
        log("%d of %d modules failed verification" % (len(failures), len(sidecars)))
        return 1
    log("verified %d modules: %d supplied by the cache, %d compiled, %d compiled and not cacheable; AOT runtime "
        "identity %s" % (len(sidecars), counts["hit"], counts["miss"], counts["uncacheable"], identity))
    if args.require_sidecars and not sidecars:
        log("FAIL: no module was built through the cache")
        return 1
    return 0


def dir_size(path):
    total = 0
    for dirpath, _, filenames in os.walk(path):
        for n in filenames:
            try:
                total += os.lstat(os.path.join(dirpath, n)).st_size
            except OSError:
                pass
    return total


def build_counts_and_keys(build_dir):
    """Returns ({origin: count}, set of keys) of the modules of a build tree that the cache saw"""
    counts = {}
    keys = set()
    for sc in find_sidecars(os.path.abspath(build_dir)):
        try:
            with open(sc, "r", encoding="utf-8") as f:
                rec = json.load(f)
            origin = rec["origin"]
            if rec.get("key"):
                keys.add(rec["key"])
        except (OSError, ValueError, KeyError):
            origin = "unreadable"
        counts[origin] = counts.get(origin, 0) + 1
    return counts, keys


def cmd_stats(args):
    counts = {}
    if args.build_dir:
        counts, _ = build_counts_and_keys(args.build_dir)
    cache = Cache(args.cache_dir)
    objects = 0
    try:
        objects = len([n for n in os.listdir(cache.objects) if not n.startswith(".")])
    except OSError:
        pass
    size = dir_size(cache.root)
    print("qmod-cache: modules: %s; cache: %d objects, %.1f MiB in %s" % (
        ", ".join("%s %d" % (k, v) for k, v in sorted(counts.items())) or "none", objects, size / 1048576.0,
        cache.root))
    return 0


#: how old an orphan (an object without an entry, an entry without an object, an interrupted store or removal) must
#: be before a prune removes it: a younger one can belong to a store in progress in another job
ORPHAN_AGE = 3600


def cache_entries(cache):
    """Returns [last use, entry path, key, size] of every entry of the cache"""
    rv = []
    try:
        bases = os.listdir(cache.entries)
    except OSError:
        return rv
    for base in bases:
        d = os.path.join(cache.entries, base)
        try:
            names = os.listdir(d)
        except OSError:
            continue
        for n in names:
            if not n.endswith(".json"):
                continue
            p = os.path.join(d, n)
            key = n[:-5]
            try:
                used = os.stat(p).st_mtime
            except OSError:
                continue
            try:
                used = max(used, os.stat(cache.lock_path(key)).st_mtime)
            except OSError:
                pass
            size = dir_size(cache.object_dir(key))
            try:
                size += os.stat(p).st_size
            except OSError:
                pass
            rv.append([used, p, key, size])
    return rv


def remove_orphans(cache, now):
    """Removes what no entry references and that is older than ORPHAN_AGE"""
    def old(path):
        try:
            return now - os.lstat(path).st_mtime > ORPHAN_AGE
        except OSError:
            return False
    keys = set(k for _, _, k, _ in cache_entries(cache))
    removed = 0
    try:
        names = os.listdir(cache.objects)
    except OSError:
        names = []
    for n in names:
        p = os.path.join(cache.objects, n)
        key = n[:-5] if n.endswith(".lock") else n
        if key in keys or not old(p):
            continue
        if n.startswith(".tmp.") or not n.endswith(".lock"):
            # an interrupted store, or an object without an entry: taken out under its lock like an entry
            if n.startswith(".tmp."):
                shutil.rmtree(p, ignore_errors=True)
                removed += 1
            elif cache.remove(os.path.join(cache.entries, "-", key + ".json"), key):
                removed += 1
        elif not os.path.isdir(cache.object_dir(key)):
            # a lock file without an object
            fd = cache.open_lock(key, create=False)
            if fd is not None:
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    if not os.path.isdir(cache.object_dir(key)):
                        os.unlink(p)
                except OSError:
                    pass
                finally:
                    os.close(fd)
    # entries without an object
    for used, p, key, _ in cache_entries(cache):
        if not os.path.isdir(cache.object_dir(key)) and now - used > ORPHAN_AGE:
            if cache.remove(p, key):
                removed += 1
    # the leftovers of interrupted removals
    try:
        for n in os.listdir(cache.trash):
            p = os.path.join(cache.trash, n)
            if old(p):
                if os.path.isdir(p):
                    shutil.rmtree(p, ignore_errors=True)
                else:
                    os.unlink(p)
    except OSError:
        pass
    return removed


def cmd_prune(args):
    """Prunes the cache and prints one line with the hits and misses of a build and the size of the cache before and
    after: entries not used for --max-age-days are removed, then, with --keep-build, those that no module of the build
    tree came from or was stored as, then the least recently used ones while the cache is larger than --max-size MiB;
    an entry that a job is reading is never removed"""
    cache = Cache(args.cache_dir)
    now = time.time()
    counts = {}
    keep_keys = None
    if args.build_dir:
        counts, _ = build_counts_and_keys(args.build_dir)
    if args.keep_build:
        _, keep_keys = build_counts_and_keys(args.keep_build)
    before = dir_size(cache.root)
    stats = {"age": 0, "build": 0, "size": 0, "orphans": 0, "in use": 0}
    note = ""
    os.makedirs(cache.root, exist_ok=True)
    lock_fd = os.open(os.path.join(cache.root, "prune.lock"), os.O_RDWR | os.O_CREAT, 0o666)
    try:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            note = "; another job is pruning the cache"
        if not note:
            stats["orphans"] = remove_orphans(cache, now)
            entries = cache_entries(cache)
            keep = []
            # the entries that a job is reading, which are left for a later prune
            busy = set()
            for e in entries:
                used, p, key, _ = e
                if args.max_age_days is not None and now - used > args.max_age_days * 86400:
                    reason = "age"
                elif keep_keys is not None and key not in keep_keys:
                    reason = "build"
                else:
                    keep.append(e)
                    continue
                if cache.remove(p, key):
                    stats[reason] += 1
                else:
                    busy.add(key)
            if args.max_size is not None:
                limit = args.max_size * 1048576
                size = dir_size(cache.root)
                # the least recently used first
                keep.sort(key=lambda e: e[0])
                for used, p, key, esize in keep:
                    if size <= limit:
                        break
                    if cache.remove(p, key):
                        stats["size"] += 1
                        size -= esize
                    else:
                        busy.add(key)
            stats["in use"] = len(busy)
            for base in os.listdir(cache.entries) if os.path.isdir(cache.entries) else []:
                try:
                    os.rmdir(os.path.join(cache.entries, base))
                except OSError:
                    pass
    finally:
        os.close(lock_fd)
    after = dir_size(cache.root)
    removed = ", ".join("%d %s" % (v, k if k != "in use" else "skipped (in use)") for k, v in stats.items() if v)
    line = "qmod-cache: %smodules: %s; cache %s: %.1f MiB before pruning, %.1f MiB after%s%s" % (
        "job %s: " % args.job if args.job else "",
        ", ".join("%s %d" % (k, v) for k, v in sorted(counts.items())) or "none",
        cache.root, before / 1048576.0, after / 1048576.0,
        " (removed: %s)" % removed if removed else "", note)
    print(line)
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(prog="qore-qmod-cache", description=__doc__.split("\n")[0])
    sub = p.add_subparsers(dest="command")
    c = sub.add_parser("context", help="write the build context")
    c.add_argument("--source-dir", required=True)
    c.add_argument("--build-dir", required=True)
    c.add_argument("--qcc", required=True)
    c.add_argument("--input", action="append", default=[], help="a file whose content is part of the context")
    c.add_argument("--output", required=True)
    e = sub.add_parser("exec", help="compile a module through the cache")
    e.add_argument("--cache-dir", required=True)
    e.add_argument("--context", required=True)
    e.add_argument("--build-dir", required=True)
    e.add_argument("--name", required=True)
    e.add_argument("--source-root", required=True)
    e.add_argument("--source", action="append", default=[])
    e.add_argument("--default-module-dir", action="append", default=[],
                   help="a directory that the module loader searches after QORE_MODULE_DIR")
    e.add_argument("qcc_argv", nargs=argparse.REMAINDER)
    v = sub.add_parser("verify", help="check the qmods that the cache supplied to a build")
    v.add_argument("--build-dir", required=True)
    v.add_argument("--qcc")
    v.add_argument("--require-sidecars", action="store_true")
    s = sub.add_parser("stats", help="print the hits and misses of a build and the size of the cache")
    s.add_argument("--cache-dir", required=True)
    s.add_argument("--build-dir")
    r = sub.add_parser("prune", help="remove old cache entries")
    r.add_argument("--cache-dir", required=True)
    r.add_argument("--max-age-days", type=float, help="remove the entries not used for this many days")
    r.add_argument("--keep-build", help="keep only the entries that the modules of this build tree came from")
    r.add_argument("--max-size", type=float, help="remove the least recently used entries above this many MiB")
    r.add_argument("--build-dir", help="the build tree whose hits and misses are printed")
    r.add_argument("--job", help="the name of the job, for the statistics line")
    args = p.parse_args(argv)
    if args.command == "exec" and args.qcc_argv and args.qcc_argv[0] == "--":
        args.qcc_argv = args.qcc_argv[1:]
    handlers = {"context": cmd_context, "exec": cmd_exec, "verify": cmd_verify, "stats": cmd_stats,
                "prune": cmd_prune}
    if args.command not in handlers:
        p.print_help()
        return 2
    try:
        return handlers[args.command](args)
    except CacheError as e:
        log("error: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
