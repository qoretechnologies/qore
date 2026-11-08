#!/usr/bin/env python3
"""Tests of the qmod build cache (tools/qore-qmod-cache.py, design/qmod-build-cache.md).

- the keys: each input of the build context, of a module's base key and of its result key changes the key, and
  nothing else does;
- the parts the keys and checks are made of: the depfile parser (the module loader's rules), the environment
  filter, the AOT module metadata reader, the optional module candidates, the audit sample;
- the cache with CMake, make and a stand-in for qcc: a cold build stores every module, a warm build from an empty
  build tree compiles none and writes the same files, a change to one module compiles only that module and the
  modules that depend on it (transitively), a damaged entry, a new AOT runtime identity, a source that changes while
  it is compiled and an optional module that becomes available are misses, "verify" fails for a supplied module that
  was changed afterwards, and the audit fails the build when qcc does not write the same module again;
- the cache with the real qcc and qore of a build tree (QORE_TEST_BUILD_DIR, as test-cmake.sh runs these tests):
  the same cold, warm and changed builds, modules that are byte for byte those qcc writes, and the module loader
  loading the modules the cache supplied without finding them out of date.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "tools" / "qore-qmod-cache.py"
CMAKE = os.environ.get("CMAKE_EXECUTABLE", "cmake")


def load_tool():
    spec = importlib.util.spec_from_file_location("qore_qmod_cache", str(TOOL))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


qc = load_tool()


def ed25519_keys(directory, name):
    """Returns (the private key's file, the public key's PEM text) of a new Ed25519 key pair, or None without
    OpenSSL 3"""
    openssl = qc.find_openssl(os.environ)
    if not openssl:
        return None
    priv = os.path.join(str(directory), name + ".pem")
    pub = os.path.join(str(directory), name + ".pub.pem")
    subprocess.run([openssl, "genpkey", "-algorithm", "ed25519", "-out", priv], check=True, capture_output=True)
    subprocess.run([openssl, "pkey", "-in", priv, "-pubout", "-out", pub], check=True, capture_output=True)
    with open(pub) as f:
        return priv, f.read()


def sign_build(cache, build, key_path, env=None):
    """Runs "sign" for a build, with the private key's file path on stdin, as qore_qmod_cache_finish does"""
    full = {k: v for k, v in os.environ.items() if not k.startswith("QORE_QMOD_CACHE_")}
    full.update(env or {})
    return subprocess.run([sys.executable, str(TOOL), "sign", "--cache-dir", str(cache), "--build-dir", str(build)],
                          input=key_path + "\n", capture_output=True, text=True, timeout=120, env=full)


def qami_record(name, deps=(), optional=(), compiled_with=(), prepend=(), append=(), version=2):
    """An AOT module metadata record (version 2): with @p compiled_with, the module names of the module's compile-time
    closure, and its own module path directives; @p version 1 writes the earlier layout, without them"""
    def s(v):
        b = v.encode()
        return struct.pack("<I", len(b)) + b
    def lst(items):
        return struct.pack("<I", len(items)) + b"".join(s(i) for i in items)
    payload = s(name) + lst(deps) + lst(optional)
    if version != 1:
        payload += lst(compiled_with) + lst(prepend) + lst(append)
    return b"QAMI" + struct.pack("<II", version, len(payload)) + payload


class DepfileTest(unittest.TestCase):
    def test_rule(self):
        target, deps = qc.parse_depfile_text("/b/M.qmod: /s/M.qm \\\n    /s/a\\ b.qc \\\n    /s/c.qc\n")
        self.assertEqual("/b/M.qmod", target)
        self.assertEqual(["/s/M.qm", "/s/a b.qc", "/s/c.qc"], deps)

    def test_space_before_colon(self):
        self.assertEqual(("/b/M.qmod", ["x"]), qc.parse_depfile_text("/b/M.qmod : x\n"))

    def test_not_a_rule(self):
        for text in ("", "no rule here\n", ": x\n", "a b: x\n", "a\\\nb: x\n"):
            with self.assertRaises(qc.CacheError, msg=text):
                qc.parse_depfile_text(text)


class EnvironmentTest(unittest.TestCase):
    def test_filter(self):
        env = {"QORE_MODULE_DIR": "/m", "QCC_JOBS": "4", "TZ": "Europe/Prague", "CXX": "c++", "LANG": "C",
               "QORE_TEST_OPTS": "-penable-debug", "QORE_DB_CONNSTR_MYSQL": "secret", "QORE_QMOD_CACHE_AUDIT": "1",
               "QORE_SRC_DIR": "/src", "HOME": "/home/x", "CI_JOB_ID": "1", "PATH": "/bin"}
        self.assertEqual([["CXX", "c++"], ["LANG", "C"], ["QCC_JOBS", "4"], ["QORE_MODULE_DIR", "/m"],
                          ["TZ", "Europe/Prague"]], qc.key_environment(env))

    def test_excluded_names_are_not_read(self):
        """No libqore or qcc source names an environment variable that the key leaves out"""
        sources = [ROOT / "qcc-main.cpp"]
        for d in ("lib", "include"):
            for dirpath, _, names in os.walk(ROOT / d):
                sources += [Path(dirpath) / n for n in names if n.endswith((".cpp", ".h", ".qpp", ".ypp", ".lpp"))]
        literals = set()
        for p in sources:
            literals.update(re.findall(r'"(Q(?:ORE|CC)_[A-Z0-9_]+)"', p.read_text(errors="replace")))
        self.assertTrue(literals, "no environment variable literal found in the sources")
        for name in literals:
            self.assertNotIn(name, qc.ENV_EXCLUDED_NAMES)
            self.assertFalse(name.startswith(qc.ENV_EXCLUDED_PREFIXES), name)


class ModuleInfoTest(unittest.TestCase):
    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        self.dir = Path(d.name)

    def write(self, data):
        p = self.dir / "M.qmod"
        p.write_bytes(data)
        return str(p)

    def test_record(self):
        # an ELF note header has the record's magic as its type; the note's name follows it, not a record
        note = struct.pack("<III", 5, 100, 0x494d4151) + b"Qore\0\0\0\0"
        p = self.write(b"\x7fELF junk" + note + qami_record("M", ["Util"], ["xml >= 1.3"], ["Util"]) + b"tail QAMI")
        info = qc.read_module_info(p, "M")
        self.assertEqual((["Util"], ["xml >= 1.3"], ["Util"]),
                         (info.dependencies, info.optional, info.compiled_with))

    def test_other_versions(self):
        """Only version 2 records are read: a module with a record of version 1 (before the closure was recorded) or
        of a later version is refused, saying which version it has"""
        for version in (1, 3):
            with self.assertRaisesRegex(qc.CacheError, "metadata record of version %d" % version):
                qc.read_module_info(self.write(qami_record("M", ["Util"], version=version)), "M")

    def test_record_v2(self):
        """Version 2 adds the names of every module of the compile-time closure (re-exported ones too) and the
        module's own module path directives"""
        p = self.write(b"junk" + qami_record("M", ["Util"], ["xml"], ["Util", "Base", "Re"], ["/p1", "/p2"], ["/a"])
                       + b"tail")
        info = qc.read_module_info(p, "M")
        self.assertEqual(["Util"], info.dependencies)
        self.assertEqual(["xml"], info.optional)
        self.assertEqual(["Util", "Base", "Re"], info.compiled_with)
        self.assertEqual(["/p1", "/p2"], info.prepend_paths)
        self.assertEqual(["/a"], info.append_paths)
        info = qc.read_module_info(self.write(qami_record("M")), "M")
        self.assertEqual(([], [], []), (info.compiled_with, info.prepend_paths, info.append_paths))
        # a version 2 record must hold all six fields, and nothing after them
        rec = bytearray(qami_record("M", compiled_with=["A"]))
        short = rec[:-4]
        struct.pack_into("<I", short, 8, len(short) - 12)
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(bytes(short)), "M")
        longer = rec + b"\0\0\0\0"
        struct.pack_into("<I", longer, 8, len(longer) - 12)
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(bytes(longer)), "M")
        v1 = qami_record("M", ["Util"], version=1)
        rec = bytearray(v1)
        rec[4] = 2
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(bytes(rec)), "M")
        rec = bytearray(qami_record("M", compiled_with=["A"]))
        rec[4] = 3
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(bytes(rec)), "M")

    def test_no_or_two_records(self):
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(b"no metadata"), "M")
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(qami_record("Other")), "M")
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(qami_record("M") + qami_record("M")), "M")

    def test_damaged_record(self):
        rec = bytearray(qami_record("M", ["Util"]))
        rec[8] += 1  # the payload size
        with self.assertRaises(qc.CacheError):
            qc.read_module_info(self.write(bytes(rec) + b"\0" * 8), "M")

    def test_spec_name(self):
        self.assertEqual("xml", qc.module_spec_name("xml >= 1.3"))
        self.assertEqual("Mod", qc.module_spec_name("Mod"))
        self.assertEqual("a", qc.module_spec_name("a>1"))

    def test_candidates(self):
        a, b = self.dir / "a", self.dir / "b"
        (b / "Opt").mkdir(parents=True)
        a.mkdir()
        self.assertEqual([], qc.module_candidates("Opt", [str(a), str(b), str(self.dir / "missing")]))
        (b / "Opt" / "Opt.qm").write_text("x")
        (a / "Opt-api-2.0.qmod").write_text("y")
        (a / "Optional.qm").write_text("z")
        # a process lists each directory once
        self.assertEqual([], qc.module_candidates("Opt", [str(a), str(b)]))
        qc._listdir_cache.clear()
        names = [os.path.relpath(p, self.dir) for p, _ in qc.module_candidates("Opt", [str(a), str(b)])]
        self.assertEqual(["a/Opt-api-2.0.qmod", os.path.join("b", "Opt", "Opt.qm")], names)


class IncludeTest(unittest.TestCase):
    def test_includes_listed(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        root = Path(d.name)
        mod = root / "M.qm"
        part = root / "part.ql"
        part.write_text("x\n")
        mod.write_text("%modern\nmodule M {}\n")
        qc.check_includes_listed(str(mod), [str(mod)])
        for directive in ("%include part.ql", "  %include \"part.ql\"", "%include 'part.ql'"):
            mod.write_text(directive + "\nmodule M {}\n")
            qc.check_includes_listed(str(mod), [str(mod), str(part)])
            with self.assertRaises(qc.CacheError, msg=directive):
                qc.check_includes_listed(str(mod), [str(mod)])
        # a file included from an included file must be listed too
        nested = root / "nested.ql"
        nested.write_text("x\n")
        part.write_text("%include nested.ql\n")
        mod.write_text("%include part.ql\nmodule M {}\n")
        qc.check_includes_listed(str(mod), [str(mod), str(part), str(nested)])
        with self.assertRaises(qc.CacheError):
            qc.check_includes_listed(str(mod), [str(mod), str(part)])
        part.write_text("x\n")
        # a name that cannot be resolved here
        mod.write_text("%include $DIR/part.ql\nmodule M {}\n")
        with self.assertRaises(qc.CacheError):
            qc.check_includes_listed(str(mod), [str(mod), str(part)])


class RequiredModuleNamesTest(unittest.TestCase):
    def test_names(self):
        """The names are those qcc wrote: the descriptor dependencies and the compile-time closure, which has the
        modules required with %requires(reexport) (no descriptor dependency) too; a file named <x>.qm that a module
        includes is no module x (no name is taken from the depfile)"""
        self.assertEqual(["Base", "Mid", "Re", "json"],
                         qc.required_module_names(["Mid >= 1.0"], ["Mid", "Base", "Re", "json", "Top"], "Top"))


class EntryProblemTest(unittest.TestCase):
    def valid(self):
        return {"schema": qc.SCHEMA, "key": "a" * 64, "base_key": "b" * 64,
                "deps": [{"path": "/s/M.qm", "kind": "own", "sha256": "c"},
                         {"path": "/s/U.qm", "kind": "module", "sha256": "d", "module_root": "/s/U.qm",
                          "module_key": "e"}],
                "optional": [{"spec": "xml", "name": "xml", "candidates": [["/m/x", "f"]]}],
                "resolution": [{"name": "U", "candidates": [["/s/U.qm", "/s/U.qm"]]}],
                "files": {"qmod": "1" * 64, ".d": "2" * 64, ".d.fingerprints": "3" * 64}, "mode": 0o755,
                "epoch": "4" * 32}

    def test_entry_problem(self):
        self.assertIsNone(qc.entry_problem(self.valid(), "a" * 64))
        for bad in ([], "x", None, 1, {}):
            self.assertIsNotNone(qc.entry_problem(bad, "a" * 64), bad)
        self.assertIsNotNone(qc.entry_problem(self.valid(), "f" * 64))
        for field, value in (("deps", None), ("deps", ["x"]), ("deps", [{"path": 1}]), ("optional", None),
                             ("optional", [{"spec": "x"}]), ("resolution", {"a": 1}),
                             ("resolution", [{"name": "U", "candidates": [["a"]]}]), ("files", []),
                             ("files", {"qmod": "x"}), ("mode", "755"), ("mode", True), ("signature", 1),
                             ("base_key", None), ("schema", "other"), ("epoch", None), ("epoch", "x"),
                             ("epoch", 1)):
            e = self.valid()
            e[field] = value
            self.assertIsNotNone(qc.entry_problem(e, "a" * 64), (field, value))
        e = self.valid()
        e["deps"][1].pop("module_key")
        self.assertIsNotNone(qc.entry_problem(e, "a" * 64))


class CacheStoreTest(unittest.TestCase):
    """Cache.store(): collisions, replaced objects, and the entries kept under a base key"""

    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        self.root = Path(d.name)
        self.cache = qc.Cache(str(self.root / "cache"))

    def files(self, content):
        rv = {}
        for suffix in qc.STORED_SUFFIXES:
            p = self.root / ("src" + suffix)
            p.write_bytes(content + suffix.encode())
            rv[suffix] = str(p)
        return rv

    def entry(self, key, files, base="b" * 64):
        return {"schema": qc.SCHEMA, "key": key, "base_key": base, "deps": [], "optional": [], "resolution": [],
                "mode": 0o755, "files": {(s or "qmod"): qc.sha256_file(p) for s, p in files.items()}}

    def test_collision(self):
        f1 = self.files(b"one")
        self.cache.store("b" * 64, "c" * 64, self.entry("c" * 64, f1), f1)
        # the same files again: fine
        self.cache.store("b" * 64, "c" * 64, self.entry("c" * 64, f1), f1)
        f2 = self.files(b"two")
        with self.assertRaises(qc.KeyCollision):
            self.cache.store("b" * 64, "c" * 64, self.entry("c" * 64, f2), f2)

    def test_invalid_object_is_replaced(self):
        f1 = self.files(b"one")
        self.cache.store("b" * 64, "c" * 64, self.entry("c" * 64, f1), f1)
        # damaged: replaced by the new files
        (Path(self.cache.object_dir("c" * 64)) / "qmod").write_bytes(b"damaged")
        f2 = self.files(b"two")
        self.cache.store("b" * 64, "c" * 64, self.entry("c" * 64, f2), f2)
        self.assertEqual(b"two", (Path(self.cache.object_dir("c" * 64)) / "qmod").read_bytes())
        # unsigned for a cache whose entries are signed: replaced
        keys = ed25519_keys(self.root, "k")
        if not keys:
            self.skipTest("OpenSSL 3 is required for signed entries")
        signer = qc.Signer({"QORE_QMOD_CACHE_VERIFY_KEY": keys[1]})
        self.addCleanup(signer.close)
        f3 = self.files(b"three")
        e3 = self.entry("c" * 64, f3)
        e3["epoch"] = self.cache.epoch(create=True)
        e3["signature"] = qc.sign_entry(signer.openssl, keys[0], e3, {}, str(self.root))
        self.cache.store("b" * 64, "c" * 64, e3, f3, signer)
        self.assertEqual(b"three", (Path(self.cache.object_dir("c" * 64)) / "qmod").read_bytes())
        self.assertTrue(self.cache.valid_object("b" * 64, "c" * 64, signer))
        # an unsigned entry, stored to be signed after the build, is valid for a job that signs after its build, but
        # not one with another signature
        e4 = self.entry("c" * 64, f3)
        e4["epoch"] = e3["epoch"]
        (Path(self.cache.entries) / ("b" * 64) / ("c" * 64 + ".json")).write_text(json.dumps(e4))
        self.assertFalse(self.cache.valid_object("b" * 64, "c" * 64, signer))
        self.assertTrue(self.cache.valid_object("b" * 64, "c" * 64, signer, allow_unsigned=True))
        e4["signature"] = e3["signature"]
        (Path(self.cache.entries) / ("b" * 64) / ("c" * 64 + ".json")).write_text(json.dumps(e4))
        self.assertTrue(self.cache.valid_object("b" * 64, "c" * 64, signer, allow_unsigned=True))
        e4["mode"] = 0o700
        (Path(self.cache.entries) / ("b" * 64) / ("c" * 64 + ".json")).write_text(json.dumps(e4))
        self.assertFalse(self.cache.valid_object("b" * 64, "c" * 64, signer, allow_unsigned=True))

    def test_unreadable_entry(self):
        """Any exception while an entry is read makes it invalid: left out (removed from a writable cache), never
        raised, also from the pruning of the base key after a store"""
        f1 = self.files(b"one")
        base = "b" * 64
        self.cache.store(base, "c" * 64, self.entry("c" * 64, f1), f1)
        nested = Path(self.cache.entries) / base / ("d" * 64 + ".json")
        nested.write_text("[" * 200000 + "]" * 200000)
        self.assertEqual(["c" * 64], [e["key"] for _, e in self.cache.candidates(base)])
        self.assertTrue(nested.exists())
        self.assertFalse(self.cache.valid_object(base, "d" * 64))
        # the entry of the object being stored itself
        (Path(self.cache.entries) / base / ("c" * 64 + ".json")).write_text("[" * 200000 + "]" * 200000)
        self.assertFalse(self.cache.valid_object(base, "c" * 64))
        self.cache.store(base, "c" * 64, self.entry("c" * 64, f1), f1)
        self.assertEqual(["c" * 64], [e["key"] for _, e in self.cache.candidates(base, remove_invalid=True)])
        self.assertFalse(nested.exists())

    def test_epoch(self):
        """Every entry carries the epoch of its cache, which a quarantine renews: an entry of another epoch is invalid"""
        self.assertIsNone(self.cache.epoch())
        f1 = self.files(b"one")
        base = "b" * 64
        self.cache.store(base, "c" * 64, self.entry("c" * 64, f1), f1)
        epoch = self.cache.epoch()
        self.assertRegex(epoch, r"^[0-9a-f]{32}$")
        self.assertEqual(epoch, qc.Cache(str(self.root / "cache")).epoch())
        entry_path = Path(self.cache.entries) / base / ("c" * 64 + ".json")
        self.assertEqual(epoch, json.loads(entry_path.read_text())["epoch"])
        old_entry = entry_path.read_text()
        old_object = Path(self.cache.object_dir("c" * 64))
        moved = self.cache.quarantine()
        self.assertTrue(moved)
        cache = qc.Cache(str(self.root / "cache"))
        self.assertIsNone(cache.epoch())
        cache.store(base, "c" * 64, self.entry("c" * 64, f1), f1)
        self.assertNotEqual(epoch, cache.epoch())
        # the entry of the quarantined cache, put back: invalid
        entry_path.write_text(old_entry)
        self.assertEqual([], cache.candidates(base))
        self.assertFalse(cache.valid_object(base, "c" * 64))
        self.assertIn("another epoch", cache.last_problem)
        # and removed from a cache that the job writes
        self.assertEqual([], cache.candidates(base, remove_invalid=True))
        self.assertFalse(entry_path.exists())
        self.assertFalse(old_object.exists())
        # an entry of another epoch is not stored
        e = self.entry("c" * 64, f1)
        e["epoch"] = epoch
        with self.assertRaises(qc.CacheError):
            cache.store(base, "c" * 64, e, f1)

    def test_least_recently_used_entries_of_a_base_go(self):
        base = "b" * 64
        keys = ["%064d" % i for i in range(qc.MAX_ENTRIES_PER_BASE + 1)]
        now = time.time()
        for i, key in enumerate(keys[:-1]):
            f = self.files(key.encode())
            self.cache.store(base, key, self.entry(key, f), f)
            # stored long ago, in order
            when = now - 10000 + i
            os.utime(self.cache.lock_path(key), (when, when))
            os.utime(os.path.join(self.cache.entries, base, key + ".json"), (when, when))
        # the first one stored was used recently (a hit)
        fd = self.cache.open_lock(keys[0])
        self.cache.mark_used(fd)
        os.close(fd)
        self.assertEqual(keys[0], self.cache.candidates(base)[0][1]["key"])
        f = self.files(keys[-1].encode())
        self.cache.store(base, keys[-1], self.entry(keys[-1], f), f)
        left = sorted(e["key"] for _, e in self.cache.candidates(base))
        # the least recently used one went, not the oldest stored one
        self.assertEqual(sorted(keys[:1] + keys[2:]), left)


class AuditSampleTest(unittest.TestCase):
    def test_sample(self):
        keys = ["%064x" % i for i in range(2000)]
        self.assertFalse(any(qc.audit_selected(k, "") for k in keys))
        self.assertFalse(any(qc.audit_selected(k, "0") for k in keys))
        self.assertTrue(all(qc.audit_selected(k, "1") for k in keys))
        half = sum(qc.audit_selected(k, "0.5:7") for k in keys)
        self.assertTrue(800 < half < 1200, half)
        self.assertEqual([qc.audit_selected(k, "0.1:3") for k in keys], [qc.audit_selected(k, "0.1:3") for k in keys])
        self.assertNotEqual([qc.audit_selected(k, "0.1:3") for k in keys],
                            [qc.audit_selected(k, "0.1:4") for k in keys])
        with self.assertRaises(qc.CacheError):
            qc.audit_selected(keys[0], "often")


class KeyTest(unittest.TestCase):
    """Each input of a key changes it; nothing else does"""

    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        self.dir = Path(d.name)
        self.single = self.dir / "qlib" / "Single.qm"
        self.single.parent.mkdir()
        self.single.write_text("%modern\nmodule Single {}\n")
        self.split = self.dir / "qlib" / "Split"
        self.split.mkdir()
        (self.split / "Split.qm").write_text("module Split {}\n")
        (self.split / "A.qc").write_text("class A {}\n")
        self.extra = self.dir / "extra.qc"
        self.extra.write_text("x")

    def key(self, root=None, context="c" * 64, argv=None, cwd="/b", env=None, extra=None):
        return qc.base_key(context, argv or ["qcc", "-m", str(root or self.single), "-o", "/b/Single.qmod"], cwd,
                           {"QORE_MODULE_DIR": "/m"} if env is None else env, str(root or self.single),
                           [str(self.extra)] if extra is None else extra)

    def test_base_key_inputs(self):
        k = self.key()
        self.assertEqual(k, self.key())
        self.assertNotEqual(k, self.key(context="d" * 64))
        self.assertNotEqual(k, self.key(argv=["qcc", "-O2", "-m", str(self.single), "-o", "/b/Single.qmod"]))
        self.assertNotEqual(k, self.key(cwd="/other"))
        self.assertNotEqual(k, self.key(env={"QORE_MODULE_DIR": "/n"}))
        self.assertNotEqual(k, self.key(env={"QORE_MODULE_DIR": "/m", "QORE_AOT_OPT_LEVEL": "1"}))
        self.assertNotEqual(k, self.key(env={"QORE_MODULE_DIR": "/m", "TZ": "UTC"}))
        self.assertNotEqual(k, self.key(extra=[]))
        # neither another environment variable nor a cache setting is an input
        self.assertEqual(k, self.key(env={"QORE_MODULE_DIR": "/m", "HOME": "/x", "QORE_QMOD_CACHE_AUDIT": "1",
                                          "QORE_TEST_OPTS": "-x", "QORE_DB_CONNSTR_PGSQL": "p"}))
        self.single.write_text("%modern\nmodule Single { # changed\n}\n")
        self.assertNotEqual(k, self.key())
        k = self.key()
        self.extra.write_text("y")
        self.assertNotEqual(k, self.key())

    def test_split_module_sources(self):
        k = self.key(root=self.split)
        (self.split / "A.qc").write_text("class A { int x; }\n")
        k2 = self.key(root=self.split)
        self.assertNotEqual(k, k2)
        (self.split / "B.qc").write_text("class B {}\n")
        k3 = self.key(root=self.split)
        self.assertNotEqual(k2, k3)
        (self.split / "schema.yaml").write_text("a: 1\n")
        k4 = self.key(root=self.split)
        self.assertNotEqual(k3, k4)
        # a qmod linked into the module directory is the build's output, not an input
        (self.split / "Split.qmod").symlink_to(self.dir / "nowhere.qmod")
        (self.split / "Split.qmod.d").write_text("x")
        self.assertEqual(k4, self.key(root=self.split))

    def test_result_key_inputs(self):
        deps = [{"path": "/s/M.qm", "kind": "own", "sha256": "a"},
                {"path": "/s/U.qm", "kind": "module", "sha256": "b", "module_root": "/s/U.qm", "module_key": "k"}]
        opt = [{"spec": "xml", "name": "xml", "candidates": []}]
        res = [{"name": "U", "candidates": [["/s/U.qm", "/s/U.qm"]]}]
        k = qc.result_key("base", deps, opt, res)
        self.assertEqual(k, qc.result_key("base", json.loads(json.dumps(deps)), opt, res))
        self.assertNotEqual(k, qc.result_key("other", deps, opt, res))
        changed = json.loads(json.dumps(deps))
        changed[1]["module_key"] = "k2"
        self.assertNotEqual(k, qc.result_key("base", changed, opt, res))
        changed = json.loads(json.dumps(deps))
        changed[0]["sha256"] = "c"
        self.assertNotEqual(k, qc.result_key("base", changed, opt, res))
        self.assertNotEqual(k, qc.result_key("base", deps[:1], opt, res))
        self.assertNotEqual(k, qc.result_key("base", deps, [], res))
        self.assertNotEqual(k, qc.result_key("base", deps, [{"spec": "xml", "name": "xml",
                                                             "candidates": [["/m/xml-api-2.0.qmod", "d"]]}], res))
        self.assertNotEqual(k, qc.result_key("base", deps, opt, []))
        self.assertNotEqual(k, qc.result_key("base", deps, opt, [{"name": "U", "candidates": [
            ["/e/U.qm", "/e/U.qm"], ["/s/U.qm", "/s/U.qm"]]}]))


FAKE_QCC_VERSION = r'''import sys
from pathlib import Path
print("qcc (Qore Code Compiler) v1.0")
print("Using Qore library v3.0.0")
print("AOT runtime identity: " + (Path(__file__).parent / "identity.txt").read_text().strip())
'''


class ContextTest(unittest.TestCase):
    """Each input of the build context changes its digest"""

    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        root = Path(d.name)
        self.src, self.build = root / "src", root / "build"
        for p, text in (("lib/QoreLib.cpp", "int x;\n"), ("lib/parser.ypp", "%%\n"), ("include/qore/Qore.h", "//\n"),
                        ("qcc-main.cpp", "int main() {}\n"), ("CMakeLists.txt", "project(x)\n"),
                        ("cmake/third-party-sources.cmake", "set(a 1)\n")):
            (self.src / p).parent.mkdir(parents=True, exist_ok=True)
            (self.src / p).write_text(text)
        for p, text in (("include/qore/intern/unix-config.h", "#define A 1\n"),
                        ("include/qore/intern/git-revision.h", '#define BUILD "1"\n'),
                        ("CMakeFiles/libqore.dir/flags.make", "CXX_FLAGS = -O2\n"),
                        ("CMakeFiles/qcc.dir/link.txt", "c++ -o qcc\n"),
                        ("CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=release\nQORE_X:BOOL=ON\n"
                                           "QORE_QMOD_CACHE_DIR:PATH=/c\nOTHER:STRING=1\nQORE_I:INTERNAL=1\n"),
                        ("qcc-format.stamp", "format=1\n"), ("aot-link.conf", "cxx=c++\n"),
                        ("identity.txt", "a" * 64 + "\n"), ("qcc.py", FAKE_QCC_VERSION)):
            (self.build / p).parent.mkdir(parents=True, exist_ok=True)
            (self.build / p).write_text(text)
        self.qcc = self.build / "qcc"
        self.qcc.write_text('#!/bin/sh\nexec "%s" "%s" "$@"\n' % (sys.executable, self.build / "qcc.py"))
        self.qcc.chmod(0o755)

    def digest(self):
        return qc.compute_context(str(self.src), str(self.build), str(self.qcc),
                                  [str(self.build / "qcc-format.stamp"), str(self.build / "aot-link.conf")])["digest"]

    def check_changes(self, path, text):
        before = self.digest()
        old = path.read_text()
        path.write_text(text)
        after = self.digest()
        path.write_text(old)
        return before != after

    def test_inputs(self):
        k = self.digest()
        self.assertEqual(k, self.digest())
        for rel, text in (("lib/QoreLib.cpp", "int y;\n"), ("lib/parser.ypp", "%% x\n"),
                          ("include/qore/Qore.h", "// x\n"), ("qcc-main.cpp", "int main() { return 1; }\n"),
                          ("CMakeLists.txt", "project(y)\n"), ("cmake/third-party-sources.cmake", "set(a 2)\n")):
            self.assertTrue(self.check_changes(self.src / rel, text), rel)
        for rel, text in (("include/qore/intern/unix-config.h", "#define A 2\n"),
                          ("CMakeFiles/libqore.dir/flags.make", "CXX_FLAGS = -O3\n"),
                          ("CMakeFiles/qcc.dir/link.txt", "c++ -o qcc -lx\n"),
                          ("CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=debug\nQORE_X:BOOL=ON\n"),
                          ("CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=release\nQORE_X:BOOL=OFF\n"),
                          ("qcc-format.stamp", "format=2\n"), ("aot-link.conf", "cxx=clang++\n"),
                          ("identity.txt", "b" * 64 + "\n")):
            self.assertTrue(self.check_changes(self.build / rel, text), rel)
        # a new libqore source
        (self.src / "lib" / "New.cpp").write_text("\n")
        self.assertNotEqual(k, self.digest())
        (self.src / "lib" / "New.cpp").unlink()
        self.assertEqual(k, self.digest())

    def test_not_inputs(self):
        k = self.digest()
        for rel, text in (("include/qore/intern/git-revision.h", '#define BUILD "2"\n'),
                          ("CMakeCache.txt", "CMAKE_BUILD_TYPE:STRING=release\nQORE_X:BOOL=ON\n"
                                             "QORE_QMOD_CACHE_DIR:PATH=/d\nOTHER:STRING=2\nQORE_I:INTERNAL=2\n")):
            self.assertFalse(self.check_changes(self.build / rel, text), rel)
        (self.src / "lib" / ".#QoreLib.cpp").write_text("editor lock\n")
        self.assertEqual(k, self.digest())

    def test_parser_generators_and_llvm_library(self):
        tools = self.build / "tools"
        tools.mkdir()
        bison = tools / "bison"
        bison.write_text("#!/bin/sh\necho 'bison (GNU Bison) 3.8.2'\n")
        bison.chmod(0o755)
        llvm = self.build / "llvm" / "lib"
        (llvm / "cmake" / "llvm").mkdir(parents=True)
        (llvm / "libLLVM.so.21.1").write_text("llvm 1\n")
        cache = self.build / "CMakeCache.txt"
        cache.write_text(cache.read_text() + "BISON_EXECUTABLE:FILEPATH=%s\nLLVM_DIR:PATH=%s\n"
                         % (bison, llvm / "cmake" / "llvm"))
        k = self.digest()
        self.assertTrue(self.check_changes(bison, "#!/bin/sh\necho 'bison (GNU Bison) 3.8.3'\n"))
        self.assertTrue(self.check_changes(llvm / "libLLVM.so.21.1", "llvm 2\n"))
        self.assertEqual(k, self.digest())

    def test_no_identity(self):
        (self.build / "identity.txt").write_text("none\n")
        with self.assertRaises(qc.CacheError):
            self.digest()


# A stand-in for qcc: "--version" prints the identity in identity.txt; "-m <module> -o <out> --depfile=<d>" writes
# <out> (the identity, an AOT module metadata record and a digest of the module's sources), the depfile (the module's
# own sources and those of the modules it %requires, as qcc lists them: direct dependencies only) and the source
# fingerprints, and logs the module's name in compiles.log.  A "%try-module <name>" whose module is not in a
# directory of QORE_MODULE_DIR is recorded as unavailable.  With nondeterministic.txt present, every module differs.
FAKE_QCC = r'''import hashlib, os, re, struct, sys, time
from pathlib import Path
here = Path(__file__).parent
args = sys.argv[1:]
leaked = sorted(k for k in os.environ if k.startswith("QORE_QMOD_CACHE_"))
if (here / "probe-env.txt").exists():
    leaked += sorted(k for k, v in os.environ.items() if "PRIVATE KEY" in v or v.endswith(".pem"))
if leaked:
    with open(here / "qcc-env.txt", "a") as log:
        log.write(" ".join(leaked) + "\n")
if "--version" in args:
    print("AOT runtime identity: " + (here / "identity.txt").read_text().strip())
    sys.exit(0)
out = Path(args[args.index("-o") + 1])
depfile = Path(next(a for a in args if a.startswith("--depfile=")).split("=", 1)[1])
src = Path(args[args.index("-m") + 1])
qlib = here / "qlib"
def sources(p):
    return sorted(p.glob("*.q[mc]")) if p.is_dir() else [p]
def module_path(name):
    for d in [Path(x) for x in os.environ.get("QORE_MODULE_DIR", "").split(":") if x] + [qlib]:
        for p in (d / (name + ".qm"), d / name, d / (name + "-api-1.0.qmod")):
            if p.exists():
                return p
    return None
own = sources(src)
text = "".join(p.read_text() for p in own)
# a qcc that lists included files in the depfile (list-includes.txt), as qcc does since the fix of
# design/aot-linked-module-sources.md, or one that does not
includes = []
if (here / "list-includes.txt").exists():
    for p in own:
        for name in re.findall(r"^%include (\S+)", p.read_text(), re.M):
            includes.append(p.parent / name)
            text += (p.parent / name).read_text()
deps, optional, required = [], [], []
# as qcc: a module required with %requires(reexport) is not a dependency of the module descriptor (metadata)
for kind, reexport, name in re.findall(r"^%(requires|try-module)(\(reexport\))? (\w+)", text, re.M):
    p = module_path(name)
    if p is None:
        if kind == "requires":
            sys.exit("module %s not found" % name)
        optional.append(name)
    else:
        deps += sources(p)
        if not reexport:
            required.append(name)
all_deps = own + includes + deps
lines = ["# qore AOT module source fingerprints 1"]
for p in all_deps + ([Path((here / "extra-dep.txt").read_text().strip())] if (here / "extra-dep.txt").exists() else []):
    st = p.stat()
    lines.append("%d %d %016x %s" % (st.st_size, st.st_mtime_ns, 0, p))
def s(v):
    b = v.encode()
    return struct.pack("<I", len(b)) + b
payload = s(out.stem) + struct.pack("<I", len(required)) + b"".join(s(r) for r in required) \
    + struct.pack("<I", len(optional)) + b"".join(s(o) for o in optional)
qami_version = 2
# a qcc before version 2 of the record (qami-v1.txt), which wrote no closure
if (here / "qami-v1.txt").exists():
    qami_version = 1
else:
    # the names of the compile-time closure, re-exported modules too
    closure, todo = [], [n for k, r, n in re.findall(r"^%(requires|try-module)(\(reexport\))? (\w+)", text, re.M)]
    while todo:
        name = todo.pop(0)
        p = module_path(name)
        if name in closure or p is None:
            continue
        closure.append(name)
        if p.suffix != ".qmod":
            todo += [n for k, r, n in re.findall(r"^%(requires|try-module)(\(reexport\))? (\w+)",
                                                   "".join(x.read_text() for x in sources(p)), re.M)]
    payload += struct.pack("<I", len(closure)) + b"".join(s(n) for n in closure)
    # a module path directive (qami-paths.txt): the module's own %prepend-module-path, compiled in
    paths = [str(here / "elsewhere")] if (here / "qami-paths.txt").exists() else []
    payload += struct.pack("<I", len(paths)) + b"".join(s(x) for x in paths) + struct.pack("<I", 0)
digest = hashlib.sha256(text.encode() + b"".join(p.read_bytes() for p in deps)).hexdigest()
noise = str(time.time()) if (here / "nondeterministic.txt").exists() else ""
tmp = out.with_name(out.name + ".tmp")
tmp.write_bytes(b"FAKEQMOD " + (here / "identity.txt").read_text().strip().encode() + b" " + digest.encode()
                + noise.encode() + b"\n" + b"QAMI" + struct.pack("<II", qami_version, len(payload)) + payload)
tmp.replace(out)
# a depfile with another dependency than the module was compiled from (extra-dep.txt names it)
listed = all_deps + ([Path((here / "extra-dep.txt").read_text().strip())] if (here / "extra-dep.txt").exists() else [])
depfile.write_text("%s: \\\n    %s\n" % (out, " \\\n    ".join(str(p) for p in listed)))
Path(str(out) + ".d.fingerprints").write_text("\n".join(lines) + "\n")
# a source that is written after qcc read it
if (here / "slow.txt").exists():
    time.sleep(0.05)
    for p in own:
        p.write_text(p.read_text() + "#\n")
with open(here / "compiles.log", "a") as log:
    log.write(out.stem + "\n")
'''


class CacheBuildTest(unittest.TestCase):
    """The cache in a CMake build of four modules with a stand-in for qcc: Base; Mid, which requires Base; Top, a
    module directory that requires Mid; and Other, which tries to load Opt"""

    def setUp(self):
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        if not shutil.which("make") and not shutil.which("gmake"):
            self.skipTest("make is required for the Makefile generator")
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-")
        self.addCleanup(d.cleanup)
        self.root = Path(d.name)
        self.source, self.build, self.cache = self.root / "source", self.root / "build", self.root / "cache"
        self.extra_dir = self.root / "extra-modules"
        self.extra_dir.mkdir()
        q = self.source / "qlib"
        (q / "Top").mkdir(parents=True)
        (q / "Base.qm").write_text("module Base {}\n")
        (q / "Mid.qm").write_text("%requires Base\nmodule Mid {}\n")
        (q / "Top" / "Top.qm").write_text("%requires Mid\nmodule Top {}\n")
        (q / "Top" / "Part.qc").write_text("class Part {}\n")
        (q / "Other.qm").write_text("%try-module Opt\n%requires binmod\nmodule Other {}\n")
        # a binary module that Other requires
        (self.extra_dir / "binmod-api-1.0.qmod").write_bytes(b"\x7fELF binary module 1\n")
        (self.source / "qcc.py").write_text(FAKE_QCC)
        (self.source / "identity.txt").write_text("1" * 64 + "\n")
        qcc = self.source / "qcc"
        qcc.write_text('#!/bin/sh\nexec "%s" "%s" "$@"\n' % (sys.executable, self.source / "qcc.py"))
        qcc.chmod(0o755)
        (self.source / "qcc-format.stamp").write_text("format\n")
        (self.source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.14...3.31)
project(QmodCache NONE)
include("{root}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_QCC_EXECUTABLE "{qcc}")
set(QCC_FORMAT_STAMP "${{CMAKE_SOURCE_DIR}}/qcc-format.stamp")
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_QMOD_CACHE_TOOL "{tool}")
set(QORE_QM_METADATA_ENV "QORE_MODULE_DIR={extra}")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
QORE_QMOD_CACHE_SETUP("{extra}")
qore_user_module("qlib/Base.qm")
qore_user_module("qlib/Mid.qm")
qore_user_module("qlib/Top")
qore_user_module("qlib/Other.qm")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
'''.format(root=ROOT.as_posix(), qcc=qcc.as_posix(), tool=TOOL.as_posix(), extra=self.extra_dir.as_posix()))

    def qmod(self, name):
        return self.build / "qlib-qmod" / (name + ".qmod" if name != "Top" else "Top/Top.qmod")

    def compiles(self):
        log = self.source / "compiles.log"
        return log.read_text().splitlines() if log.exists() else []

    def build_all(self, fresh=True, env=None, expect_rc=0, cache=None, read_dirs=None, target=None):
        """Builds every module (in a new build tree with @p fresh) and returns the modules qcc compiled"""
        if fresh:
            shutil.rmtree(self.build, ignore_errors=True)
            res = subprocess.run([CMAKE, "-G", "Unix Makefiles", "-S", str(self.source), "-B", str(self.build),
                                  "-DQORE_QMOD_CACHE_DIR=%s" % (cache or self.cache),
                                  "-DQORE_QMOD_CACHE_READ_DIRS=%s" % ";".join(str(d) for d in read_dirs or []),
                                  "-DPython3_EXECUTABLE=%s" % sys.executable],
                                 capture_output=True, text=True, timeout=120)
            self.assertEqual(0, res.returncode, res.stdout + res.stderr)
        before = len(self.compiles())
        full_env = dict(os.environ)
        for name in [n for n in full_env if n.startswith("QORE_QMOD_CACHE_")]:
            del full_env[name]
        full_env.update(env or {})
        res = subprocess.run([CMAKE, "--build", str(self.build)] + (["--target", target] if target else []),
                             capture_output=True, text=True, timeout=300, env=full_env)
        self.output = res.stdout + res.stderr
        if expect_rc is None:
            self.assertNotEqual(0, res.returncode, self.output)
        else:
            self.assertEqual(expect_rc, res.returncode, self.output)
        return sorted(self.compiles()[before:])

    def origins(self):
        rv = {}
        for name in ("Base", "Mid", "Top", "Other"):
            with open(str(self.qmod(name)) + qc.SIDECAR_SUFFIX) as f:
                rv[name] = json.load(f)["origin"]
        return rv

    def snapshot(self):
        rv = {}
        for name in ("Base", "Mid", "Top", "Other"):
            for suffix in qc.STORED_SUFFIXES:
                rv[name + suffix] = Path(str(self.qmod(name)) + suffix).read_bytes()
        return rv

    def verify(self):
        return subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                               str(self.source / "qcc"), "--require-sidecars"], capture_output=True, text=True,
                              timeout=60)

    def entries(self):
        return sorted((self.cache / qc.SCHEMA / "entries").glob("*/*.json"))

    def test_cold_warm_and_changed(self):
        all_modules = ["Base", "Mid", "Other", "Top"]
        self.assertEqual(all_modules, self.build_all())
        self.assertEqual({"Base": "miss", "Mid": "miss", "Top": "miss", "Other": "miss"}, self.origins())
        self.assertEqual(4, len(self.entries()))
        cold = self.snapshot()
        self.assertEqual(0, self.verify().returncode)

        # a new build tree: every module comes from the cache, with the same files
        self.assertEqual([], self.build_all())
        self.assertEqual({"Base": "hit", "Mid": "hit", "Top": "hit", "Other": "hit"}, self.origins())
        self.assertEqual(cold, self.snapshot())
        res = self.verify()
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertIn("4 supplied by the cache", res.stderr)
        # the build tree is current: nothing runs
        self.assertEqual([], self.build_all(fresh=False))

        # a change to Mid compiles Mid and Top, which requires it, and nothing else
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text(mid.read_text() + "# changed\n")
        self.assertEqual(["Mid", "Top"], self.build_all())
        self.assertEqual({"Base": "hit", "Mid": "miss", "Top": "miss", "Other": "hit"}, self.origins())
        # the same in an incremental build of the tree
        mid.write_text(mid.read_text() + "# again\n")
        self.assertEqual(["Mid", "Top"], self.build_all(fresh=False))

        # Base changes: Top requires Base only through Mid, and is compiled again too
        base = self.source / "qlib" / "Base.qm"
        base.write_text(base.read_text() + "# changed\n")
        self.assertEqual(["Base", "Mid", "Top"], self.build_all())
        self.assertEqual({"Base": "miss", "Mid": "miss", "Top": "miss", "Other": "hit"}, self.origins())

        # a source file of the module directory
        (self.source / "qlib" / "Top" / "Part.qc").write_text("class Part { int x; }\n")
        self.assertEqual(["Top"], self.build_all())
        # a new file in the module directory
        (self.source / "qlib" / "Top" / "New.qc").write_text("class New {}\n")
        self.assertEqual(["Top"], self.build_all())

        # the earlier sources again: the entries stored for them are used
        base.write_text("module Base {}\n")
        mid.write_text("%requires Base\nmodule Mid {}\n")
        (self.source / "qlib" / "Top" / "Part.qc").write_text("class Part {}\n")
        (self.source / "qlib" / "Top" / "New.qc").unlink()
        self.assertEqual([], self.build_all())
        self.assertEqual(cold, self.snapshot())

        # another AOT runtime identity changes the build context: every module is compiled
        (self.source / "identity.txt").write_text("2" * 64 + "\n")
        self.assertEqual(all_modules, self.build_all())

    def test_damaged_entry_is_a_miss(self):
        self.build_all()
        stored = self.cache / qc.SCHEMA / "objects"
        for obj in [p for p in stored.iterdir() if p.is_dir()]:
            qmod = obj / "qmod"
            qmod.write_bytes(qmod.read_bytes() + b"x")
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertIn("damaged", self.output)
        # the damaged entries were replaced
        self.assertEqual([], self.build_all())

    def test_optional_module_becomes_available(self):
        self.build_all()
        self.assertEqual([], self.build_all())
        # the module Other tried to load is available now: Other is compiled again; its new dependency is a module
        # source from outside the build, whose own dependencies are unknown, so it is not stored
        (self.extra_dir / "Opt.qm").write_text("module Opt {}\n")
        self.assertEqual(["Other"], self.build_all())
        self.assertEqual("uncacheable", self.origins()["Other"])
        self.assertEqual(["Other"], self.build_all())
        (self.extra_dir / "Opt.qm").unlink()
        self.assertEqual([], self.build_all())

    def test_module_shadowed_in_module_path(self):
        """A module that appears earlier in the module path would be loaded instead: the modules requiring it are
        compiled again"""
        self.build_all()
        self.assertEqual([], self.build_all())
        (self.extra_dir / "Base.qm").write_text("module Base { # another one\n}\n")
        self.assertEqual(["Mid", "Top"], self.build_all())
        self.assertEqual({"Base": "hit", "Mid": "uncacheable", "Top": "uncacheable", "Other": "hit"},
                         self.origins())
        (self.extra_dir / "Base.qm").unlink()
        self.assertEqual([], self.build_all())

    def test_include_is_not_stored(self):
        """A qcc that does not list the files a module includes in its depfile: a module with %include is not stored,
        and neither are the modules that require it"""
        (self.source / "qlib" / "part.ql").write_text("# part\n")
        base = self.source / "qlib" / "Base.qm"
        base.write_text("%include part.ql\nmodule Base {}\n")
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertEqual({"Base": "uncacheable", "Mid": "uncacheable", "Top": "uncacheable", "Other": "miss"},
                         self.origins())
        self.assertIn("which its depfile does not list", self.output)
        self.assertEqual(["Base", "Mid", "Top"], self.build_all())

    def test_include_listed_is_stored(self):
        """A qcc that lists included files: the module is stored, and a change to the included file is a miss"""
        (self.source / "list-includes.txt").write_text("1")
        part = self.source / "qlib" / "part.ql"
        part.write_text("# part\n")
        base = self.source / "qlib" / "Base.qm"
        base.write_text("%include part.ql\nmodule Base {}\n")
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertEqual({"Base": "miss", "Mid": "miss", "Top": "miss", "Other": "miss"}, self.origins())
        self.assertEqual([], self.build_all())
        part.write_text("# part changed\n")
        self.assertEqual(["Base", "Mid", "Top"], self.build_all())

    def test_reexported_module_shadowed_in_module_path(self):
        """A module required with %requires(reexport) is no dependency in the module metadata, but a module that
        appears for it earlier in the module path makes the modules requiring it, directly or not, misses too"""
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text("%requires(reexport) Base\nmodule Mid {}\n")
        self.build_all()
        self.assertEqual([], self.build_all())
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                              str(self.source / "qcc"), "--require-sidecars"], capture_output=True, text=True,
                             timeout=60)
        self.assertEqual(0, res.returncode, res.stderr)
        (self.extra_dir / "Base.qm").write_text("module Base { # another one\n}\n")
        self.assertEqual(["Mid", "Top"], self.build_all())
        self.assertIn("module 'Base' has other candidate files now", self.output)
        (self.extra_dir / "Base.qm").unlink()
        self.assertEqual([], self.build_all())

    def test_included_qm_file_is_no_module(self):
        """With the module names that qcc writes (metadata version 2), a file named <x>.qm that a module includes is
        no module x: a module x that appears in the module path does not make the module a miss"""
        (self.source / "list-includes.txt").write_text("1")
        (self.source / "qlib" / "helpers.qm").write_text("# helpers\n")
        base = self.source / "qlib" / "Base.qm"
        base.write_text("%include helpers.qm\nmodule Base {}\n")
        self.build_all()
        self.assertEqual([], self.build_all())
        (self.extra_dir / "helpers.qm").write_text("module helpers {}\n")
        self.assertEqual([], self.build_all())
        self.assertEqual(0, self.verify().returncode)

    def test_metadata_version_1_is_never_used(self):
        """A module whose metadata record has version 1 (a qcc before the closure was recorded) is never stored, and
        never a hit, even from an entry that names it with the right digests"""
        (self.source / "qami-v1.txt").write_text("1")
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertEqual({"Base": "uncacheable", "Mid": "uncacheable", "Top": "uncacheable", "Other": "uncacheable"},
                         self.origins())
        self.assertIn("metadata record of version 1", self.output)
        self.assertEqual([], self.entries())
        # an entry whose stored module has a version 1 record, with every digest right
        (self.source / "qami-v1.txt").unlink()
        self.build_all()
        def change(obj):
            q = obj / "qmod"
            data = q.read_bytes()
            pos = data.index(b"QAMI")
            name_len = struct.unpack_from("<I", data, pos + 12)[0]
            end = pos + 16 + name_len
            for _ in range(2):
                count = struct.unpack_from("<I", data, end)[0]
                end += 4
                for _ in range(count):
                    end += 4 + struct.unpack_from("<I", data, end)[0]
            q.write_bytes(data[:pos] + b"QAMI" + struct.pack("<II", 1, end - pos - 12) + data[pos + 12:end])
        entry_path, entry = self.tamper("Base", change)
        entry_path.write_text(json.dumps(entry))
        self.assertEqual(["Base"], self.build_all())
        self.assertRegex(self.output, r"stored module: module '[^']*' has a metadata record of version 1; the cache "
                                      r"uses only version 2")

    def test_module_path_in_v2_metadata_is_not_stored(self):
        """A module whose metadata records module path directives of its own is not stored"""
        (self.source / "qami-paths.txt").write_text("1")
        self.build_all()
        self.assertEqual("uncacheable", self.origins()["Base"])
        self.assertIn("the module changes the module path", self.output)

    def test_reexported_module_in_v2_metadata(self):
        """A module that a module re-exports is in the names that qcc writes (metadata version 2), also when it is
        reached through another module: one that appears earlier in the module path makes the modules misses"""
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text("%requires(reexport) Base\nmodule Mid {}\n")
        self.build_all()
        self.assertEqual([], self.build_all())
        self.assertEqual(0, self.verify().returncode)
        top = json.loads(Path(str(self.qmod("Top")) + qc.SIDECAR_SUFFIX).read_text())
        self.assertEqual(["Base", "Mid"], [r["name"] for r in top["entry"]["resolution"]])
        (self.extra_dir / "Base.qm").write_text("module Base { # another one\n}\n")
        self.assertEqual(["Mid", "Top"], self.build_all())
        (self.extra_dir / "Base.qm").unlink()
        self.assertEqual([], self.build_all())

    def test_source_changed_during_compile_is_not_stored(self):
        (self.source / "slow.txt").write_text("1")
        self.build_all()
        self.assertEqual(0, len(self.entries()))
        self.assertIn("changed while the module was compiled", self.output)
        (self.source / "slow.txt").unlink()

    def test_verify_finds_changed_modules(self):
        self.build_all()
        self.build_all()
        self.assertEqual(0, self.verify().returncode)
        # a supplied module that was replaced afterwards
        qmod = self.qmod("Mid")
        data = qmod.read_bytes()
        qmod.write_bytes(data.replace(b"FAKEQMOD", b"OTHERMOD"))
        res = self.verify()
        self.assertEqual(1, res.returncode)
        self.assertIn("not the module the cache supplied", res.stderr)
        qmod.write_bytes(data)
        # a module without the build's identity
        qmod.write_bytes(data.replace(b"1" * 64, b"3" * 64))
        res = self.verify()
        self.assertEqual(1, res.returncode)
        self.assertIn("AOT runtime identity", res.stderr)
        qmod.write_bytes(data)
        # the fingerprints or the depfile of a supplied module changed afterwards, even where the depfile still lists
        # the same files: the loader decides from them whether the module is current
        for suffix, change in ((".d.fingerprints", lambda t: t.replace(" 0000000000000000 ", " 0000000000000001 ", 1)),
                               (".d", lambda t: t + "\n")):
            side = Path(str(qmod) + suffix)
            text = side.read_text()
            side.write_text(change(text))
            self.assertNotEqual(text, side.read_text())
            res = self.verify()
            self.assertEqual(1, res.returncode, suffix)
            self.assertIn("its %s is not the one the cache supplied" % suffix, res.stderr)
            side.write_text(text)
        self.assertEqual(0, self.verify().returncode)
        # a dependency changed after the module was supplied
        base = self.source / "qlib" / "Base.qm"
        base.write_text(base.read_text() + "#\n")
        res = self.verify()
        self.assertEqual(1, res.returncode)
        self.assertIn("has changed since the module was supplied", res.stderr)
        base.write_text(base.read_text()[:-2])
        self.assertEqual(0, self.verify().returncode)
        # qcc reports another identity than the one the modules were built for
        (self.source / "identity.txt").write_text("4" * 64 + "\n")
        res = self.verify()
        self.assertEqual(1, res.returncode)
        self.assertIn("identities of the build differ", res.stderr)

    def test_audit(self):
        self.build_all()
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "1"}))
        self.assertEqual(4, self.output.count("audited: qcc wrote the same module"))
        self.assertEqual({"Base": "hit", "Mid": "hit", "Top": "hit", "Other": "hit"}, self.origins())
        # qcc writes another module: the audit fails the build
        (self.source / "nondeterministic.txt").write_text("1")
        self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "1"}, expect_rc=None)
        self.assertIn("AUDIT FAILED", self.output)
        (self.source / "nondeterministic.txt").unlink()
        self.build_all()
        # a sample of none audits nothing
        self.assertEqual([], self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "0"}))

    def test_disabled_and_readonly(self):
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env={"QORE_QMOD_CACHE_READONLY": "1"}))
        self.assertEqual(0, len(self.entries()))
        self.build_all()
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env={"QORE_QMOD_CACHE_DISABLE": "1"}))

    def test_prune(self):
        self.build_all()
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text(mid.read_text() + "# changed\n")
        self.build_all()
        self.assertEqual(6, len(self.entries()))
        # generous limits remove nothing: another job's entries stay
        res = subprocess.run([sys.executable, str(TOOL), "prune", "--cache-dir", str(self.cache), "--max-age-days",
                              "7", "--max-size", "1000"], capture_output=True, text=True, timeout=60)
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertEqual(6, len(self.entries()))
        self.assertEqual(6, len([p for p in (self.cache / qc.SCHEMA / "objects").iterdir() if p.is_dir()]))
        self.assertEqual([], self.build_all())
        res = subprocess.run([sys.executable, str(TOOL), "prune", "--cache-dir", str(self.cache), "--max-size", "0"],
                             capture_output=True, text=True, timeout=60)
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertEqual(0, len(self.entries()))

    def tamper(self, name, change):
        """Changes the stored files of a module's entry with @p change(object dir) and makes the entry's file digests
        match them again, as someone who can write the cache could"""
        key = self.keys()[name]
        cache = qc.Cache(str(self.cache))
        obj = Path(cache.object_dir(key))
        change(obj)
        entry_path = [e for e in self.entries() if e.stem == key][0]
        entry = json.loads(entry_path.read_text())
        for suffix in qc.STORED_SUFFIXES:
            entry["files"][suffix or "qmod"] = qc.sha256_file(str(obj / ("qmod" + suffix)))
        return entry_path, entry

    def test_malformed_entries_are_misses(self):
        """An entry that is valid JSON but not an entry this tool wrote is a miss and is removed; the build never
        fails for it"""
        self.build_all()
        paths = self.entries()
        self.assertEqual(4, len(paths))
        def deps_null(e):
            e["deps"] = None
        def deps_strings(e):
            e["deps"] = ["x"]
        def files_list(e):
            e["files"] = []
        def resolution_dict(e):
            e["resolution"] = {"a": 1}
        mutators = [deps_null, deps_strings, files_list, resolution_dict]
        for i, path in enumerate(paths):
            if i % 2 == 0:
                path.write_text(["[]", "\"x\"", "{}", "{\"broken"][i // 2 % 4])
            else:
                entry = json.loads(path.read_text())
                mutators[i // 2 % 4](entry)
                path.write_text(json.dumps(entry))
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertNotIn("Traceback", self.output)
        # the malformed entries are gone, and the new ones are used
        self.assertEqual([], self.build_all())
        for path in paths:
            if path.exists():
                self.assertIsNone(qc.entry_problem(json.loads(path.read_text()), path.stem))

    def test_unreadable_entries_are_misses(self):
        """An entry that raises any exception when it is read (here JSON nested deeper than the parser's recursion
        limit) is a miss and is removed; neither the lookup nor the store after a compile fails the build"""
        self.build_all()
        nested = "[" * 200000 + "]" * 200000
        planted = []
        for path in self.entries():
            p = path.with_name("d" * 64 + ".json")
            p.write_text(nested)
            planted.append(p)
        # every module is still found (the nested entries are removed while the lookup lists the entries)
        self.assertEqual([], self.build_all())
        self.assertNotIn("Traceback", self.output)
        self.assertFalse([p for p in planted if p.exists()])
        # a compile that stores its module next to such an entry
        for p in planted:
            p.write_text(nested)
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text(mid.read_text() + "# changed\n")
        self.assertEqual(["Mid", "Top"], self.build_all())
        self.assertNotIn("Traceback", self.output)
        self.assertEqual("miss", self.origins()["Mid"])

    def test_symbolic_links_are_not_followed(self):
        """A symbolic link in the cache (an entry, a base key directory, an object directory or a stored file) is
        never followed: the module is a miss, and the link's target is neither read nor removed"""
        self.build_all()
        keys = self.keys()
        cache = qc.Cache(str(self.cache))
        elsewhere = self.root / "elsewhere"
        elsewhere.mkdir()
        def entry_of(name):
            return [e for e in self.entries() if e.stem == keys[name]][0]
        # Base: the entry is a link to a copy of itself
        e = entry_of("Base")
        shutil.copy(str(e), str(elsewhere / "Base.json"))
        e.unlink()
        e.symlink_to(elsewhere / "Base.json")
        # Mid: the object directory is a link to a copy of itself
        obj = Path(cache.object_dir(keys["Mid"]))
        shutil.copytree(str(obj), str(elsewhere / "Mid"))
        shutil.rmtree(str(obj))
        obj.symlink_to(elsewhere / "Mid")
        # Top: the stored module is a link to a copy of itself
        obj = Path(cache.object_dir(keys["Top"]))
        shutil.copy(str(obj / "qmod"), str(elsewhere / "Top.qmod"))
        (obj / "qmod").unlink()
        (obj / "qmod").symlink_to(elsewhere / "Top.qmod")
        # Other: the base key directory is a link to a copy of itself
        base_dir = entry_of("Other").parent
        shutil.copytree(str(base_dir), str(elsewhere / "Other"))
        shutil.rmtree(str(base_dir))
        base_dir.symlink_to(elsewhere / "Other")
        before = sorted((str(p), p.read_bytes()) for p in elsewhere.rglob("*") if p.is_file())
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        self.assertNotIn("Traceback", self.output)
        self.assertEqual(before, sorted((str(p), p.read_bytes()) for p in elsewhere.rglob("*") if p.is_file()))
        # the links were replaced by what the compiles stored
        self.assertEqual([], self.build_all())
        for p in (self.cache / qc.SCHEMA).rglob("*"):
            self.assertFalse(p.is_symlink(), p)
        self.prune("--max-size", "0")
        self.assertEqual(before, sorted((str(p), p.read_bytes()) for p in elsewhere.rglob("*") if p.is_file()))

    def test_changed_binary_module_dependency(self):
        self.build_all()
        self.assertEqual([], self.build_all())
        (self.extra_dir / "binmod-api-1.0.qmod").write_bytes(b"\x7fELF binary module 2\n")
        self.assertEqual(["Other"], self.build_all())
        self.assertIn("binmod-api-1.0.qmod' has changed", self.output)

    def test_hit_needs_the_build_identity(self):
        self.build_all()
        def change(obj):
            q = obj / "qmod"
            q.write_bytes(q.read_bytes().replace(b"1" * 64, b"5" * 64))
        entry_path, entry = self.tamper("Mid", change)
        entry_path.write_text(json.dumps(entry))
        self.assertEqual(["Mid"], self.build_all())
        self.assertIn("lacks the AOT runtime identity", self.output)

    def test_hit_needs_the_depfile_target(self):
        self.build_all()
        def change(obj):
            d = obj / "qmod.d"
            text = d.read_text()
            d.write_text("/elsewhere/Mid.qmod" + text[text.index(":"):])
        entry_path, entry = self.tamper("Mid", change)
        entry_path.write_text(json.dumps(entry))
        self.assertEqual(["Mid"], self.build_all())
        self.assertIn("stored dependency file names '/elsewhere/Mid.qmod'", self.output)

    def test_hit_needs_the_entry_key(self):
        self.build_all()
        key = self.keys()["Top"]
        entry_path = [e for e in self.entries() if e.stem == key][0]
        entry = json.loads(entry_path.read_text())
        # an entry that leaves out a dependency still matches the build tree, but is not the entry of its key
        entry["deps"] = [d for d in entry["deps"] if d["kind"] != "module"]
        entry_path.write_text(json.dumps(entry))
        self.assertEqual(["Top"], self.build_all())
        self.assertIn("entry key mismatch", self.output)

    def test_audit_compares_dependencies(self):
        self.build_all()
        # qcc writes the same module, but finds another dependency
        (self.source / "extra-dep.txt").write_text(str(self.source / "qlib" / "Other.qm"))
        self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "1"}, expect_rc=None)
        self.assertIn("has other dependencies than the cache entry", self.output)

    def test_restored_files_are_checked(self):
        self.build_all()
        for damaged in ("1", ".d", ".d.fingerprints"):
            self.assertEqual(["Base", "Mid", "Other", "Top"],
                             self.build_all(env={"QORE_QMOD_CACHE_TEST_CORRUPT_RESTORE": damaged}), damaged)
            self.assertIn("differs from the cache", self.output)
            self.assertEqual({"Base": "miss", "Mid": "miss", "Top": "miss", "Other": "miss"}, self.origins())

    def test_module_changed_in_the_build_tree_is_not_a_dependency_key(self):
        """A module's key in the build tree's index is used only while its qmod is the one the cache supplied"""
        self.build_all()
        self.assertEqual([], self.build_all())
        # Mid's qmod changes behind the cache's back; Top is compiled again, and not stored
        mid = self.qmod("Mid")
        mid.write_bytes(mid.read_bytes() + b"changed")
        for p in self.qmod("Top").parent.glob("Top.qmod*"):
            p.unlink()
        self.assertEqual(["Top"], self.build_all(fresh=False, target="Top-qmod"))
        with open(str(self.qmod("Top")) + qc.SIDECAR_SUFFIX) as f:
            self.assertEqual("uncacheable", json.load(f)["origin"])

    def test_failed_audit_quarantines_the_cache(self):
        self.build_all()
        (self.source / "nondeterministic.txt").write_text("1")
        self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "1"}, expect_rc=None)
        self.assertIn("AUDIT FAILED", self.output)
        self.assertIn("quarantined the cache", self.output)
        # the next build reuses nothing of the cache whose key missed an input
        (self.source / "nondeterministic.txt").unlink()
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all())
        # a prune keeps a quarantined cache while a job can still be reading it, however old the directory's
        # modification time (a rename does not change it)
        quarantined = list(self.cache.glob(qc.SCHEMA + ".quarantine.*"))
        self.assertEqual(1, len(quarantined))
        os.utime(str(quarantined[0]), (time.time() - 2 * qc.ORPHAN_AGE,) * 2)
        self.prune("--max-size", "1000")
        self.assertTrue(quarantined[0].exists())
        # and removes it then: the time is in its name
        old = quarantined[0].with_name(re.sub(r"\.[0-9]+$", ".%d" % (time.time() - 2 * qc.ORPHAN_AGE),
                                              quarantined[0].name))
        quarantined[0].rename(old)
        out = self.prune("--max-size", "1000")
        self.assertIn("1 quarantined caches", out)
        self.assertFalse(list(self.cache.glob(qc.SCHEMA + ".quarantine.*")))

    def test_module_path_directive_is_not_stored(self):
        base = self.source / "qlib" / "Base.qm"
        base.write_text("%prepend-module-path $HOME/mods\nmodule Base {}\n")
        self.build_all()
        self.assertEqual({"Base": "uncacheable", "Mid": "uncacheable", "Top": "uncacheable", "Other": "miss"},
                         self.origins())
        self.assertIn("changes the module path", self.output)

    def test_module_path_directive_in_include_file_is_not_stored(self):
        (self.source / "list-includes.txt").write_text("1")
        (self.source / "qlib" / "part.ql").write_text("%append-module-path /elsewhere\n")
        (self.source / "qlib" / "Base.qm").write_text("%include part.ql\nmodule Base {}\n")
        self.build_all()
        self.assertEqual({"Base": "uncacheable", "Mid": "uncacheable", "Top": "uncacheable", "Other": "miss"},
                         self.origins())
        self.assertIn("part.ql' changes the module path", self.output)

    def key_pair(self, name):
        keys = ed25519_keys(self.root, name)
        if not keys:
            self.skipTest("OpenSSL 3 is required for signed entries")
        return keys

    def test_signed_entries(self):
        priv1, pub1 = self.key_pair("k1")
        priv2, pub2 = self.key_pair("k2")
        writer = {"QORE_QMOD_CACHE_VERIFY_KEY": pub1, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1"}
        self.build_all(env=writer)
        # unsigned until the build is signed: no job uses them
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env=writer))
        self.assertIn("stored (signed after the build)", self.output)
        res = sign_build(self.cache, self.build, priv1)
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertIn("signed 4 entries that this build stored", res.stderr)
        self.assertEqual([], self.build_all(env=writer))
        # a job of another ref verifies with the public key: every entry is used, and nothing is written
        before = sorted((str(p), p.stat().st_mtime_ns) for p in self.cache.rglob("*"))
        self.assertEqual([], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub1}))
        self.assertEqual({"Base": "hit", "Mid": "hit", "Top": "hit", "Other": "hit"}, self.origins())
        self.assertEqual(before, sorted((str(p), p.stat().st_mtime_ns) for p in self.cache.rglob("*")))
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                              str(self.source / "qcc")], capture_output=True, text=True, timeout=60,
                             env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY=pub1))
        self.assertEqual(0, res.returncode, res.stderr)
        # the guard refuses entries another key signed
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                              str(self.source / "qcc")], capture_output=True, text=True, timeout=60,
                             env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY=pub2))
        self.assertEqual(1, res.returncode, res.stderr)
        self.assertIn("entry not signed with the signing key", res.stderr)
        # an entry changed by someone without the private key: a reader does not use it
        def change(obj):
            q = obj / "qmod"
            q.write_bytes(q.read_bytes().replace(b"FAKEQMOD", b"EVILQMOD"))
        entry_path, entry = self.tamper("Mid", change)
        entry_path.write_text(json.dumps(entry))
        self.assertEqual(["Mid"], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub1}))
        self.assertIn("entry not signed with the signing key", self.output)
        self.assertNotIn(b"EVILQMOD", self.qmod("Mid").read_bytes())
        # nor does a writer, which replaces it
        self.assertEqual(["Mid"], self.build_all(env=writer))
        self.assertEqual(0, sign_build(self.cache, self.build, priv1).returncode)
        self.assertEqual([], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub1}))
        # entries written with another key, or without one, are not used
        self.assertEqual(["Base", "Mid", "Other", "Top"],
                         self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub2}))
        unsigned = self.root / "unsigned-cache"
        self.build_all(cache=unsigned)
        self.assertEqual(["Base", "Mid", "Other", "Top"],
                         self.build_all(cache=unsigned, env={"QORE_QMOD_CACHE_VERIFY_KEY": pub1}))

    def test_quarantined_entries_cannot_be_replayed(self):
        """A signed entry of a quarantined cache, put back into the cache that replaced it, is not used: the signature
        covers the cache's epoch, which the quarantine renewed"""
        priv, pub = self.key_pair("k")
        writer = {"QORE_QMOD_CACHE_VERIFY_KEY": pub, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1"}
        reader = {"QORE_QMOD_CACHE_VERIFY_KEY": pub}
        # a qcc that writes another module every time: the audit of the next build fails and quarantines the cache
        (self.source / "nondeterministic.txt").write_text("1")
        self.build_all(env=writer)
        self.assertEqual(0, sign_build(self.cache, self.build, priv).returncode)
        bad = self.qmod("Mid").read_bytes()
        keys = self.keys()
        self.build_all(env=dict(writer, QORE_QMOD_CACHE_AUDIT="1"), expect_rc=None)
        self.assertIn("AUDIT FAILED", self.output)
        quarantined = list(self.cache.glob(qc.SCHEMA + ".quarantine.*"))
        self.assertEqual(1, len(quarantined))
        (self.source / "nondeterministic.txt").unlink()
        self.build_all(env=writer)
        self.assertEqual(0, sign_build(self.cache, self.build, priv).returncode)
        self.assertEqual([], self.build_all(env=reader))
        # Mid's signed entry and object of the quarantined cache, put back
        old = qc.Cache(str(self.cache))
        old.root = str(quarantined[0])
        old.entries = os.path.join(old.root, "entries")
        old.objects = os.path.join(old.root, "objects")
        old_entry = [p for p in Path(old.entries).glob("*/*.json") if p.stem == keys["Mid"]][0]
        new_entry = [p for p in self.entries() if p.stem == keys["Mid"]][0]
        shutil.copy(str(old_entry), str(new_entry))
        new_obj = Path(qc.Cache(str(self.cache)).object_dir(keys["Mid"]))
        shutil.rmtree(str(new_obj))
        shutil.copytree(old.object_dir(keys["Mid"]), str(new_obj))
        self.assertEqual(["Mid"], self.build_all(env=reader))
        self.assertIn("another epoch", self.output)
        self.assertNotEqual(bad, self.qmod("Mid").read_bytes())

    def test_public_key_as_text_or_file(self):
        """QORE_QMOD_CACHE_VERIFY_KEY is the public key's PEM text (a CI variable of type env_var: several lines, with
        or without a final newline) or the path of a file with it (type file); exec, verify and signer-check take
        both, and an entry signed with the private key is a hit with each"""
        priv, pub = self.key_pair("k")
        pub_file = self.root / "k.pub.pem"
        self.assertEqual(pub, pub_file.read_text())
        self.assertGreater(len(pub.strip().splitlines()), 2)
        writer = {"QORE_QMOD_CACHE_VERIFY_KEY": pub, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1"}
        self.build_all(env=writer)
        self.assertEqual(0, sign_build(self.cache, self.build, priv).returncode)
        for value in (pub, pub.rstrip("\n"), str(pub_file)):
            self.assertEqual([], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": value}))
            res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                                  str(self.source / "qcc"), "--require-sidecars"], capture_output=True, text=True,
                                 timeout=60, env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY=value))
            self.assertEqual(0, res.returncode, res.stderr)
            self.assertIn("4 supplied by the cache", res.stderr)
            res = subprocess.run([sys.executable, str(TOOL), "signer-check", "--verify-only"], capture_output=True,
                                 text=True, timeout=60, env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY=value))
            self.assertEqual(0, res.returncode, res.stderr)
            # and as the public key of the private key, which protected jobs check
            res = subprocess.run([sys.executable, str(TOOL), "signer-check"], input=priv + "\n", capture_output=True,
                                 text=True, timeout=60, env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY=value))
            self.assertEqual(0, res.returncode, res.stderr)
            self.assertEqual(pub, res.stdout)
        # another key's text is no match
        _, other = self.key_pair("other")
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": other}))

    def test_keys_without_openssl(self):
        _, pub = self.key_pair("k")
        self.build_all()
        # with a key but no OpenSSL 3, nothing is taken from the cache nor written to it
        env = {"QORE_QMOD_CACHE_VERIFY_KEY": pub, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1",
               "QORE_QMOD_CACHE_OPENSSL": "/nonexistent/openssl"}
        entries = self.entries()
        self.assertEqual(["Base", "Mid", "Other", "Top"], self.build_all(env=env))
        self.assertIn("signed entries need OpenSSL 3", self.output)
        self.assertEqual(entries, self.entries())
        # the guard of such a build: the cache was cold, which is no failure
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                              str(self.source / "qcc"), "--require-sidecars"], capture_output=True, text=True,
                             timeout=60, env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY="pub",
                                                  QORE_QMOD_CACHE_OPENSSL="/nonexistent/openssl"))
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertIn("no OpenSSL 3 to check signatures: the build ran without the cache", res.stderr)
        # but a module that the cache supplied, which cannot be checked now, fails it
        self.build_all()
        self.assertEqual([], self.build_all())
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build), "--qcc",
                              str(self.source / "qcc")], capture_output=True, text=True, timeout=60,
                             env=dict(os.environ, QORE_QMOD_CACHE_VERIFY_KEY="pub",
                                      QORE_QMOD_CACHE_OPENSSL="/nonexistent/openssl"))
        self.assertEqual(1, res.returncode, res.stderr)
        self.assertIn("its signature cannot be checked without OpenSSL 3", res.stderr)

    def test_qcc_gets_no_cache_settings(self):
        _, pub = self.key_pair("k")
        self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1",
                            "QORE_QMOD_CACHE_AUDIT": "0"})
        self.assertFalse((self.source / "qcc-env.txt").exists(),
                         (self.source / "qcc-env.txt").read_text() if (self.source / "qcc-env.txt").exists() else "")

    def test_private_key_in_the_build_environment(self):
        """The private key is only for "sign", after the build: a build with it in its environment (a script that did
        not take it out) uses no cache, and qcc does not get it"""
        priv, pub = self.key_pair("k")
        self.build_all()
        self.assertEqual(["Base", "Mid", "Other", "Top"],
                         self.build_all(env={"QORE_QMOD_CACHE_SIGNING_KEY": priv, "QORE_QMOD_CACHE_VERIFY_KEY": pub}))
        self.assertIn("QORE_QMOD_CACHE_SIGNING_KEY is in the environment of the build", self.output)
        self.assertFalse((self.source / "qcc-env.txt").exists())

    def test_sign_only_what_the_build_stored(self):
        """"sign" signs an entry only when the cache has the entry and files that the build stored; a private key
        that is no file is refused"""
        priv, pub = self.key_pair("k")
        writer = {"QORE_QMOD_CACHE_VERIFY_KEY": pub, "QORE_QMOD_CACHE_SIGN_AFTER_BUILD": "1"}
        self.build_all(env=writer)
        keys = self.keys()
        # another entry for Mid's key, and another module for Top's
        entry_path = [e for e in self.entries() if e.stem == keys["Mid"]][0]
        entry = json.loads(entry_path.read_text())
        entry["env_names"] = entry["env_names"] + ["X"]
        entry_path.write_text(json.dumps(entry))
        top = Path(qc.Cache(str(self.cache)).object_dir(keys["Top"])) / "qmod"
        top.write_bytes(top.read_bytes() + b"x")
        with open(priv) as f:
            pem = f.read()
        res = sign_build(self.cache, self.build, pem.splitlines()[0])
        self.assertEqual(1, res.returncode)
        self.assertIn("the private key is not a file", res.stderr)
        self.assertNotIn(pem.splitlines()[1], res.stderr)
        res = sign_build(self.cache, self.build, priv)
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertIn("signed 2 entries that this build stored, 2 failed", res.stderr)
        self.assertIn("the cache does not have the entry this build stored", res.stderr)
        self.assertIn("stored file 'qmod' is not the one this build stored", res.stderr)
        self.assertEqual(["Mid", "Top"], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub}))

    def test_ci_scripts_never_show_the_private_key(self):
        """A job of a protected ref, run with "set -x" from setup to finish: neither the trace nor the environment of
        any process of the build (qcc, the build tools) nor of openssl has the private key or its file path; the
        entries the build stored are signed after the guard"""
        priv, pub = self.key_pair("k")
        with open(priv) as f:
            pem = f.read()
        material = [line for line in pem.splitlines() if line and not line.startswith("-----")]
        self.assertTrue(material)
        mount = self.root / "mount"
        (mount / "protected" / "build-x").mkdir(parents=True)
        real = qc.find_openssl(os.environ)
        log_dir = self.root / "openssl-log"
        log_dir.mkdir()
        wrapper = self.root / "openssl-wrapper"
        wrapper.write_text('#!/bin/sh\nenv > "%s/env.$$"\necho "$@" > "%s/argv.$$"\nexec "%s" "$@"\n'
                           % (log_dir, log_dir, real))
        wrapper.chmod(0o755)
        (self.source / "probe-env.txt").write_text("1")
        script = self.root / "job.sh"
        script.write_text("""set -ex
. "{script}"
qore_qmod_cache_setup "{root}"
cmake -G "Unix Makefiles" -S "{source}" -B "{build}" "-DPython3_EXECUTABLE={python}" ${{QMOD_CACHE_ARG:+"${{QMOD_CACHE_ARG}}"}}
ln -sf "{source}/qcc" "{build}/qcc"
cmake --build "{build}"
env > "{after}"
qore_qmod_cache_finish "{root}" "{build}"
env > "{finished}"
""".format(script=CiScriptTest.SCRIPT, root=ROOT, source=self.source, build=self.build, python=sys.executable,
           after=self.root / "env-after-build.txt", finished=self.root / "env-after-finish.txt"))
        env = {k: v for k, v in os.environ.items() if not k.startswith(("QORE_QMOD_CACHE_", "CI_"))}
        env.update({"QORE_QMOD_CACHE_DIR": str(mount), "CI_COMMIT_REF_PROTECTED": "true", "CI_JOB_NAME": "build-x",
                    "CI_PROJECT_DIR": str(self.root), "QORE_QMOD_CACHE_SIGNING_KEY": priv,
                    "QORE_QMOD_CACHE_OPENSSL": str(wrapper)})
        for shell in [sh for sh in ("sh", "dash", "bash") if shutil.which(sh)]:
            shutil.rmtree(str(self.build), ignore_errors=True)
            for p in log_dir.iterdir():
                p.unlink()
            res = subprocess.run([shell, str(script)], capture_output=True, text=True, timeout=600, env=env)
            out = res.stdout + res.stderr
            self.assertEqual(0, res.returncode, out)
            self.assertIn("qmod-cache: signed 4 entries that this build stored", out)
            for text, what in [(out, "the trace of " + shell)] + [
                    (p.read_text(), p.name) for p in [self.root / "env-after-build.txt",
                                                      self.root / "env-after-finish.txt"]
                    + sorted(log_dir.glob("env.*"))]:
                self.assertNotIn(priv, text, what)
                for line in material:
                    self.assertNotIn(line, text, what)
            # openssl got the key's path as an argument, and only to sign
            argv = [p.read_text() for p in log_dir.glob("argv.*")]
            self.assertTrue([a for a in argv if "-sign" in a and priv in a], argv)
            self.assertFalse([a for a in argv if priv in a and "-sign" not in a and "pkey -in" not in a], argv)
            # no process of the build got the key's path
            self.assertFalse((self.source / "qcc-env.txt").exists(),
                             (self.source / "qcc-env.txt").read_text() if (self.source / "qcc-env.txt").exists()
                             else "")
            # the entries are signed: a job of another ref uses them
            shutil.rmtree(str(self.build), ignore_errors=True)
            self.assertEqual([], self.build_all(env={"QORE_QMOD_CACHE_VERIFY_KEY": pub},
                                                cache=mount / "protected" / "build-x"))
            shutil.rmtree(str(mount / "protected" / "build-x" / qc.SCHEMA))

    def test_read_cache_of_a_trusted_level(self):
        protected = self.root / "protected"
        unprotected = self.root / "unprotected"
        self.build_all(cache=protected)
        before = sorted((str(p), p.stat().st_mtime_ns) for p in protected.rglob("*"))
        # a job of an unprotected ref reads the protected cache, writes its own, and never writes the protected one
        self.assertEqual([], self.build_all(cache=unprotected, read_dirs=[protected]))
        self.assertEqual({"Base": "hit", "Mid": "hit", "Top": "hit", "Other": "hit"}, self.origins())
        mid = self.source / "qlib" / "Mid.qm"
        mid.write_text(mid.read_text() + "# changed\n")
        self.assertEqual(["Mid", "Top"], self.build_all(cache=unprotected, read_dirs=[protected]))
        self.assertEqual(before, sorted((str(p), p.stat().st_mtime_ns) for p in protected.rglob("*")))
        self.assertEqual(2, len(list((unprotected / qc.SCHEMA / "entries").glob("*/*.json"))))
        # and a damaged entry of the protected cache is not removed by such a job
        key = json.loads(Path(str(self.qmod("Base")) + qc.SIDECAR_SUFFIX).read_text())["key"]
        (Path(qc.Cache(str(protected)).object_dir(key)) / "qmod").write_bytes(b"damaged")
        self.assertEqual(["Base"], self.build_all(cache=unprotected, read_dirs=[protected]))
        self.assertTrue(os.path.isdir(qc.Cache(str(protected)).object_dir(key)))

    def test_prune_leaves_work_in_progress(self):
        """Prune never touches a store in progress: a young temporary directory, an object whose entry is not
        written yet, or one that a job holds"""
        import fcntl
        self.build_all()
        cache = qc.Cache(str(self.cache))
        objects = Path(cache.objects)
        young_tmp = objects / ".tmp.young"
        young_tmp.mkdir()
        old_tmp = objects / ".tmp.old"
        old_tmp.mkdir()
        os.utime(str(old_tmp), (time.time() - 2 * qc.ORPHAN_AGE,) * 2)
        young_obj = objects / ("a" * 64)
        young_obj.mkdir()
        held_obj = objects / ("b" * 64)
        held_obj.mkdir()
        os.utime(str(held_obj), (time.time() - 2 * qc.ORPHAN_AGE,) * 2)
        fd = os.open(cache.lock_path("b" * 64), os.O_RDWR | os.O_CREAT)
        os.utime(cache.lock_path("b" * 64), (time.time() - 2 * qc.ORPHAN_AGE,) * 2)
        try:
            fcntl.flock(fd, fcntl.LOCK_SH)
            self.prune("--max-size", "1000")
            self.assertTrue(young_tmp.is_dir())
            self.assertFalse(old_tmp.exists())
            self.assertTrue(young_obj.is_dir())
            self.assertTrue(held_obj.is_dir())
        finally:
            os.close(fd)
        self.prune("--max-size", "1000")
        self.assertFalse(held_obj.exists())
        # the entries of the build are all still there
        self.assertEqual(4, len(self.entries()))

    def test_another_jobs_prune_keeps_this_jobs_entries(self):
        self.build_all()
        other = self.root / "build-other"
        res = subprocess.run([sys.executable, str(TOOL), "prune", "--cache-dir", str(self.cache), "--build-dir",
                              str(other), "--max-age-days", "7", "--max-size", "1000"], capture_output=True,
                             text=True, timeout=60)
        self.assertEqual(0, res.returncode, res.stderr)
        self.assertEqual([], self.build_all())

    def prune(self, *args):
        res = subprocess.run([sys.executable, str(TOOL), "prune", "--cache-dir", str(self.cache)] + list(args),
                             capture_output=True, text=True, timeout=120)
        self.assertEqual(0, res.returncode, res.stdout + res.stderr)
        return res.stdout

    def keys(self):
        """The cache key of each module of the build tree"""
        rv = {}
        for name in ("Base", "Mid", "Top", "Other"):
            with open(str(self.qmod(name)) + qc.SIDECAR_SUFFIX) as f:
                rv[name] = json.load(f)["key"]
        return rv

    def entry_keys(self):
        return sorted(p.stem for p in self.entries())

    def set_last_use(self, key, when):
        cache = qc.Cache(str(self.cache))
        for p in [Path(cache.lock_path(key))] + [e for e in self.entries() if e.stem == key]:
            os.utime(str(p), (when, when))

    def test_lru_and_age(self):
        self.build_all()
        keys = self.keys()
        now = time.time()
        # Top was used last 4000 s ago, Mid 3000 s, Base 2000 s, Other 1000 s
        for i, name in enumerate(("Other", "Base", "Mid", "Top")):
            self.set_last_use(keys[name], now - 1000 * (i + 1))
        # a hit makes an entry the most recently used one: Top
        shutil.rmtree(str(self.build))
        self.build_all()
        cache = qc.Cache(str(self.cache))
        self.assertGreater(os.stat(cache.lock_path(keys["Top"])).st_mtime, now - 60)
        for i, name in enumerate(("Other", "Base", "Mid")):
            self.set_last_use(keys[name], now - 1000 * (i + 1))
        self.set_last_use(keys["Top"], now)
        # the cap leaves room for two entries: the two least recently used ones go (Mid, then Base)
        entries = {e[2]: e for e in qc.cache_entries(cache)}
        total = qc.dir_size(cache.root)
        cap = total - entries[keys["Mid"]][3] - entries[keys["Base"]][3] + 1
        out = self.prune("--max-size", str(cap / 1048576.0))
        self.assertIn("2 size", out)
        self.assertEqual(sorted([keys["Other"], keys["Top"]]), self.entry_keys())
        # an entry unused for longer than the maximum age goes
        self.set_last_use(keys["Other"], now - 8 * 86400)
        out = self.prune("--max-age-days", "7")
        self.assertIn("1 age", out)
        self.assertEqual([keys["Top"]], self.entry_keys())
        # nothing else
        self.prune("--max-age-days", "7", "--max-size", "1000")
        self.assertEqual([keys["Top"]], self.entry_keys())

    def test_prune_never_removes_an_entry_being_read(self):
        import fcntl
        self.build_all()
        keys = self.keys()
        cache = qc.Cache(str(self.cache))
        # a job reading Mid's entry holds a shared lock on it
        fd = os.open(cache.lock_path(keys["Mid"]), os.O_RDWR)
        try:
            fcntl.flock(fd, fcntl.LOCK_SH)
            out = self.prune("--max-size", "0")
            self.assertIn("1 skipped (in use)", out)
            self.assertEqual([keys["Mid"]], self.entry_keys())
            self.assertTrue(os.path.isdir(cache.object_dir(keys["Mid"])))
        finally:
            os.close(fd)
        out = self.prune("--max-size", "0")
        self.assertEqual([], self.entry_keys())
        self.assertNotIn("in use", out)

    def test_one_pruner_at_a_time(self):
        import fcntl
        self.build_all()
        cache = qc.Cache(str(self.cache))
        fd = os.open(os.path.join(cache.root, "prune.lock"), os.O_RDWR | os.O_CREAT)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX)
            out = self.prune("--max-size", "0")
            self.assertIn("another job is pruning the cache", out)
            self.assertEqual(4, len(self.entries()))
        finally:
            os.close(fd)
        self.prune("--max-size", "0")
        self.assertEqual(0, len(self.entries()))

    def test_hits_from_a_cache_that_cannot_be_written(self):
        self.build_all()
        cold = self.snapshot()
        modes = {}
        for dirpath, dirnames, filenames in os.walk(str(self.cache)):
            for n in dirnames + filenames:
                p = os.path.join(dirpath, n)
                modes[p] = os.stat(p).st_mode
        modes[str(self.cache)] = os.stat(str(self.cache)).st_mode
        try:
            for p, m in modes.items():
                os.chmod(p, m & ~0o222)
            if os.access(str(self.cache), os.W_OK):
                self.skipTest("the cache directory stays writable (running as root)")
            self.assertEqual([], self.build_all())
            self.assertEqual({"Base": "hit", "Mid": "hit", "Top": "hit", "Other": "hit"}, self.origins())
            self.assertEqual(cold, self.snapshot())
        finally:
            for p, m in sorted(modes.items()):
                os.chmod(p, m)

    def test_stats_line(self):
        self.build_all()
        out = self.prune("--build-dir", str(self.build), "--job", "build-x", "--max-size", "1000")
        self.assertRegex(out, r"^qmod-cache: job build-x: modules: miss 4; cache .*: [0-9.]+ MiB before pruning, "
                              r"[0-9.]+ MiB after\n$")

    def test_concurrent_jobs_share_the_cache(self):
        """Builds of four trees and a pruner run at once on one cache: every build succeeds and its modules pass the
        guard, whatever the pruner removed while they ran"""
        self.build_all()
        trees = []
        for i in range(4):
            src = self.root / ("source%d" % i)
            shutil.copytree(str(self.source), str(src), symlinks=True)
            build = self.root / ("build%d" % i)
            res = subprocess.run([CMAKE, "-G", "Unix Makefiles", "-S", str(src), "-B", str(build),
                                  "-DQORE_QMOD_CACHE_DIR=%s" % self.cache, "-DPython3_EXECUTABLE=%s" % sys.executable],
                                 capture_output=True, text=True, timeout=120)
            self.assertEqual(0, res.returncode, res.stdout + res.stderr)
            trees.append((src, build))
        env = dict(os.environ)
        for name in [n for n in env if n.startswith("QORE_QMOD_CACHE_")]:
            del env[name]
        for rnd in range(3):
            builds = []
            for src, build in trees:
                for q in (build / "qlib-qmod").rglob("*.qmod"):
                    q.unlink()
                builds.append(subprocess.Popen([CMAKE, "--build", str(build), "-j4"], stdout=subprocess.PIPE,
                                               stderr=subprocess.STDOUT, env=env))
            pruners = [subprocess.Popen([sys.executable, str(TOOL), "prune", "--cache-dir", str(self.cache),
                                         "--max-size", "0"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                       for _ in range(3)]
            for p in builds + pruners:
                out = p.communicate(timeout=300)[0].decode()
                self.assertEqual(0, p.returncode, out)
                self.assertNotIn("Traceback", out)
            for src, build in trees:
                res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(build), "--qcc",
                                      str(src / "qcc"), "--require-sidecars"], capture_output=True, text=True,
                                     timeout=60)
                self.assertEqual(0, res.returncode, res.stderr)
        # what the pruners left is consistent: every entry has its object
        cache = qc.Cache(str(self.cache))
        for _, p, key, _ in qc.cache_entries(cache):
            self.assertTrue(os.path.isdir(cache.object_dir(key)), p)


class CiScriptTest(unittest.TestCase):
    """test/docker_test/qmod-cache.sh: the cache of a job by its trust level, and a build without the cache"""

    SCRIPT = ROOT / "examples" / "test" / "docker_test" / "qmod-cache.sh"

    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-ci-")
        self.addCleanup(d.cleanup)
        self.root = Path(d.name)

    def keys(self):
        """(private key file, public key text)"""
        if not hasattr(self, "_keys"):
            self._keys = ed25519_keys(self.root, "k")
            if not self._keys:
                self.skipTest("OpenSSL 3 is required for signed entries")
        return self._keys

    def run_sh(self, cmd, **env):
        full = {k: v for k, v in os.environ.items() if not k.startswith(("QORE_QMOD_CACHE", "CI_"))}
        full.update(env)
        return subprocess.run(["sh", "-c", '. "%s"; %s' % (self.SCRIPT, cmd)], capture_output=True, text=True,
                              env=full, timeout=60)

    def setup(self, **env):
        """Runs qore_qmod_cache_setup and returns its variables"""
        res = self.run_sh('qore_qmod_cache_setup "%s"; echo "ARG=$QMOD_CACHE_ARG"; echo "PRUNE=$QMOD_CACHE_PRUNE_DIR"; '
                          'echo "RO=${QORE_QMOD_CACHE_READONLY:-}"; echo "KEY=${QORE_QMOD_CACHE_SIGNING_KEY:-}"; '
                          'echo "SIGN=${QORE_QMOD_CACHE_SIGN_AFTER_BUILD:-}"; echo "HELD=${_qmc_key:+held}"; '
                          'echo "EXPORTED=$(env | grep -c "^_qmc_key=")"' % ROOT, **env)
        self.assertEqual(0, res.returncode, res.stderr)
        rv = {"out": res.stdout + res.stderr}
        for line in res.stdout.splitlines():
            k, sep, v = line.partition("=")
            if sep and k in ("ARG", "PRUNE", "RO", "KEY", "SIGN", "HELD", "EXPORTED"):
                rv[k] = v
        # the private key's path never stays in the environment, nor gets into it
        self.assertEqual("", rv["KEY"])
        self.assertEqual("0", rv["EXPORTED"])
        return rv

    def test_without_cache(self):
        r = self.setup(QORE_QMOD_CACHE_SIGNING_KEY="/k.pem")
        self.assertEqual("", r["ARG"])
        self.assertEqual("", r["HELD"])
        self.assertIn("QORE_QMOD_CACHE_DIR is not set", r["out"])
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root / "missing"))
        self.assertEqual("", r["ARG"])
        self.assertIn("is not mounted", r["out"])
        # a mount without the protected cache
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="b",
                       QORE_QMOD_CACHE_SIGNING_KEY="k")
        self.assertEqual("", r["ARG"])
        self.assertEqual("", r["HELD"])
        # finishing without the cache does nothing, but unsets the private key
        res = self.run_sh('qore_qmod_cache_finish /nowhere /nowhere && echo "done ${QORE_QMOD_CACHE_SIGNING_KEY:-}"',
                          QORE_QMOD_CACHE_SIGNING_KEY="k")
        self.assertEqual("done \n", res.stdout)

    def test_protected_ref(self):
        (self.root / "protected").mkdir()
        own = self.root / "protected" / "build_x"
        # without the private key: no cache at all
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="build x")
        self.assertEqual("", r["ARG"])
        self.assertIn("without QORE_QMOD_CACHE_SIGNING_KEY", r["out"])
        priv, pub = self.keys()
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="build x",
                       QORE_QMOD_CACHE_SIGNING_KEY=priv)
        self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s" % own, r["ARG"])
        self.assertEqual(str(own), r["PRUNE"])
        self.assertEqual("", r["RO"])
        self.assertEqual("1", r["SIGN"])
        self.assertEqual("held", r["HELD"])
        # the private key is a file: its text is not accepted
        with open(priv) as f:
            pem = f.read()
        for value in (pem, "/nonexistent/key.pem"):
            r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true",
                           CI_JOB_NAME="build x", QORE_QMOD_CACHE_SIGNING_KEY=value)
            self.assertEqual("", r["ARG"])
            self.assertEqual("", r["HELD"])
            self.assertIn("the signing key cannot be used", r["out"])
            self.assertNotIn(pem.splitlines()[1], r["out"])
        # a public key that is not the private key's
        _, other = ed25519_keys(self.root, "other")
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="build x",
                       QORE_QMOD_CACHE_SIGNING_KEY=priv, QORE_QMOD_CACHE_VERIFY_KEY=other)
        self.assertEqual("", r["ARG"])
        self.assertIn("is not the public key of QORE_QMOD_CACHE_SIGNING_KEY", r["out"])
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="build x",
                       QORE_QMOD_CACHE_SIGNING_KEY=priv, QORE_QMOD_CACHE_VERIFY_KEY=pub)
        self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s" % own, r["ARG"])
        # a runner that mounts the protected cache read-only: read only, never pruned
        os.chmod(str(self.root / "protected"), 0o555)
        try:
            if os.access(str(self.root / "protected"), os.W_OK):
                self.skipTest("running as root")
            r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true",
                           CI_JOB_NAME="build x", QORE_QMOD_CACHE_SIGNING_KEY=priv)
            self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s" % own, r["ARG"])
            self.assertEqual("1", r["RO"])
            self.assertEqual("", r["PRUNE"])
            self.assertEqual("", r["SIGN"])
            self.assertEqual("", r["HELD"])
        finally:
            os.chmod(str(self.root / "protected"), 0o755)

    def test_unprotected_ref(self):
        (self.root / "protected" / "build-x").mkdir(parents=True)
        # without the public key: no cache
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="false", CI_JOB_NAME="build-x",
                       CI_COMMIT_REF_SLUG="feature-y")
        self.assertEqual("", r["ARG"])
        self.assertIn("no QORE_QMOD_CACHE_VERIFY_KEY", r["out"])
        # with it: the protected cache, read only, and no writable cache of its own; the private key is dropped
        priv, pub = self.keys()
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="false", CI_JOB_NAME="build-x",
                       CI_COMMIT_REF_SLUG="feature-y", QORE_QMOD_CACHE_VERIFY_KEY=pub,
                       QORE_QMOD_CACHE_SIGNING_KEY=priv)
        self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s" % (self.root / "protected" / "build-x"), r["ARG"])
        self.assertEqual("1", r["RO"])
        self.assertEqual("", r["PRUNE"])
        self.assertEqual("", r["SIGN"])
        self.assertEqual("", r["HELD"])
        self.assertEqual(["k.pem", "k.pub.pem", "protected"], sorted(p.name for p in self.root.iterdir()))
        # the public key as a file (a CI variable of type file)
        pub_file = self.root / "k.pub.pem"
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="false", CI_JOB_NAME="build-x",
                       QORE_QMOD_CACHE_VERIFY_KEY=str(pub_file))
        self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s" % (self.root / "protected" / "build-x"), r["ARG"])
        # a public key that is none
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="false", CI_JOB_NAME="build-x",
                       QORE_QMOD_CACHE_VERIFY_KEY="pub")
        self.assertEqual("", r["ARG"])
        self.assertIn("QORE_QMOD_CACHE_VERIFY_KEY cannot be used", r["out"])
        # a job name the protected jobs have not built yet: no cache
        r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_JOB_NAME="build-z", QORE_QMOD_CACHE_VERIFY_KEY=pub)
        self.assertEqual("", r["ARG"])

    def test_without_openssl(self):
        """A job with a key but no OpenSSL 3 to make or check the signatures builds without the cache"""
        (self.root / "protected" / "build-x").mkdir(parents=True)
        for env in ({"CI_COMMIT_REF_PROTECTED": "true", "QORE_QMOD_CACHE_SIGNING_KEY": "k"},
                    {"CI_COMMIT_REF_PROTECTED": "false", "QORE_QMOD_CACHE_VERIFY_KEY": "pub"}):
            r = self.setup(QORE_QMOD_CACHE_DIR=str(self.root), CI_JOB_NAME="build-x",
                           QORE_QMOD_CACHE_OPENSSL="/nonexistent/openssl", **env)
            self.assertEqual("", r["ARG"], env)
            self.assertIn("no OpenSSL 3 for the signatures: building without the cache", r["out"])
            self.assertEqual("", r["KEY"])

    def test_mount_path_with_spaces(self):
        mount = self.root / "My Shared Files"
        (mount / "protected").mkdir(parents=True)
        project = self.root / "project"
        project.mkdir()
        r = self.setup(QORE_QMOD_CACHE_DIR=str(mount), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="test-macos",
                       CI_PROJECT_DIR=str(project), QORE_QMOD_CACHE_SIGNING_KEY=self.keys()[0])
        self.assertEqual("-DQORE_QMOD_CACHE_DIR=%s/.qmod-cache" % project, r["ARG"])
        self.assertEqual(str(mount / "protected" / "test-macos"), os.readlink(str(project / ".qmod-cache")))

    def test_prune_other_job_caches_by_age(self):
        """The cache of a job name that no longer runs empties out: each writing job prunes the others by age"""
        for job, age in (("old-job", 8), ("other-job", 1), ("this-job", 30)):
            cache = qc.Cache(str(self.root / "protected" / job))
            entries = Path(cache.entries) / ("b" * 64)
            entries.mkdir(parents=True)
            obj = Path(cache.object_dir("c" * 64))
            obj.mkdir(parents=True)
            (obj / "qmod").write_text("x")
            entry = entries / ("c" * 64 + ".json")
            entry.write_text("{}")
            when = time.time() - age * 86400
            for p in (entry, obj, obj / "qmod"):
                os.utime(str(p), (when, when))
        res = self.run_sh('qore_qmod_cache_prune_others "%s"; echo rc=$?' % TOOL, QORE_QMOD_CACHE_DIR=str(self.root),
                          CI_JOB_NAME="this-job", QORE_QMOD_CACHE_MAX_AGE_DAYS="7")
        self.assertEqual("rc=0\n", res.stdout, res.stderr)
        def left(job):
            return sorted(p.name for p in (self.root / "protected" / job / qc.SCHEMA / "entries").rglob("*.json"))
        self.assertEqual([], left("old-job"))
        self.assertEqual(["c" * 64 + ".json"], left("other-job"))
        # this job's own cache is pruned by qore_qmod_cache_finish, with its size limit too
        self.assertEqual(["c" * 64 + ".json"], left("this-job"))

    def test_finish_forgets_the_private_key(self):
        (self.root / "protected").mkdir()
        res = self.run_sh('qore_qmod_cache_setup "%s" >/dev/null; echo "held=${_qmc_key:+1}"; '
                          'qore_qmod_cache_finish "%s" "%s" >/dev/null 2>&1; '
                          'echo "rc=$? key=${QORE_QMOD_CACHE_SIGNING_KEY:-}${_qmc_key:-}"'
                          % (ROOT, ROOT, self.root / "no-build"),
                          QORE_QMOD_CACHE_DIR=str(self.root), CI_COMMIT_REF_PROTECTED="true", CI_JOB_NAME="b",
                          QORE_QMOD_CACHE_SIGNING_KEY=self.keys()[0])
        # the guard fails for a build tree that does not exist (nothing is signed), and the key is forgotten all the
        # same
        self.assertEqual("held=1\nrc=1 key=\n", res.stdout)


def find_build_dir():
    d = os.environ.get("QORE_TEST_BUILD_DIR")
    if d and (Path(d) / "qcc").is_file() and (Path(d) / "qore").is_file():
        return Path(d)
    return None


def qmod_metadata_version(build_dir):
    """The version of the AOT module metadata record of a qmod that a Qore build tree compiled, or None"""
    for qmod in sorted((build_dir / "qlib-qmod").glob("*.qmod"))[:1]:
        data = qmod.read_bytes()
        pos = data.find(b"QAMI")
        while pos >= 0:
            if pos + 12 <= len(data):
                version, size = struct.unpack_from("<II", data, pos + 4)
                if pos + 12 + size <= len(data) and version in (1, 2):
                    return version
            pos = data.find(b"QAMI", pos + 1)
    return None


class RealQccTest(unittest.TestCase):
    """The cache with the qcc and qore of a build tree: three modules compiled for real"""

    def setUp(self):
        self.qore_build = find_build_dir()
        if not self.qore_build:
            self.skipTest("QORE_TEST_BUILD_DIR must name a Qore build tree with qcc and qore")
        version = qmod_metadata_version(self.qore_build)
        if version != 2:
            # the cache uses only modules with version 2 records; a build of this branch alone (before
            # fix/qmod-staleness-gaps, whose qcc writes them) has none, and every module is uncacheable
            self.skipTest("the qcc of %s writes AOT module metadata of version %s; the cache needs version 2"
                          % (self.qore_build, version))
        if not shutil.which(CMAKE):
            self.skipTest("CMake is required")
        d = tempfile.TemporaryDirectory(prefix="qore-qmod-cache-real-")
        self.addCleanup(d.cleanup)
        self.root = Path(d.name)
        self.source, self.build, self.cache = self.root / "source", self.root / "build", self.root / "cache"
        q = self.source / "qlib"
        q.mkdir(parents=True)
        (q / "CacheBase.qm").write_text('''%modern
module CacheBase {
    version = "1.0";
    desc = "qmod cache test";
    author = "Qore Technologies";
    url = "https://qore.org";
}
public namespace CacheBase {
    public class Value {
        public { int v = 1; }
    }
    public int sub base_value() { return 1; }
}
''')
        (q / "CacheMid.qm").write_text('''%modern
%requires CacheBase
module CacheMid {
    version = "1.0";
    desc = "qmod cache test";
    author = "Qore Technologies";
    url = "https://qore.org";
}
public namespace CacheMid {
    public int sub mid_value() { return CacheBase::base_value() + 1; }
}
''')
        (q / "CacheOther.qm").write_text('''%modern
module CacheOther {
    version = "1.0";
    desc = "qmod cache test";
    author = "Qore Technologies";
    url = "https://qore.org";
}
public namespace CacheOther {
    public string sub other_value() { return "other"; }
}
''')
        b = self.qore_build
        libpath = "LD_LIBRARY_PATH=%s" % b if sys.platform != "darwin" else "DYLD_LIBRARY_PATH=%s" % b
        (self.source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.14...3.31)
project(QmodCacheReal NONE)
include("{root}/cmake/QoreMacros.cmake")
set(QORE_BUILD_AOT_MODULES ON)
set(QORE_QCC_EXECUTABLE "{b}/qcc")
set(QCC_FORMAT_STAMP "{b}/qcc-format.stamp")
set(QORE_AOT_LINK_SOURCE_MODULES OFF)
set(QORE_GENERATE_JAVA_BINDINGS OFF)
set(QORE_USER_MODULES_DIR share/qore-modules)
set(QORE_QMOD_CACHE_TOOL "{tool}")
set(QORE_QM_METADATA_ENV "QORE_MODULE_DIR=${{CMAKE_SOURCE_DIR}}/qlib:${{CMAKE_BINARY_DIR}}/qlib-qmod"
    "QORE_MODULE_DIR_ONLY=1" "QORE_INCLUDE_DIR=" "{libpath}")
foreach(target qore docs docs-lang docs-lib)
    add_custom_target(${{target}})
endforeach()
QORE_QMOD_CACHE_SETUP()
qore_user_module("qlib/CacheBase.qm")
qore_user_module("qlib/CacheMid.qm")
qore_user_module("qlib/CacheOther.qm")
QORE_FINALIZE_USER_MODULE_DEPENDENCIES()
'''.format(root=ROOT.as_posix(), b=b.as_posix(), tool=TOOL.as_posix(), libpath=libpath))

    def build_all(self, env=None):
        shutil.rmtree(self.build, ignore_errors=True)
        res = subprocess.run([CMAKE, "-S", str(self.source), "-B", str(self.build),
                              "-DQORE_QMOD_CACHE_DIR=%s" % self.cache, "-DPython3_EXECUTABLE=%s" % sys.executable],
                             capture_output=True, text=True, timeout=120)
        self.assertEqual(0, res.returncode, res.stdout + res.stderr)
        full_env = dict(os.environ)
        for name in [n for n in full_env if n.startswith("QORE_QMOD_CACHE_")]:
            del full_env[name]
        full_env.update(env or {})
        res = subprocess.run([CMAKE, "--build", str(self.build)], capture_output=True, text=True, timeout=600,
                             env=full_env)
        self.output = res.stdout + res.stderr
        self.assertEqual(0, res.returncode, self.output)
        rv = {}
        for name in ("CacheBase", "CacheMid", "CacheOther"):
            with open(str(self.build / "qlib-qmod" / (name + ".qmod")) + qc.SIDECAR_SUFFIX) as f:
                rv[name] = json.load(f)["origin"]
        return rv

    def qmods(self):
        return {n: (self.build / "qlib-qmod" / (n + ".qmod")).read_bytes()
                for n in ("CacheBase", "CacheMid", "CacheOther")}

    def load(self, name, expr):
        """Loads a module with the build's qore from the build tree's qlib-qmod, as the in-tree tests do"""
        env = dict(os.environ)
        env["QORE_MODULE_DIR"] = "%s:%s" % (self.build / "qlib-qmod", self.source / "qlib")
        env["QORE_MODULE_DIR_ONLY"] = "1"
        env["LD_LIBRARY_PATH" if sys.platform != "darwin" else "DYLD_LIBRARY_PATH"] = str(self.qore_build)
        return subprocess.run([str(self.qore_build / "qore"), "-l", name, "-X", expr], capture_output=True,
                              text=True, timeout=120, env=env)

    def test_real_modules(self):
        miss = {"CacheBase": "miss", "CacheMid": "miss", "CacheOther": "miss"}
        hit = {"CacheBase": "hit", "CacheMid": "hit", "CacheOther": "hit"}
        self.assertEqual(miss, self.build_all())
        cold = self.qmods()
        self.assertEqual(hit, self.build_all())
        # byte for byte what qcc wrote
        self.assertEqual(cold, self.qmods())
        # nothing in a module depends on the commit of the Qore that compiled it (a commit that changes nothing qcc
        # compiles with must give the same module)
        res = subprocess.run([str(self.qore_build / "qcc"), "--dump-info",
                              str(self.build / "qlib-qmod" / "CacheBase.qmod")], capture_output=True, text=True,
                             timeout=120, env=dict(os.environ, LD_LIBRARY_PATH=str(self.qore_build),
                                                   DYLD_LIBRARY_PATH=str(self.qore_build)))
        self.assertIn("aot-runtime-identity", res.stdout, res.stderr)
        self.assertNotIn("git-hash", res.stdout)
        res = subprocess.run([sys.executable, str(TOOL), "verify", "--build-dir", str(self.build)],
                             capture_output=True, text=True, timeout=60)
        self.assertEqual(0, res.returncode, res.stderr)
        # the module loader uses the supplied modules: they are not out of date with their sources, also when the
        # sources have other modification times than when the modules were compiled (as after a checkout), so that
        # the loader compares their content with the fingerprints the cache supplied
        later = time.time() + 10
        for p in (self.source / "qlib").glob("*.qm"):
            os.utime(str(p), (later, later))
        res = self.load("CacheMid", "CacheMid::mid_value()")
        self.assertEqual(0, res.returncode, res.stdout + res.stderr)
        self.assertEqual("2", res.stdout.strip())
        self.assertNotIn("out of date", res.stderr)
        self.assertEqual("", res.stderr.strip())
        # qcc writes the same module again
        self.assertEqual(hit, self.build_all(env={"QORE_QMOD_CACHE_AUDIT": "1"}))
        self.assertEqual(3, self.output.count("audited: qcc wrote the same module"))

        # a change to CacheBase compiles it and CacheMid, which requires it
        base = self.source / "qlib" / "CacheBase.qm"
        base.write_text(base.read_text().replace("return 1;", "return 10;"))
        self.assertEqual({"CacheBase": "miss", "CacheMid": "miss", "CacheOther": "hit"}, self.build_all())
        res = self.load("CacheMid", "CacheMid::mid_value()")
        self.assertEqual(0, res.returncode, res.stdout + res.stderr)
        self.assertEqual("11", res.stdout.strip())
        self.assertNotIn("out of date", res.stderr)


if __name__ == "__main__":
    unittest.main()
