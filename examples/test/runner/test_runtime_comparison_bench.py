#!/usr/bin/env python3
"""The runtime comparison harness (bench/runtime-comparison) measures the kernels of kernels.qr and its other Qore
scripts in the AST, IR, JIT and AOT tiers, against Java.  These tests run its own tests, which need no Qore, Java or
network access (references, record parsers, argument checks, bounded child processes), and check its conventions:
every Qore script is %modern and runs no interpreter itself (it never reads QORE_ARGV[0] or starts a process), and every
child process that its Python scripts start goes through run.run_bounded(), which bounds it with a timeout and ends the
process group it starts.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import ast
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
BENCH = ROOT / "bench/runtime-comparison"

#: the calls that start a process
PROCESS_CALLS = {("subprocess", name) for name in ("run", "call", "check_call", "check_output", "Popen", "getoutput",
                                                     "getstatusoutput")}
PROCESS_CALLS |= {("os", name) for name in ("system", "popen", "fork", "forkpty", "spawnv", "spawnve", "spawnl",
                                            "spawnle", "spawnlp", "spawnvp", "execv", "execve", "execl", "execle",
                                            "execlp", "execvp", "posix_spawn", "posix_spawnp")}

#: the modules that start processes in other ways, which the harness does not use
OTHER_PROCESS_MODULES = ("asyncio", "multiprocessing", "pty", "concurrent")

#: the one function that may start a process: run.py's run_bounded()
BOUNDED = ("run.py", "run_bounded")

#: Qore code that starts a process or finds the interpreter
QORE_PROCESS = re.compile(r"\bQORE_ARGV\b|\bsystem\s*\(|\bexec\s*\(|\bfork\s*\(|\bnew\s+Process\b|`")


def process_calls(path):
    """Returns (function, line, call) for each call in a Python source that starts a process"""
    tree = ast.parse(path.read_text(), str(path))
    found = []

    def visit(node, function):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            function = node.name
        if isinstance(node, ast.ImportFrom) and node.module in ("subprocess", "os"):
            for alias in node.names:
                if (node.module, alias.name) in PROCESS_CALLS or alias.name == "*":
                    found.append((function, node.lineno, "from %s import %s" % (node.module, alias.name)))
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            names = [node.module] if isinstance(node, ast.ImportFrom) else [alias.name for alias in node.names]
            for name in names:
                if name and name.split(".")[0] in OTHER_PROCESS_MODULES:
                    found.append((function, node.lineno, "import " + name))
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
                and isinstance(node.func.value, ast.Name) and (node.func.value.id, node.func.attr) in PROCESS_CALLS:
            found.append((function, node.lineno, node.func.value.id + "." + node.func.attr))
        for child in ast.iter_child_nodes(node):
            visit(child, function)

    visit(tree, None)
    return found


def qore_code(text):
    """Returns Qore source without its comments and string literals"""
    return re.sub(r"#[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'", "", text, flags=re.S)


class RuntimeComparisonBenchTest(unittest.TestCase):
    def test_harness_tests(self):
        result = subprocess.run([sys.executable, "-B", "-W", "error", "-m", "unittest", "discover", "-s", str(BENCH),
                                 "-p", "test_*.py"], cwd=str(BENCH), text=True, capture_output=True, timeout=600)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertRegex(result.stderr, r"\nRan [1-9]\d* tests? in ")

    def test_child_processes_are_bounded(self):
        sources = sorted(BENCH.glob("*.py"))
        self.assertIn(BENCH / "run.py", sources)
        errors = []
        bounded = []
        for path in sources:
            for function, line, call in process_calls(path):
                if (path.name, function) == BOUNDED:
                    bounded.append(call)
                else:
                    errors.append("%s:%d: %s: start child processes with run.run_bounded()"
                                  % (path.relative_to(ROOT), line, call))
        self.assertEqual([], errors, "\n".join(errors))
        self.assertEqual(["subprocess.Popen"], bounded)

    def test_process_call_scan(self):
        # the scan finds a process call wherever it is, and attributes it to its innermost function
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.py"
            probe.write_text("import os, subprocess\n"
                             "subprocess.run(['x'])\n"
                             "def outer():\n"
                             "    def inner():\n"
                             "        return subprocess.check_output(['x'], timeout=1)\n"
                             "    os.system('x')\n"
                             "def safe(run):\n"
                             "    return run.run_bounded(['x'], timeout=1)\n"
                             "from subprocess import Popen as P, TimeoutExpired\n"
                             "import multiprocessing.pool, pty\n")
            self.assertEqual([(None, 2, "subprocess.run"), ("inner", 5, "subprocess.check_output"),
                              ("outer", 6, "os.system"), (None, 9, "from subprocess import Popen"),
                              (None, 10, "import multiprocessing.pool"), (None, 10, "import pty")],
                             process_calls(probe))

    def test_qore_scripts(self):
        scripts = sorted(BENCH.glob("*.qr"))
        self.assertIn(BENCH / "kernels.qr", scripts)
        errors = []
        for path in scripts:
            text = path.read_text()
            rel = path.relative_to(ROOT)
            if not re.search(r"^%modern$", text, re.M):
                errors.append("%s: not %%modern" % rel)
            if not re.search(r"Copyright.*2026", text):
                errors.append("%s: no 2026 copyright" % rel)
            for match in QORE_PROCESS.finditer(qore_code(text)):
                errors.append("%s: %s: a benchmark script runs no process and no interpreter" % (rel, match.group(0)))
        self.assertEqual([], errors, "\n".join(errors))

    def test_qore_code_scan(self):
        self.assertEqual("x = ; \n", qore_code("x = \"QORE_ARGV[0]\"; # system()\n"))
        self.assertTrue(QORE_PROCESS.search(qore_code("string q = QORE_ARGV[0];")))
        self.assertTrue(QORE_PROCESS.search(qore_code("Process p = new Process(\"qore\");")))
        self.assertTrue(QORE_PROCESS.search(qore_code("system(\"qore x.q\");")))
        self.assertFalse(QORE_PROCESS.search(qore_code("/* QORE_ARGV */ int i = 1;")))


if __name__ == "__main__":
    unittest.main()
