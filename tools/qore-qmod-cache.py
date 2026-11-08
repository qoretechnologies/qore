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
- sign: signs the entries that a build stored, after the guard passed (the private key's file path on stdin)
- signer-check, check-openssl: check the keys and the OpenSSL 3 command of a job before its build
- stats: prints the hits, misses and cache size of a build
- prune: removes cache entries not used since a time, then the oldest ones above a size limit

Environment (read by "exec"; never part of a key):
- QORE_QMOD_CACHE_DISABLE=1: compile every module and store nothing
- QORE_QMOD_CACHE_READONLY=1: look modules up but store nothing
- QORE_QMOD_CACHE_AUDIT=<rate>[:<seed>]: on a hit, also compile a sample of the modules (a rate from 0 to 1 of
  them, chosen by their key and the seed) and fail the build unless qcc writes the same module byte for byte; a
  failure quarantines the cache the job writes to
- QORE_QMOD_CACHE_VERIFY_KEY=<key>: the Ed25519 public key (a file or PEM text); only entries signed with the
  private key are used, and the job writes nothing unless QORE_QMOD_CACHE_SIGN_AFTER_BUILD is set
- QORE_QMOD_CACHE_SIGN_AFTER_BUILD=1: with the public key, store the entries unsigned; "sign" signs them after the
  build, with the private key, whose file path it reads from stdin (only the jobs of protected refs have it, and the
  build never does: QORE_QMOD_CACHE_SIGNING_KEY in the environment of "exec" disables the cache)
- QORE_QMOD_CACHE_OPENSSL=<path>: the OpenSSL 3 command for the signatures
- QORE_QMOD_CACHE_TEST_CORRUPT_RESTORE=1|.d|.d.fingerprints: for the tests only, a restored file (the module, its
  depfile or its fingerprints) is damaged after the copy
"""

import argparse
import atexit
import base64
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
    "QORE_TEST_PLATFORM_SUBSET",
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
#: the only record version read: 2 (name, dependencies, unavailable optional modules, the names of the compile-time
#: closure, the module's own module path directives); a module with a record of another version is never cached
QAMI_VERSION = 2
QAMI_MAX_PAYLOAD = 16 * 1024 * 1024

FINGERPRINT_HEADER = "# qore AOT module source fingerprints 1"

QORE_SOURCE_SUFFIXES = (".qm", ".qc")

#: a %include directive: qcc does not list the files a module includes in its depfile
INCLUDE_DIRECTIVE = re.compile(rb"^[ \t]*%include\b[ \t]*([^\r\n]*)", re.M)


class CacheError(Exception):
    """An inconsistency that makes a module a miss, or makes "verify" fail"""


class NoOpenSSL(CacheError):
    """Signed entries are configured, but there is no OpenSSL 3 command to make or check the signatures"""


class KeyCollision(Exception):
    """qcc wrote other files than the cache has for the same key: the key misses an input"""


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


class ModuleInfo:
    """The metadata record of an AOT module

    - dependencies: the hard dependencies of the module descriptor (not the modules required with %requires(reexport))
    - optional: the %try-module requests that failed when the module was compiled
    - compiled_with: the names of every module of the compile-time closure, direct and transitive, re-exported
      ones too, but not the module itself
    - prepend_paths, append_paths: the module's own %prepend-module-path and %append-module-path directories as
      compiled in
    """

    def __init__(self, name, dependencies, optional, compiled_with, prepend_paths=(), append_paths=()):
        self.name = name
        self.dependencies = dependencies
        self.optional = optional
        self.compiled_with = compiled_with
        self.prepend_paths = list(prepend_paths)
        self.append_paths = list(append_paths)


def parse_qami_record(data, offset):
    """Parses a version 2 AOT module metadata record at @p offset; returns a ModuleInfo or None"""
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
        compiled_with, pos = read_list(pos)
        prepend, pos = read_list(pos)
        append, pos = read_list(pos)
        info = ModuleInfo(name, deps, optional, compiled_with, prepend, append)
    except (ValueError, UnicodeDecodeError):
        return None
    if pos != end:
        return None
    return info


def record_version_of(data, name):
    """The version of a metadata record for module @p name in @p data that is not version 2, or None: the header and
    the module name, which every version starts its payload with, are all that is read of it"""
    pos = data.find(QAMI_MAGIC)
    encoded = name.encode("utf-8")
    while pos >= 0:
        if pos + 16 <= len(data):
            version, size, name_len = struct.unpack_from("<III", data, pos + 4)
            if (version != QAMI_VERSION and size <= QAMI_MAX_PAYLOAD and pos + 12 + size <= len(data)
                    and name_len == len(encoded) and data[pos + 16:pos + 16 + name_len] == encoded):
                return version
        pos = data.find(QAMI_MAGIC, pos + 1)
    return None


def read_module_info(path, name):
    """Returns the ModuleInfo of the version 2 metadata record of the AOT module @p name

    The record is found by its magic and must parse completely and name the module; exactly one such record must be
    in the file (an ELF note or a Mach-O section holds it), or CacheError is raised.
    """
    with open(path, "rb") as f:
        data = f.read()
    found = []
    pos = data.find(QAMI_MAGIC)
    while pos >= 0:
        rec = parse_qami_record(data, pos)
        if rec and rec.name == name:
            found.append(rec)
        pos = data.find(QAMI_MAGIC, pos + 1)
    if not found:
        other = record_version_of(data, name)
        if other is not None:
            raise CacheError("module '%s' has a metadata record of version %d; the cache uses only version %d "
                             "(compile it with a qcc that writes it)" % (path, other, QAMI_VERSION))
    if len(found) != 1:
        raise CacheError("module '%s' has %d metadata records for '%s'" % (path, len(found), name))
    return found[0]


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
    """Runs a command and returns its output; the command gets none of the cache's settings (child_environment())"""
    try:
        res = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             env=child_environment(os.environ if env is None else env), timeout=120)
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


def cmake_cache_value(path, name):
    """Returns the value of a CMake cache entry, or None"""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"^([A-Za-z0-9_]+):[A-Z]+=(.*)$", line.rstrip("\n"))
                if m and m.group(1) == name:
                    return m.group(2)
    except OSError:
        pass
    return None


def tool_versions(cmake_cache):
    """The first line of --version of the parser generators that generate libqore's parser and scanner"""
    rv = {}
    for name, var in (("bison", "BISON_EXECUTABLE"), ("flex", "FLEX_EXECUTABLE")):
        exe = cmake_cache_value(cmake_cache, var) or shutil.which(name)
        rv[name] = first_lines(run_capture([exe, "--version"]), 1) if exe else "none"
    return rv


def shared_library_digests(build_dir):
    """The content of every shared library that libqore and qcc are linked with (the LLVM library and the system
    libraries, whose patch versions /etc/os-release does not name), by their real paths"""
    rv = {}
    targets = [os.path.join(build_dir, "qcc")]
    for n in sorted(os.listdir(build_dir)) if os.path.isdir(build_dir) else []:
        if n.startswith("libqore.") and not os.path.islink(os.path.join(build_dir, n)):
            targets.append(os.path.join(build_dir, n))
    for t in targets:
        if not os.path.isfile(t):
            continue
        if sys.platform == "darwin":
            out = run_capture(["otool", "-L", t])
            libs = [line.strip().split(" (", 1)[0] for line in out.splitlines()[1:] if line.startswith("\t")]
        else:
            out = run_capture(["ldd", t])
            libs = []
            for line in out.splitlines():
                m = re.search(r"=>\s*(/\S+)", line) or re.match(r"\s*(/\S+)", line)
                if m:
                    libs.append(m.group(1))
        for lib in libs:
            if lib.startswith("@"):
                # @rpath/libqore is the build's own library, whose sources are in the context; @rpath/libLLVM is
                # found through LLVM_DIR below
                continue
            real = os.path.realpath(lib)
            if real not in rv:
                # a library of the dyld shared cache has no file: its path and the OS version stand for it
                rv[real] = file_record(real) if os.path.isfile(real) else "NO-FILE"
    # the LLVM library that qcc generates code with: LLVM_PACKAGE_VERSION names only its x.y.z
    llvm_dir = cmake_cache_value(os.path.join(build_dir, "CMakeCache.txt"), "LLVM_DIR")
    if llvm_dir:
        libdir = os.path.dirname(os.path.dirname(os.path.normpath(llvm_dir)))
        try:
            names = sorted(os.listdir(libdir))
        except OSError:
            names = []
        for n in names:
            if n.startswith("libLLVM") and (".so" in n or n.endswith(".dylib")):
                real = os.path.realpath(os.path.join(libdir, n))
                if real not in rv and os.path.isfile(real):
                    rv[real] = file_record(real)
    return rv


def qcc_identity(version_output):
    m = re.search(r"^AOT runtime identity: ([0-9a-f]{64})\s*$", version_output, re.M)
    return m.group(1) if m else None


def compute_context(source_dir, build_dir, qcc, inputs, env=None):
    """Computes the build context: everything that can change what qcc writes and is the same for every module"""
    env = os.environ if env is None else env
    version = run_capture([qcc, "--version"], env=qcc_environment(env))
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
        "tools": tool_versions(os.path.join(build_dir, "CMakeCache.txt")),
        "libraries": shared_library_digests(build_dir),
        # the local time zone of parse-time date values, when TZ (which is in the key) is not set
        "localtime": [os.path.realpath("/etc/localtime"), file_record(os.path.realpath("/etc/localtime"))],
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


#: directives that change the module path while a module is parsed: the modules they make qcc find are not in the
#: candidate-file records, so a module using one is not stored
MODULE_PATH_DIRECTIVE = re.compile(rb"^[ \t]*%(prepend|append)-module-path\b", re.M)


def check_no_module_path_directives(source_root, deps):
    """Raises CacheError for a module whose sources, or the files they include, change the module path
    (%prepend-module-path, %append-module-path, with or without environment variables in them)"""
    for path in own_source_files(source_root) + module_include_files(source_root, deps):
        with open(path, "rb") as f:
            if MODULE_PATH_DIRECTIVE.search(f.read()):
                raise CacheError("source '%s' changes the module path, which the key does not cover" % path)


_openssl = []


def find_openssl(env):
    """Returns an OpenSSL 3 command (Ed25519 with -rawin), or None

    QORE_QMOD_CACHE_OPENSSL names it when set (and nothing else is tried); otherwise the openssl of the PATH and the
    usual MacPorts and Homebrew locations are tried (macOS' own openssl is LibreSSL, which has no Ed25519).
    """
    if _openssl:
        return _openssl[0]
    if env.get("QORE_QMOD_CACHE_OPENSSL"):
        cands = [env["QORE_QMOD_CACHE_OPENSSL"]]
    else:
        cands = [shutil.which("openssl", path=env.get("PATH")), "/opt/local/bin/openssl",
                 "/opt/homebrew/opt/openssl@3/bin/openssl", "/usr/local/opt/openssl@3/bin/openssl", "/usr/bin/openssl"]
    found = None
    for c in cands:
        if not c or not os.path.isfile(c):
            continue
        m = re.match(r"^OpenSSL ([0-9]+)\.", run_capture([c, "version"], env=child_environment(env)))
        if m and int(m.group(1)) >= 3:
            found = c
            break
    _openssl.append(found)
    return found


def child_environment(env):
    """The environment of every process the cache runs (qcc, openssl): without the cache's settings, among them the
    keys"""
    return {k: v for k, v in env.items() if not k.startswith("QORE_QMOD_CACHE_")}


def signed_data(entry):
    """The bytes an entry's signature covers: the entry without its signature"""
    return json.dumps({k: v for k, v in entry.items() if k != "signature"}, sort_keys=True,
                      separators=(",", ":")).encode("utf-8")


class Signer:
    """Checks the signatures of entries with the Ed25519 public key QORE_QMOD_CACHE_VERIFY_KEY (OpenSSL 3)

    The public key is not secret: every job can check that an entry was signed with the private key, which only the
    "sign" command uses, after the build (see sign_entries()).  The key is a file or PEM text.
    """

    def __init__(self, env):
        self.openssl = find_openssl(env)
        if not self.openssl:
            raise NoOpenSSL("signed entries need OpenSSL 3 (set QORE_QMOD_CACHE_OPENSSL)")
        self.env = child_environment(env)
        self.dir = tempfile.mkdtemp(prefix="qmod-cache-verify.")
        atexit.register(self.close)
        value = env["QORE_QMOD_CACHE_VERIFY_KEY"]
        if os.path.isfile(value):
            self.public = value
        elif value.lstrip().startswith("-----BEGIN"):
            self.public = os.path.join(self.dir, "public.pem")
            with open(self.public, "w", encoding="ascii") as f:
                f.write(value)
        else:
            raise CacheError("QORE_QMOD_CACHE_VERIFY_KEY is neither a file nor a PEM public key")

    @staticmethod
    def from_env(env):
        """A verifier for the public key in the environment, or None without one

        The private key must not be in the environment of the build: the setup takes it out (see qmod-cache.sh), and
        only "sign" gets its file, after the build.
        """
        if env.get("QORE_QMOD_CACHE_SIGNING_KEY"):
            raise CacheError("QORE_QMOD_CACHE_SIGNING_KEY is in the environment of the build; only 'sign' may use the "
                             "private key, after the build (see test/docker_test/qmod-cache.sh)")
        if env.get("QORE_QMOD_CACHE_VERIFY_KEY"):
            return Signer(env)
        return None

    def verify(self, entry):
        sig_b64 = entry.get("signature")
        if not isinstance(sig_b64, str):
            return False
        try:
            raw = base64.b64decode(sig_b64.encode("ascii"), validate=True)
        except (ValueError, UnicodeEncodeError):
            return False
        fd, data = tempfile.mkstemp(dir=self.dir)
        sig = data + ".sig"
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(signed_data(entry))
            with open(sig, "wb") as f:
                f.write(raw)
            res = subprocess.run([self.openssl, "pkeyutl", "-verify", "-pubin", "-inkey", self.public, "-rawin",
                                  "-in", data, "-sigfile", sig], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 env=self.env)
            return res.returncode == 0
        finally:
            for p in (data, sig):
                try:
                    os.unlink(p)
                except OSError:
                    pass

    def close(self):
        shutil.rmtree(self.dir, ignore_errors=True)


def read_key_path(stream):
    """Reads the file path of the private key from @p stream (one line; never from the environment or the command
    line, see qmod-cache.sh); the file must be a regular file"""
    path = stream.readline().rstrip("\n")
    if not path:
        raise CacheError("no private key file given")
    if not os.path.isfile(path):
        raise CacheError("the private key is not a file (QORE_QMOD_CACHE_SIGNING_KEY must be a file variable)")
    return path


def sign_entry(openssl, key_path, entry, env, tmpdir):
    """Returns the signature of an entry, made with the private key in the file @p key_path"""
    fd, data = tempfile.mkstemp(dir=tmpdir)
    sig = data + ".sig"
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(signed_data(entry))
        res = subprocess.run([openssl, "pkeyutl", "-sign", "-inkey", key_path, "-rawin", "-in", data, "-out", sig],
                             stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
        if res.returncode:
            raise CacheError("cannot sign the entry: %s" % res.stderr.decode("utf-8", "replace").strip())
        with open(sig, "rb") as f:
            return base64.b64encode(f.read()).decode("ascii")
    finally:
        for p in (data, sig):
            try:
                os.unlink(p)
            except OSError:
                pass


def check_signature(entry, signer):
    """Returns None if the entry is signed with the signing key (or no key is set), else the reason"""
    if signer is None:
        return None
    if not signer.verify(entry):
        return "entry not signed with the signing key"
    return None


def qcc_environment(env):
    """The environment of qcc: without the cache's settings, among them the keys"""
    return child_environment(env)


def module_include_files(source_root, deps):
    """Returns the files that the module's sources include with %include, directly or from an included file

    Raises CacheError for an included file that the module's depfile does not list: a qcc that does not list included
    files (before the fix of design/aot-linked-module-sources.md "The sources of a module") would leave their content
    out of the key.  A name is resolved as the scanner does first, against the directory of the module's main source
    (the Program's script directory); one that cannot be resolved here (an environment variable, a file found through
    QORE_INCLUDE_DIR or an include path directive) counts as not listed.
    """
    listed = set(os.path.realpath(d) for d in deps)
    script_dir = source_root if os.path.isdir(source_root) else os.path.dirname(source_root)
    rv = []
    todo = list(own_source_files(source_root))
    seen = set(os.path.realpath(p) for p in todo)
    while todo:
        path = todo.pop(0)
        with open(path, "rb") as f:
            text = f.read()
        for m in INCLUDE_DIRECTIVE.finditer(text):
            name = m.group(1).strip().decode("utf-8", "replace")
            if len(name) > 1 and name[0] == name[-1] and name[0] in "\"'":
                name = name[1:-1]
            included = os.path.realpath(os.path.join(script_dir, name)) if name and name[0] != "$" else None
            if not included or included not in listed or not os.path.isfile(included):
                raise CacheError("source '%s' includes '%s', which its depfile does not list" % (path, name))
            if included not in seen:
                seen.add(included)
                rv.append(included)
                todo.append(included)
    return rv


def check_includes_listed(source_root, deps):
    """Raises CacheError for a module that includes a file its depfile does not list; see module_include_files()"""
    module_include_files(source_root, deps)


def classify_dependencies(build_dir, source_root, deps, include_files=()):
    """Returns the dependency records of a module from its depfile's dependencies

    - a source of the module itself: its content ("own")
    - a source of another in-tree module compiled or supplied by the cache: its content and that module's key
      ("module"), which covers the module's own dependencies
    - any other file (a binary module, a file the module includes, @p include_files, whatever its name): its content
      ("file")

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
        # a file the module includes is no module source, even when it is named like one (<x>.qm)
        if real.endswith(QORE_SOURCE_SUFFIXES) and real not in include_files:
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


def required_module_names(required, compiled_with, own_name):
    """The names of the modules a module requires: those of its metadata's dependencies (@p required, module specs)
    and every module of its compile-time closure (@p compiled_with): every module the module was compiled against,
    directly or not, re-exported ones too

    The descriptor dependencies leave out the modules required with %requires(reexport) (to avoid load cycles); the
    closure names them.  No name is taken from the depfile, whose files are no reliable sign of a module (a file that
    a module includes may be named <x>.qm for a module x).
    """
    names = set(module_spec_name(r) for r in required)
    names.update(compiled_with)
    names.discard(own_name)
    names.discard("")
    return sorted(names)


def resolution_records(names, dirs):
    """The candidate files in the module path of each module a module requires (see required_module_names()), by
    path: a file that appears for one of them, as one in a directory searched earlier, would make qcc load another
    module (the content of the files it loaded is in the dependency records)"""
    rv = []
    for name in names:
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

HEX64 = re.compile(r"^[0-9a-f]{64}$")
HEX32 = re.compile(r"^[0-9a-f]{32}$")


def _is_str_list(v):
    return isinstance(v, list) and all(isinstance(x, str) for x in v)


def _is_candidates(v):
    return isinstance(v, list) and all(isinstance(c, list) and len(c) == 2 and _is_str_list(c) for c in v)


def entry_problem(entry, key):
    """Returns why an entry read from the cache is not one this tool wrote for @p key, or None

    Every field a lookup reads is checked for its type, so that a damaged or forged entry (any valid JSON) is a miss,
    never an exception in the build.
    """
    if not isinstance(entry, dict):
        return "the entry is no object"
    if entry.get("schema") != SCHEMA:
        return "the entry has another schema"
    if entry.get("key") != key or not HEX64.match(key):
        return "the entry is not the one of its key"
    if not isinstance(entry.get("base_key"), str) or not HEX64.match(entry["base_key"]):
        return "the entry has no base key"
    deps = entry.get("deps")
    if not isinstance(deps, list):
        return "the entry has no dependency list"
    for d in deps:
        if (not isinstance(d, dict) or not isinstance(d.get("path"), str) or not isinstance(d.get("sha256"), str)
                or d.get("kind") not in ("own", "module", "file")):
            return "the entry has an invalid dependency"
        if d["kind"] == "module" and (not isinstance(d.get("module_root"), str)
                                       or not isinstance(d.get("module_key"), str)):
            return "the entry has an invalid module dependency"
    optional = entry.get("optional")
    if not isinstance(optional, list) or not all(
            isinstance(o, dict) and isinstance(o.get("spec"), str) and isinstance(o.get("name"), str)
            and _is_candidates(o.get("candidates")) for o in optional):
        return "the entry has an invalid optional module list"
    resolution = entry.get("resolution")
    if not isinstance(resolution, list) or not all(
            isinstance(r, dict) and isinstance(r.get("name"), str) and _is_candidates(r.get("candidates"))
            for r in resolution):
        return "the entry has an invalid resolution list"
    files = entry.get("files")
    if not isinstance(files, dict) or not all(isinstance(files.get(k), str) and HEX64.match(files[k])
                                              for k in ("qmod", ".d", ".d.fingerprints")):
        return "the entry has invalid file digests"
    if not isinstance(entry.get("mode"), int) or isinstance(entry.get("mode"), bool):
        return "the entry has no file mode"
    if "signature" in entry and not isinstance(entry["signature"], str):
        return "the entry has an invalid signature"
    if not isinstance(entry.get("epoch"), str) or not HEX32.match(entry["epoch"]):
        return "the entry has no epoch"
    return None


O_CLOEXEC = getattr(os, "O_CLOEXEC", 0)
O_DIRECTORY = getattr(os, "O_DIRECTORY", 0)
#: the largest entry read: an entry has a few KiB, a larger file is no entry
MAX_ENTRY_SIZE = 16 << 20


def open_regular(name, dir_fd=None):
    """Opens a regular file for reading without following a symbolic link (O_NOFOLLOW); raises OSError for a link
    and CacheError for a file of another type"""
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | O_CLOEXEC, dir_fd=dir_fd)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise CacheError("'%s' is no regular file" % name)
    except BaseException:
        os.close(fd)
        raise
    return fd


def read_fd(fd, limit=None):
    chunks = []
    total = 0
    while True:
        block = os.read(fd, 1 << 20)
        if not block:
            return b"".join(chunks)
        total += len(block)
        if limit is not None and total > limit:
            raise CacheError("the file is larger than %d bytes" % limit)
        chunks.append(block)


def remove_path(path):
    """Removes a file, a symbolic link (never what it points to) or a directory tree; best effort"""
    try:
        st = os.lstat(path)
    except OSError:
        return
    if stat.S_ISDIR(st.st_mode):
        shutil.rmtree(path, ignore_errors=True)
    else:
        try:
            os.unlink(path)
        except OSError:
            pass


def is_real_dir(path):
    """True for a directory that is not a symbolic link"""
    try:
        return stat.S_ISDIR(os.lstat(path).st_mode)
    except OSError:
        return False


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

    The cache has an epoch, <root>/epoch: a random value made when the first entry is stored, which every entry
    carries (and its signature covers).  An entry of another epoch is invalid.  A quarantine moves the directory, and
    its epoch with it, so the cache that replaces it gets a new one: an entry of the quarantined cache that is put
    back (replayed, with its valid signature) is refused.

    No symbolic link below <dir> is followed: every directory below it is opened with O_NOFOLLOW from its parent's
    descriptor and every file with O_NOFOLLOW, so a link in the cache (an entry, a base key directory, an object or
    one of its files) is an invalid entry, never what it points to; removing one removes the link only.
    """

    def __init__(self, directory):
        self.dir = os.path.abspath(directory)
        self.root = os.path.join(self.dir, SCHEMA)
        self.entries = os.path.join(self.root, "entries")
        self.objects = os.path.join(self.root, "objects")
        self.trash = os.path.join(self.root, "trash")
        self._epoch = None
        #: why the last entry that candidates() left out was invalid
        self.last_problem = None

    def epoch(self, create=False):
        """The epoch of the cache (32 hex digits), or None when it has none yet; with @p create, a cache without one
        gets one (the first job to link its file into place wins)"""
        if self._epoch:
            return self._epoch
        try:
            value = self.read_file("epoch", limit=64).decode("ascii").strip()
        except FileNotFoundError:
            value = None
        except (OSError, CacheError, UnicodeDecodeError):
            # a link or a damaged file: no entry of this cache can be valid until a writer replaces it
            value = None
            if create:
                remove_path(os.path.join(self.root, "epoch"))
        if value is None and create:
            self.make_dirs()
            dfd = self.open_dir()
            try:
                tmp = ".epoch.%d.%s" % (os.getpid(), os.urandom(8).hex())
                fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | O_CLOEXEC, 0o644,
                             dir_fd=dfd)
                try:
                    try:
                        os.write(fd, (os.urandom(16).hex() + "\n").encode("ascii"))
                    finally:
                        os.close(fd)
                    try:
                        os.link(tmp, "epoch", src_dir_fd=dfd, dst_dir_fd=dfd)
                    except FileExistsError:
                        # another job made it first
                        pass
                finally:
                    os.unlink(tmp, dir_fd=dfd)
            finally:
                os.close(dfd)
            value = self.read_file("epoch", limit=64).decode("ascii").strip()
        if value is not None and not HEX32.match(value):
            value = None
        self._epoch = value
        return value

    # access without following symbolic links

    def open_dir(self, *parts):
        """Returns a descriptor of <root>/<parts>, following no symbolic link below the cache directory (which itself
        may be one: a mount reached through a link)"""
        fd = os.open(self.dir, os.O_RDONLY | O_DIRECTORY | O_CLOEXEC)
        try:
            for name in (SCHEMA,) + parts:
                nfd = os.open(name, os.O_RDONLY | O_DIRECTORY | os.O_NOFOLLOW | O_CLOEXEC, dir_fd=fd)
                os.close(fd)
                fd = nfd
                if not stat.S_ISDIR(os.fstat(fd).st_mode):
                    raise CacheError("'%s' is no directory" % name)
        except BaseException:
            os.close(fd)
            raise
        return fd

    def open_file(self, *parts):
        """Returns a descriptor of the regular file <root>/<parts>, following no symbolic link"""
        dfd = self.open_dir(*parts[:-1])
        try:
            return open_regular(parts[-1], dfd)
        finally:
            os.close(dfd)

    def read_file(self, *parts, limit=None):
        fd = self.open_file(*parts)
        try:
            return read_fd(fd, limit)
        finally:
            os.close(fd)

    def file_sha256(self, *parts):
        fd = self.open_file(*parts)
        try:
            h = hashlib.sha256()
            while True:
                block = os.read(fd, 1 << 20)
                if not block:
                    return h.hexdigest()
                h.update(block)
        finally:
            os.close(fd)

    def read_entry(self, base, key):
        """Reads an entry; raises any exception for one that cannot be read (see candidates())"""
        return json.loads(self.read_file("entries", base, key + ".json", limit=MAX_ENTRY_SIZE).decode("utf-8"))

    def has_object(self, key):
        return is_real_dir(self.object_dir(key))

    def make_dirs(self, *parts):
        """Creates <root>/<parts> as directories; a symbolic link or a file in the place of one is removed first (the
        cache directory itself is created as needed, and may be a link)"""
        os.makedirs(self.dir, exist_ok=True)
        path = self.dir
        for name in (SCHEMA,) + parts:
            path = os.path.join(path, name)
            try:
                if stat.S_ISDIR(os.lstat(path).st_mode):
                    continue
                os.unlink(path)
            except FileNotFoundError:
                pass
            try:
                os.mkdir(path)
            except FileExistsError:
                # another job created it; open_dir() refuses it if it is no directory
                pass

    def write_file(self, data, *parts, mode=0o644):
        """Writes <root>/<parts> by renaming a new file over it in the same directory (a link there is replaced, not
        followed)"""
        self.make_dirs(*parts[:-1])
        dfd = self.open_dir(*parts[:-1])
        try:
            tmp = ".qmod-cache.%d.%s" % (os.getpid(), os.urandom(8).hex())
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | O_CLOEXEC, mode, dir_fd=dfd)
            try:
                try:
                    view = memoryview(data)
                    while view:
                        view = view[os.write(fd, view):]
                finally:
                    os.close(fd)
                os.rename(tmp, parts[-1], src_dir_fd=dfd, dst_dir_fd=dfd)
            except BaseException:
                try:
                    os.unlink(tmp, dir_fd=dfd)
                except OSError:
                    pass
                raise
        finally:
            os.close(dfd)

    def last_use(self, entry_path, key):
        """The last use of an entry: its last hit (the mtime of its lock file) or when it was stored"""
        used = 0
        for p in (entry_path, self.lock_path(key)):
            try:
                used = max(used, os.lstat(p).st_mtime)
            except OSError:
                pass
        return used

    def quarantine(self):
        """Takes every entry out of use at once (an audit found a module that qcc does not write again): the cache
        directory is renamed out of the way, so no job reuses an entry of it; a later prune deletes it"""
        now = int(time.time())
        dst = "%s.quarantine.%d.%d" % (self.root, os.getpid(), now)
        try:
            os.rename(self.root, dst)
        except OSError:
            return None
        # a rename keeps the directory's modification time; prune ages the quarantine by the time in its name, and
        # this one is for tools that look at the modification time
        try:
            os.utime(dst, (now, now), follow_symlinks=False)
        except (OSError, NotImplementedError):
            pass
        return dst

    def candidates(self, base, remove_invalid=False):
        """Returns the valid entries stored under a base key, most recently used first

        An entry that cannot be read (any exception while it is read or checked: a symbolic link, a file of another
        type, malformed JSON, JSON nested deeper than the parser's recursion limit) or that entry_problem() refuses is
        left out, and removed with @p remove_invalid (a cache this job writes to).
        """
        d = os.path.join(self.entries, base)
        try:
            dfd = self.open_dir("entries", base)
        except (OSError, CacheError):
            # a base key directory that is a link is no directory of the cache
            if remove_invalid and os.path.islink(d):
                remove_path(d)
            return []
        try:
            names = [n for n in os.listdir(dfd) if n.endswith(".json") and not n.startswith(".")]
            rv = []
            for n in names:
                p = os.path.join(d, n)
                key = n[:-5]
                try:
                    fd = open_regular(n, dfd)
                    try:
                        entry = json.loads(read_fd(fd, MAX_ENTRY_SIZE).decode("utf-8"))
                    finally:
                        os.close(fd)
                    problem = entry_problem(entry, key) or (entry.get("base_key") != base and "another base key") \
                        or (entry["epoch"] != self.epoch() and "the entry is of another epoch of the cache (replayed "
                            "from a quarantined cache, or the cache has no epoch)")
                except FileNotFoundError:
                    # removed since it was listed
                    continue
                except Exception as e:
                    # whatever an entry holds, reading it never fails the build
                    problem = "the entry cannot be read: %s" % e.__class__.__name__
                if problem:
                    self.last_problem = problem
                    if remove_invalid:
                        self.remove(p, key)
                    continue
                rv.append((self.last_use(p, key), p, entry))
        finally:
            os.close(dfd)
        rv.sort(key=lambda e: e[0], reverse=True)
        return [(p, e) for _, p, e in rv]

    def object_dir(self, key):
        return os.path.join(self.objects, key)

    def lock_path(self, key):
        return os.path.join(self.objects, key + ".lock")

    def open_lock(self, key, create=True):
        """Opens the lock file of an entry; returns None if it cannot be opened (a cache that cannot be written, or a
        lock file that is a link)"""
        flags = os.O_NOFOLLOW | O_CLOEXEC
        if create:
            try:
                self.make_dirs("objects")
                dfd = self.open_dir("objects")
                try:
                    return os.open(key + ".lock", os.O_RDWR | os.O_CREAT | flags, 0o666, dir_fd=dfd)
                finally:
                    os.close(dfd)
            except (OSError, CacheError):
                pass
        try:
            dfd = self.open_dir("objects")
            try:
                return os.open(key + ".lock", os.O_RDONLY | flags, dir_fd=dfd)
            finally:
                os.close(dfd)
        except (OSError, CacheError):
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
        """Removes an entry and its object unless a job is reading it; returns False if it is in use

        A link in the place of the entry or of the object is removed, never what it points to.
        """
        fd = self.open_lock(key)
        if fd is None:
            # a lock file that is a link: removed with what it guards
            if not os.path.islink(self.lock_path(key)):
                return False
        moved = []
        try:
            if fd is not None:
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except OSError:
                    return False
            self.make_dirs("trash")
            tag = "%s.%d.%d" % (key, os.getpid(), int(time.time() * 1e6))
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
            if fd is not None:
                os.close(fd)
        for p in moved:
            remove_path(p)
        return True

    def valid_object(self, base, key, signer=None, allow_unsigned=False):
        """True if the object of a key is backed by a valid entry: one that exists, is signed with the signing key
        (when one is set; with @p allow_unsigned, an entry without a signature, which a job stored to sign it after
        its build, is valid too, but not one with another signature) and has the digests of the object's files"""
        try:
            entry = self.read_entry(base, key)
            if entry_problem(entry, key) or entry["epoch"] != self.epoch():
                return False
            if (not allow_unsigned or "signature" in entry) and check_signature(entry, signer):
                return False
            return all(self.file_sha256("objects", key, "qmod" + suffix) == entry["files"][suffix or "qmod"]
                       for suffix in STORED_SUFFIXES)
        except Exception:
            # any exception reading the entry or its files (see candidates()) makes it invalid
            return False

    def store(self, base, key, entry, files, signer=None, allow_unsigned=False):
        """Stores the files of a compiled module

        An object that no valid entry backs (a damaged or forged one, an interrupted store, a link) is replaced.  When
        a valid entry exists already (another job stored the key first), the files must be the same: KeyCollision is
        raised otherwise, as the key then misses an input.
        """
        # the entry carries the cache's epoch; a signed one must have been signed for it
        epoch = self.epoch(create=True)
        if "epoch" not in entry:
            entry["epoch"] = epoch
        elif entry["epoch"] != epoch:
            raise CacheError("the entry is of another epoch of the cache")
        obj = self.object_dir(key)
        entry_path = os.path.join(self.entries, base, key + ".json")
        if os.path.lexists(obj) and not self.valid_object(base, key, signer, allow_unsigned):
            if not self.remove(entry_path, key):
                raise OSError("the invalid entry %s is being read by another job" % key)
        fd = self.open_lock(key)
        if fd is None:
            raise OSError("cannot create the lock file of entry %s" % key)
        try:
            # shared: a store does not conflict with a reader, but an entry being stored is not pruned
            fcntl.flock(fd, fcntl.LOCK_SH)
            created = False
            if not os.path.lexists(obj):
                self.make_dirs("objects")
                tmp = tempfile.mkdtemp(prefix=".tmp.", dir=self.objects)
                try:
                    for suffix, src in files.items():
                        shutil.copyfile(src, os.path.join(tmp, "qmod" + suffix))
                    try:
                        os.rename(tmp, obj)
                        created = True
                    except OSError:
                        # another job stored it meanwhile
                        if not os.path.lexists(obj):
                            raise
                finally:
                    shutil.rmtree(tmp, ignore_errors=True)
            if not created:
                # the same key must give the same files: anything else means that the key misses an input
                for suffix in files:
                    if self.file_sha256("objects", key, "qmod" + suffix) != entry["files"][suffix or "qmod"]:
                        raise KeyCollision("the cache has another module for key %s (file 'qmod%s'): qcc wrote "
                                           "different files for the same inputs" % (key, suffix))
            self.write_file((json.dumps(entry, sort_keys=True, indent=1) + "\n").encode(), "entries", base,
                            key + ".json")
            self.mark_used(fd)
        finally:
            os.close(fd)
        # keep the most recently used entries of the base key; the module is stored, so nothing here can fail it
        try:
            cands = self.candidates(base, remove_invalid=True)
            for p, e in cands[MAX_ENTRIES_PER_BASE:]:
                self.remove(p, e["key"])
        except Exception as e:
            log("pruning the entries of base key %s failed: %s: %s" % (base, e.__class__.__name__, e))


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
        # the public key with which the entries this job reads are verified (see design/qmod-build-cache.md "Trust
        # levels"); a job with it writes only when its entries are signed after the build (QORE_QMOD_CACHE_SIGN_AFTER_
        # BUILD, set by qore_qmod_cache_setup for a job with the private key): it stores them unsigned, and "sign"
        # signs them once the guard passed
        self.signer = Signer.from_env(env)
        self.sign_after_build = env.get("QORE_QMOD_CACHE_SIGN_AFTER_BUILD") in ("1", "true", "yes")
        if self.signer and not self.sign_after_build:
            self.readonly = True
        self.base = base_key(self.context["digest"], self.argv, self.cwd, env, self.source_root, self.extra)

    def files(self):
        return {"": self.out, ".d": self.out + ".d", ".d.fingerprints": self.out + ".d.fingerprints"}

    def sidecar(self, origin, key, entry=None, stored=False):
        rec = {"schema": SCHEMA, "name": self.name, "origin": origin, "base_key": self.base, "key": key,
               "context": self.context["digest"], "identity": self.identity, "source_root": self.source_root,
               "stored": stored}
        if os.path.isfile(self.out):
            rec["qmod_sha256"] = sha256_file(self.out)
        if entry is not None:
            rec["entry"] = entry
        atomic_write(self.out + SIDECAR_SUFFIX, (json.dumps(rec, sort_keys=True, indent=1) + "\n").encode())

    def search_dirs(self):
        return module_search_dirs(self.env, self.default_dirs)

    def try_hit(self, cache, readonly=None):
        """Restores the first entry whose dependencies still hold; returns (entry, reason for the last miss)

        @param readonly True for a cache this job does not write to (a cache of another trust level): no last-use
        mark, no removal of a damaged entry; None for this job's setting
        """
        if readonly is None:
            readonly = self.readonly
        cands = cache.candidates(self.base, remove_invalid=not readonly)
        if not cands:
            return None, cache.last_problem or "no entry"
        reason = None
        for path, entry in cands:
            key = entry["key"]
            reason = check_signature(entry, self.signer)
            if reason:
                continue
            reason = check_dependencies(self.build_dir, self.source_root, entry["deps"]) \
                or check_optional(entry["optional"], self.search_dirs()) \
                or check_resolution(entry["resolution"], self.search_dirs())
            if reason:
                continue
            if result_key(self.base, entry["deps"], entry["optional"], entry["resolution"]) != key:
                reason = "entry key mismatch"
                if not readonly:
                    cache.remove(path, key)
                continue
            # held while the entry is read, so that no job prunes it meanwhile
            fd = cache.open_lock(key, create=not readonly)
            damaged = None
            try:
                if fd is not None:
                    fcntl.flock(fd, fcntl.LOCK_SH)
                try:
                    restored = self.restore(cache, entry)
                except FileNotFoundError:
                    # removed since it was listed
                    reason = "entry removed"
                    continue
                except Exception as e:
                    reason = damaged = str(e) or e.__class__.__name__
                    continue
                # the dependency file and fingerprints first, the module last, each renamed into place
                for suffix in (".d", ".d.fingerprints", ""):
                    try:
                        os.replace(restored[suffix], self.out + suffix)
                    except BaseException:
                        for tmp in restored.values():
                            try:
                                os.unlink(tmp)
                            except OSError:
                                pass
                        raise
                    del restored[suffix]
                    corrupt = self.env.get("QORE_QMOD_CACHE_TEST_CORRUPT_RESTORE")
                    if corrupt and (corrupt == suffix or (corrupt in ("1", "qmod") and suffix == "")):
                        # for the tests: a copy that went wrong
                        with open(self.out + suffix, "ab") as f:
                            f.write(b"x")
                for suffix in STORED_SUFFIXES:
                    digest = entry["files"][suffix or "qmod"]
                    if sha256_file(self.out + suffix) != digest:
                        raise CacheError("restored file '%s' differs from the cache" % (self.out + suffix))
                if not readonly:
                    cache.mark_used(fd)
                return entry, None
            finally:
                if fd is not None:
                    os.close(fd)
                # a damaged entry is removed once it is no longer read
                if damaged and not readonly:
                    cache.remove(path, key)
        return None, reason

    def restore(self, cache, entry):
        """Copies the stored files of an entry to temporary files beside the module's files and checks them; returns
        {suffix: temporary file}, or raises

        Each stored file is opened without following a symbolic link and copied while its digest is computed, and must
        have the digest of the entry; the module must contain the build's AOT runtime identity, and the depfile must
        name the module.  The module gets the mode qcc gave it.
        """
        key = entry["key"]
        if not cache.has_object(key):
            raise FileNotFoundError(cache.object_dir(key))
        tmps = {}
        try:
            for suffix in STORED_SUFFIXES:
                src = cache.open_file("objects", key, "qmod" + suffix)
                try:
                    dst = self.out + suffix
                    tfd, tmp = tempfile.mkstemp(prefix=".qmod-cache.", dir=os.path.dirname(dst) or ".")
                    tmps[suffix] = tmp
                    h = hashlib.sha256()
                    with os.fdopen(tfd, "wb") as out:
                        while True:
                            block = os.read(src, 1 << 20)
                            if not block:
                                break
                            h.update(block)
                            out.write(block)
                finally:
                    os.close(src)
                if h.hexdigest() != entry["files"][suffix or "qmod"]:
                    raise CacheError("stored file 'qmod%s' is damaged" % suffix)
            os.chmod(tmps[""], int(entry["mode"]) & 0o777)
            if not contains_identity(tmps[""], self.identity):
                raise CacheError("stored module lacks the AOT runtime identity %s" % self.identity)
            # only modules with a version 2 metadata record are cached (see read_module_info())
            try:
                read_module_info(tmps[""], self.name)
            except CacheError as e:
                raise CacheError("stored module: %s" % e)
            target, _ = read_depfile(tmps[".d"])
            if os.path.abspath(target) != self.out:
                raise CacheError("stored dependency file names '%s'" % target)
            rv, tmps = tmps, {}
            return rv
        finally:
            for tmp in tmps.values():
                try:
                    os.unlink(tmp)
                except OSError:
                    pass

    def run_qcc(self, argv=None):
        return subprocess.run(argv or self.argv, env=qcc_environment(self.env)).returncode

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
        includes = module_include_files(self.source_root, deps)
        check_no_module_path_directives(self.source_root, deps)
        records = classify_dependencies(self.build_dir, self.source_root, deps, set(includes))
        info = read_module_info(self.out, self.name)
        if info.prepend_paths or info.append_paths:
            raise CacheError("the module changes the module path, which the key does not cover")
        optional_specs = info.optional
        # the compile may have written files in the module path since a lookup listed it
        _listdir_cache.clear()
        dirs = self.search_dirs()
        optional = []
        for spec in optional_specs:
            name = module_spec_name(spec)
            optional.append({"spec": spec, "name": name, "candidates": module_candidates(name, dirs)})
        resolution = resolution_records(required_module_names(info.dependencies, info.compiled_with, self.name),
                                        dirs)
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
        return subprocess.run(args.qcc_argv, env=qcc_environment(env)).returncode
    # a job that can only verify signed entries writes nothing
    readonly = readonly or mod.readonly
    cache = Cache(args.cache_dir)
    # the caches this job reads but never writes: those of a more trusted level
    read_caches = [Cache(d) for d in args.read_cache_dir]
    reason = "cache disabled"
    if not disabled:
        entry = None
        for c, ro in [(cache, None)] + [(c, True) for c in read_caches]:
            try:
                entry, r = mod.try_hit(c, ro)
            except Exception as e:
                # whatever a cache holds, a lookup that fails is a miss, never a failed build
                entry, r = None, "lookup failed: %s: %s" % (e.__class__.__name__, e)
            if entry:
                hit_cache, hit_readonly = c, ro or readonly
                break
            if c is cache:
                reason = r
        if entry:
            try:
                if audit_selected(entry["key"], env.get("QORE_QMOD_CACHE_AUDIT", "")):
                    mod.audit(entry)
                    log("hit %s (audited: qcc wrote the same module)" % mod.name)
                else:
                    log("hit %s" % mod.name)
            except CacheError as e:
                return fail_key_hole(mod, hit_cache, hit_readonly, "AUDIT FAILED: %s" % e)
            mod.sidecar("hit", entry["key"], entry)
            write_index(mod.build_dir, mod.source_root, mod.out, entry["key"])
            return 0
    # a miss: the index still names the module's earlier key, but with the size, modification time and inode of the
    # earlier qmod, which the compile replaces (qcc renames a new file into place), so no dependent uses that key
    rc = mod.run_qcc()
    if rc:
        return rc
    try:
        key, entry = mod.describe()
    except Exception as e:
        # a module the cache cannot describe is compiled and not stored, never a failed build
        log("miss %s (%s); not stored: %s" % (mod.name, reason, e))
        try:
            mod.sidecar("uncacheable", None)
        except OSError:
            pass
        return 0
    stored = ""
    if not disabled and not readonly:
        try:
            cache.store(mod.base, key, entry, mod.files(), mod.signer, allow_unsigned=mod.sign_after_build)
            stored = "; stored" + (" (signed after the build)" if mod.sign_after_build else "")
        except KeyCollision as e:
            return fail_key_hole(mod, cache, False, "KEY COLLISION: %s" % e)
        except Exception as e:
            # whatever the cache holds, a store that fails leaves the compiled module, never a failed build
            stored = "; not stored: %s: %s" % (e.__class__.__name__, e)
    log("miss %s (%s)%s" % (mod.name, reason, stored))
    mod.sidecar("miss", key, entry, stored=stored.startswith("; stored"))
    write_index(mod.build_dir, mod.source_root, mod.out, key)
    return 0


def fail_key_hole(mod, cache, readonly, msg):
    """An audit or a store found that qcc does not write the module a cache has for its key: the key misses an
    input, so no entry of that cache can be trusted.  The job fails, so that someone finds the input; a cache this job
    writes to is quarantined first, so that no later job reuses its entries (a cache of another trust level is left to
    its own jobs, whose audits find the same)."""
    log(msg)
    if not readonly:
        moved = cache.quarantine()
        log("quarantined the cache %s%s: every module will be compiled again" % (
            cache.root, " to %s" % moved if moved else " (failed)"))
    try:
        mod.sidecar("failed", None)
    except OSError:
        pass
    return 1


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
    no_signer = False
    try:
        signer = Signer.from_env(os.environ)
    except NoOpenSSL:
        # "exec" built every module without the cache (qore_qmod_cache_setup does not even enable it): nothing the
        # cache supplied needs a signature check, and a module it did supply fails below
        log("no OpenSSL 3 to check signatures: the build ran without the cache")
        signer = None
        no_signer = True
    except (OSError, CacheError) as e:
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
            if no_signer:
                raise CacheError("its signature cannot be checked without OpenSSL 3")
            reason = check_signature(entry, signer)
            if reason:
                raise CacheError(reason)
            if sha256_file(qmod) != entry["files"]["qmod"]:
                raise CacheError("it is not the module the cache supplied")
            # the loader decides from the depfile and the fingerprints whether the module is current
            for suffix in (".d", ".d.fingerprints"):
                if sha256_file(qmod + suffix) != entry["files"][suffix]:
                    raise CacheError("its %s is not the one the cache supplied" % suffix)
            target, deps = read_depfile(qmod + ".d")
            if os.path.abspath(target) != os.path.abspath(qmod):
                raise CacheError("its dependency file names '%s'" % target)
            if deps != [d["path"] for d in entry["deps"]]:
                raise CacheError("its dependency file lists other dependencies than the cache entry")
            for d in entry["deps"]:
                if sha256_file(os.path.realpath(d["path"])) != d["sha256"]:
                    raise CacheError("dependency '%s' has changed since the module was supplied" % d["path"])
            info = read_module_info(qmod, rec["name"])
            if info.optional != [o["spec"] for o in entry["optional"]]:
                raise CacheError("its unavailable optional modules differ from the cache entry")
            names = required_module_names(info.dependencies, info.compiled_with, rec["name"])
            if names != [r["name"] for r in entry["resolution"]]:
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
    if args.require_sidecars and not sidecars and not no_signer:
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
    # no symbolic link is followed (see Cache)
    if not is_real_dir(cache.root) or not is_real_dir(cache.entries):
        return rv
    try:
        bases = os.listdir(cache.entries)
    except OSError:
        return rv
    for base in bases:
        d = os.path.join(cache.entries, base)
        if not is_real_dir(d):
            continue
        try:
            names = os.listdir(d)
        except OSError:
            continue
        for n in names:
            if not n.endswith(".json") or n.startswith("."):
                continue
            p = os.path.join(d, n)
            key = n[:-5]
            try:
                st = os.lstat(p)
            except OSError:
                continue
            used = st.st_mtime
            try:
                used = max(used, os.lstat(cache.lock_path(key)).st_mtime)
            except OSError:
                pass
            size = st.st_size + (dir_size(cache.object_dir(key)) if cache.has_object(key) else 0)
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
    # a symbolic link in the place of a directory of the cache is removed (the link, not what it points to)
    for d in (cache.root, cache.entries, cache.objects, cache.trash):
        if os.path.islink(d) and old(d):
            remove_path(d)
            removed += 1
    if not is_real_dir(cache.root):
        return removed
    try:
        bases = os.listdir(cache.entries) if is_real_dir(cache.entries) else []
    except OSError:
        bases = []
    for base in bases:
        d = os.path.join(cache.entries, base)
        if not is_real_dir(d) and old(d):
            remove_path(d)
            removed += 1
    try:
        names = os.listdir(cache.objects) if is_real_dir(cache.objects) else []
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
                remove_path(p)
                removed += 1
            elif cache.remove(os.path.join(cache.entries, "-", key + ".json"), key):
                removed += 1
        elif os.path.islink(p):
            remove_path(p)
        elif not cache.has_object(key):
            # a lock file without an object
            fd = cache.open_lock(key, create=False)
            if fd is not None:
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    if not cache.has_object(key):
                        os.unlink(p)
                except OSError:
                    pass
                finally:
                    os.close(fd)
    # entries without an object
    for used, p, key, _ in cache_entries(cache):
        if not cache.has_object(key) and now - used > ORPHAN_AGE:
            if cache.remove(p, key):
                removed += 1
    # the leftovers of interrupted removals
    try:
        for n in os.listdir(cache.trash) if is_real_dir(cache.trash) else []:
            p = os.path.join(cache.trash, n)
            if old(p):
                remove_path(p)
    except OSError:
        pass
    return removed


def quarantine_time(name):
    """The time a cache was quarantined, from the name of its directory (<schema>.quarantine.<pid>.<time>); a name
    without one counts as quarantined now, so that it is kept for ORPHAN_AGE"""
    m = re.match(r"^.*\.quarantine\.[0-9]+\.([0-9]+)$", name)
    return int(m.group(1)) if m else time.time()


def cmd_prune(args):
    """Prunes the cache and prints one line with the hits and misses of a build and the size of the cache before and
    after: orphans older than ORPHAN_AGE and quarantined caches are removed, then the entries not used for
    --max-age-days, then the least recently used ones while the cache is larger than --max-size MiB; an entry that a
    job is reading, and a store or a removal in progress, are never touched"""
    cache = Cache(args.cache_dir)
    now = time.time()
    counts = {}
    if args.build_dir:
        counts, _ = build_counts_and_keys(args.build_dir)
    before = dir_size(cache.root)
    stats = {"age": 0, "size": 0, "orphans": 0, "quarantined caches": 0, "in use": 0}
    note = ""
    cache.make_dirs()
    dfd = cache.open_dir()
    try:
        lock_fd = os.open("prune.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | O_CLOEXEC, 0o666, dir_fd=dfd)
    finally:
        os.close(dfd)
    try:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            note = "; another job is pruning the cache"
        if not note:
            stats["orphans"] = remove_orphans(cache, now)
            # caches quarantined by a failed audit, once no job can still be reading them
            parent = os.path.dirname(cache.root)
            for n in os.listdir(parent):
                p = os.path.join(parent, n)
                if n.startswith(SCHEMA + ".quarantine.") and now - quarantine_time(n) > ORPHAN_AGE:
                    remove_path(p)
                    stats["quarantined caches"] += 1
            entries = cache_entries(cache)
            keep = []
            # the entries that a job is reading, which are left for a later prune
            busy = set()
            for e in entries:
                used, p, key, _ = e
                if args.max_age_days is not None and now - used > args.max_age_days * 86400:
                    reason = "age"
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
            for base in os.listdir(cache.entries) if is_real_dir(cache.entries) else []:
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


def comparable_entry(entry):
    """An entry without what "sign" changes, and without the time it was stored (another job that stored the same key
    wrote the same entry at another time)"""
    return {k: v for k, v in entry.items() if k not in ("signature", "created")}


def cmd_sign(args):
    """Signs the entries that a build stored, after its guard passed (qore_qmod_cache_finish)

    The file path of the private key is read from stdin: the key is never in the environment of the build, nor on a
    command line (see qmod-cache.sh); openssl gets the path as its -inkey argument, and none of the cache's settings
    in its environment.  An entry is signed only when the cache still has the entry this build stored (by the build's
    sidecars), with the stored files' digests.  Failures are reported, not fatal: an unsigned entry is a miss for
    every job.
    """
    try:
        key_path = read_key_path(sys.stdin)
    except CacheError as e:
        log("not signing: %s" % e)
        return 1
    env = dict(os.environ)
    openssl = find_openssl(env)
    if not openssl:
        log("not signing: no OpenSSL 3 command (set QORE_QMOD_CACHE_OPENSSL)")
        return 1
    cache = Cache(args.cache_dir)
    child_env = child_environment(env)
    tmpdir = tempfile.mkdtemp(prefix="qmod-cache-sign.")
    signed = 0
    failed = 0
    try:
        for sc in find_sidecars(os.path.abspath(args.build_dir)):
            try:
                with open(sc, "r", encoding="utf-8") as f:
                    rec = json.load(f)
                if rec.get("origin") != "miss" or rec.get("stored") is not True:
                    continue
                ours = rec["entry"]
                key, base = rec["key"], rec["base_key"]
                if entry_problem(ours, key) or ours.get("base_key") != base:
                    raise CacheError("the sidecar has no valid entry")
                fd = cache.open_lock(key)
                if fd is None:
                    raise CacheError("cannot open the lock file of the entry")
                try:
                    # shared: not removed while it is signed
                    fcntl.flock(fd, fcntl.LOCK_SH)
                    entry = cache.read_entry(base, key)
                    if entry_problem(entry, key) or comparable_entry(entry) != comparable_entry(ours):
                        raise CacheError("the cache does not have the entry this build stored")
                    if entry["epoch"] != cache.epoch():
                        raise CacheError("the entry is of another epoch of the cache (it was quarantined)")
                    for suffix in STORED_SUFFIXES:
                        if cache.file_sha256("objects", key, "qmod" + suffix) != entry["files"][suffix or "qmod"]:
                            raise CacheError("stored file 'qmod%s' is not the one this build stored" % suffix)
                    entry["signature"] = sign_entry(openssl, key_path, entry, child_env, tmpdir)
                    cache.write_file((json.dumps(entry, sort_keys=True, indent=1) + "\n").encode(), "entries", base,
                                     key + ".json")
                finally:
                    os.close(fd)
                signed += 1
            except Exception as e:
                failed += 1
                log("not signed: %s: %s: %s" % (sc[:-len(SIDECAR_SUFFIX)], e.__class__.__name__, e))
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)
    log("signed %d entries that this build stored%s" % (signed, ", %d failed (they are misses for every job)" % failed
                                                          if failed else ""))
    return 0


def cmd_signer_check(args):
    """Checks the keys of a job before its build (qore_qmod_cache_setup)

    With --verify-only: OpenSSL 3 and the public key QORE_QMOD_CACHE_VERIFY_KEY.  Otherwise the private key, whose
    file path is read from stdin: it must sign, its public key must verify the signature, and it must be the public key
    QORE_QMOD_CACHE_VERIFY_KEY when that is set; the public key is printed (PEM), so that the job can verify with it.
    Nothing of the private key is printed.
    """
    env = dict(os.environ)
    env.pop("QORE_QMOD_CACHE_SIGNING_KEY", None)
    openssl = find_openssl(env)
    if not openssl:
        log("no OpenSSL 3 command (Ed25519 with -rawin); set QORE_QMOD_CACHE_OPENSSL")
        return 1
    child_env = child_environment(env)
    tmpdir = tempfile.mkdtemp(prefix="qmod-cache-keys.")
    try:
        def public_pem(argv):
            res = subprocess.run([openssl, "pkey"] + argv + ["-pubout"], stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL, env=child_env)
            text = res.stdout.decode("ascii", "replace")
            return text if not res.returncode and text.startswith("-----BEGIN PUBLIC KEY-----") else None
        given = None
        if env.get("QORE_QMOD_CACHE_VERIFY_KEY"):
            try:
                verifier = Signer(env)
            except CacheError as e:
                log(str(e))
                return 1
            given = public_pem(["-pubin", "-in", verifier.public])
            if not given:
                log("QORE_QMOD_CACHE_VERIFY_KEY is no public key")
                return 1
        if args.verify_only:
            if not given:
                log("no QORE_QMOD_CACHE_VERIFY_KEY")
                return 1
            return 0
        try:
            key_path = read_key_path(sys.stdin)
        except CacheError as e:
            log(str(e))
            return 1
        derived = public_pem(["-in", key_path])
        if not derived:
            log("QORE_QMOD_CACHE_SIGNING_KEY is no private key")
            return 1
        if given and given != derived:
            log("QORE_QMOD_CACHE_VERIFY_KEY is not the public key of QORE_QMOD_CACHE_SIGNING_KEY")
            return 1
        # an Ed25519 key: a signature that the public key verifies
        probe = {"probe": "qmod-cache"}
        try:
            probe["signature"] = sign_entry(openssl, key_path, probe, child_env, tmpdir)
        except CacheError:
            log("QORE_QMOD_CACHE_SIGNING_KEY cannot sign (an Ed25519 key is needed)")
            return 1
        pub = os.path.join(tmpdir, "public.pem")
        with open(pub, "w", encoding="ascii") as f:
            f.write(derived)
        if not Signer(dict(env, QORE_QMOD_CACHE_VERIFY_KEY=pub)).verify(probe):
            log("the signature of QORE_QMOD_CACHE_SIGNING_KEY does not verify")
            return 1
        sys.stdout.write(derived)
        return 0
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def cmd_check_openssl(args):
    """Exits with 0 when an OpenSSL 3 command for the signatures is there (see find_openssl())"""
    openssl = find_openssl(os.environ)
    if not openssl:
        log("no OpenSSL 3 command (Ed25519 with -rawin); set QORE_QMOD_CACHE_OPENSSL")
        return 1
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
    e.add_argument("--read-cache-dir", action="append", default=[],
                   help="a cache that is read after --cache-dir and never written to (of a more trusted level)")
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
    sub.add_parser("check-openssl", help="check that there is an OpenSSL 3 command for the signatures")
    g = sub.add_parser("sign", help="sign the entries a build stored (the private key's file path on stdin)")
    g.add_argument("--cache-dir", required=True)
    g.add_argument("--build-dir", required=True)
    k = sub.add_parser("signer-check", help="check the keys of a job (the private key's file path on stdin)")
    k.add_argument("--verify-only", action="store_true", help="check only OpenSSL and the public key")
    r = sub.add_parser("prune", help="remove old cache entries")
    r.add_argument("--cache-dir", required=True)
    r.add_argument("--max-age-days", type=float, help="remove the entries not used for this many days")
    r.add_argument("--max-size", type=float, help="remove the least recently used entries above this many MiB")
    r.add_argument("--build-dir", help="the build tree whose hits and misses are printed")
    r.add_argument("--job", help="the name of the job, for the statistics line")
    args = p.parse_args(argv)
    if args.command == "exec" and args.qcc_argv and args.qcc_argv[0] == "--":
        args.qcc_argv = args.qcc_argv[1:]
    handlers = {"context": cmd_context, "exec": cmd_exec, "verify": cmd_verify, "stats": cmd_stats,
                "prune": cmd_prune, "check-openssl": cmd_check_openssl, "sign": cmd_sign,
                "signer-check": cmd_signer_check}
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
