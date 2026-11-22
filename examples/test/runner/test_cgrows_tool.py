#!/usr/bin/env python3
"""The callgrind row tool (tools/perf/cgrows: cgmin.sh, rows.sh and the bench scripts in bench/, qore#7193) gives the
instructions per iteration of a loop as the minimum over allocation layouts.  These tests run it with a stand-in for
valgrind: they check the layouts it writes (the script pad before the classes, the dead statements and the loop pad of
the loops), the environment of its runs (the environment pad), the value it computes and selects, its errors, rows.sh's
output, the conventions of the bench scripts and the row list of its README; and, given a qore (QORE_BINARY or
build/qore), that every layout of every bench script and every row of the list run without an error or a warning.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "tools/perf/cgrows"
CGMIN = TOOL / "cgmin.sh"
ROWS = TOOL / "rows.sh"
BENCH = TOOL / "bench"
ANCHOR = "int t = 0;"
DEAD = re.compile(r"if \(False\) \{ f\((\d+)\); \}")
PAD = re.compile(r'^const CGROW_PAD = "(p*)";$')

# a stand-in for valgrind that runs no program: the instructions of a run are 1000 + N * <cost of the layout>, where
# the layout is the number of numbered dead statements ("f(<i>)") after the first "int t = 0;" of the script (or, for an aot executable,
# the number in the file), and the costs come from CGROW_TEST_COSTS
COUNTING_VALGRIND = r"""#!/usr/bin/env python3
import os, re, sys
args = [a for a in sys.argv[1:] if not a.startswith("--")]
prog = args[0]
if prog.endswith("/qore"):
    text = open(args[-1]).read()
    line = [l for l in text.splitlines() if "int t = 0;" in l][0]
    k = len(re.findall(r"f\(\d+\)", line))
else:
    k = int(open(prog).read())
log = os.environ.get("CGROW_TEST_LOG")
if log:
    with open(log, "a") as f:
        pad = os.environ.get("CGROW_ENV_PAD")
        keys = ",".join(sorted(k for k in os.environ if k != "CGROW_ENV_PAD"))
        f.write("%s %s %s %s %s %s\n" % (os.environ["L"], os.environ["N"], os.environ.get("LD_LIBRARY_PATH", ""),
                                         "unset" if pad is None else "%d:%s" % (len(pad), set(pad) <= {"e"}), keys,
                                         " ".join(sys.argv[1:])))
costs = [float(c) for c in os.environ["CGROW_TEST_COSTS"].split()]
sys.stderr.write("==1== Events    : Ir\n==1== Collected : %d\n" % round(1000 + int(os.environ["N"]) * costs[k]))
"""

# a stand-in for qcc: writes the layout (the number of dead statements) into the executable
COUNTING_QCC = r"""#!/usr/bin/env python3
import re, sys
args = sys.argv[1:]
out = args[args.index("-o") + 1]
text = open(args[-1]).read()
line = [l for l in text.splitlines() if "int t = 0;" in l][0]
open(out, "w").write(str(len(re.findall(r"f\(\d+\)", line))))
"""

# a stand-in for valgrind that runs the program of the second run of a layout (N=2) without valgrind: it fails when
# the program fails or writes anything to its standard error (a parse warning); it reports 1000 + N * 10 instructions
PASSING_VALGRIND = r"""#!/usr/bin/env python3
import os, subprocess, sys
if os.environ["N"] != "2":
    sys.stderr.write("==1== Collected : %d\n" % (1000 + int(os.environ["N"]) * 10))
    sys.exit(0)
i = 1
while sys.argv[i].startswith("--"):
    i += 1
p = subprocess.run(sys.argv[i:], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
if p.returncode != 0 or p.stderr:
    sys.stderr.write("program failed (%d): %s\n" % (p.returncode, p.stderr))
    sys.exit(1)
sys.stderr.write("==1== Collected : %d\n" % (1000 + int(os.environ["N"]) * 10))
"""


def bench_scripts():
    return sorted(BENCH.glob("*.q"))


def rows_of(script):
    """Returns the rows (the L values) that a bench script runs"""
    return re.findall(r'^\s*case "([a-z_]+)":', script.read_text(), re.M)


def readme_rows():
    """Returns the rows of the README's row list as (script, row) pairs"""
    text = (TOOL / "README.md").read_text()
    section = text.split("## Rows", 1)[1].split("\n## ", 1)[0]
    rows = []
    for line in section.splitlines():
        m = re.match(r"^\| `(bench[-a-z]*\.q)` \| ([^|]+)\|", line)
        if m:
            rows += [(m.group(1), r) for r in re.findall(r"`([a-z_]+)`", m.group(2))]
    return rows


class CgrowsToolTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="cgrows-test."))
        self.addCleanup(shutil.rmtree, self.tmp)

    def stand_in(self, name, text):
        path = self.tmp / name
        path.write_text(text)
        path.chmod(0o755)
        return path

    def run_tool(self, args, env=None, tool=CGMIN):
        full = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "TMPDIR": str(self.tmp)}
        full.update(env or {})
        return subprocess.run(["sh", str(tool)] + [str(a) for a in args], capture_output=True, text=True, env=full,
                              timeout=600)

    def counting_env(self, costs, **kw):
        env = {"CGROW_VALGRIND": str(self.stand_in("valgrind", COUNTING_VALGRIND)), "CGROW_TEST_COSTS": costs}
        env.update(kw)
        return env

    def test_scripts_are_valid_shell(self):
        for script in (CGMIN, ROWS):
            with self.subTest(script=script.name):
                self.assertEqual(0, subprocess.run(["sh", "-n", str(script)]).returncode)
                self.assertTrue(script.stat().st_mode & 0o111, "%s is executable" % script.name)
                if shutil.which("dash"):
                    self.assertEqual(0, subprocess.run(["dash", "-n", str(script)]).returncode)

    def check_layouts(self, loop_step):
        keep = self.tmp / ("keep-%d" % loop_step)
        script = BENCH / "bench-member.q"
        p = self.run_tool(["/no/build", script, "mr", "ast"],
                          self.counting_env("7 7 7 7 7", CGROW_KEEP=str(keep), CGROW_PAD_STEP="100",
                                            CGROW_LOOP_PAD_STEP=str(loop_step)))
        self.assertEqual(0, p.returncode, p.stderr)
        src = script.read_text().splitlines()
        anchors = sum(line.count(ANCHOR) for line in src)
        self.assertGreater(anchors, 0)
        for k in range(5):
            with self.subTest(layout=k, loop_step=loop_step):
                lines = (keep / ("layout-%d.q" % k)).read_text().splitlines()
                # the first line, then the pad of k * 100 characters, before every other line of the script
                self.assertEqual(src[0], lines[0])
                m = PAD.match(lines[1])
                self.assertIsNotNone(m, lines[1])
                self.assertEqual(k * 100, len(m.group(1)))
                body = lines[2:]
                self.assertEqual(len(src) - 1, len(body))
                # k dead statements, numbered from 0, after each anchor, then (k > 0) the loop pad of
                # k * loop_step characters, and nothing else changed
                dead = ["if (False) { f(%d); }" % i for i in range(k)]
                if k and loop_step:
                    dead.append('if (False) { f("%s".size()); }' % ("l" * (k * loop_step)))
                for a, b in zip(src[1:], body):
                    if ANCHOR in a:
                        head, tail = a.split(ANCHOR, 1)
                        self.assertEqual(head + ANCHOR + "".join(" " + d for d in dead) + tail, b)
                    else:
                        self.assertEqual(a, b)
                self.assertEqual(anchors * k, sum(len(DEAD.findall(line)) for line in body))

    def test_layouts(self):
        self.check_layouts(50)

    def test_layouts_without_loop_pad(self):
        self.check_layouts(0)

    def test_no_pad(self):
        keep = self.tmp / "keep"
        script = BENCH / "bench.q"
        p = self.run_tool(["/no/build", script, "fs", "ir"],
                          self.counting_env("7 7 7", CGROW_KEEP=str(keep), CGROW_PAD="0", CGROW_LAYOUTS="3"))
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual(["layout-0.q", "layout-1.q", "layout-2.q"], sorted(f.name for f in keep.iterdir()))
        # without the pad, layout 0 is the script as it is, as with the old cgmin2.sh
        self.assertEqual(script.read_text(), (keep / "layout-0.q").read_text())
        self.assertNotIn("CGROW_PAD", (keep / "layout-2.q").read_text())
        self.assertNotIn(".size()", (keep / "layout-2.q").read_text())
        # nor an environment pad
        log = self.tmp / "runs.log"
        p = self.run_tool(["/no/build", script, "fs", "ir"],
                          self.counting_env("7 7 7", CGROW_PAD="0", CGROW_LAYOUTS="3", CGROW_TEST_LOG=str(log)))
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual(["unset"] * 6, [line.split()[3] for line in log.read_text().splitlines()])

    def test_env_pad_step(self):
        log = self.tmp / "runs.log"
        p = self.run_tool(["/no/build", BENCH / "bench.q", "fs", "ast"],
                          self.counting_env("7 7 7", CGROW_LAYOUTS="3", CGROW_ENV_PAD_STEP="100",
                                            CGROW_TEST_LOG=str(log)))
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual(["0:True", "0:True", "100:True", "100:True", "200:True", "200:True"],
                         [line.split()[3] for line in log.read_text().splitlines()])

    def test_minimum_of_the_layouts(self):
        log = self.tmp / "runs.log"
        env = self.counting_env("130 120 125 140 121", CGROW_VERBOSE="1", CGROW_TEST_LOG=str(log))
        self.env_keys = list(env) + ["PATH", "TMPDIR"]
        p = self.run_tool(["/no/build", BENCH / "bench-func.q", "fc", "jit"], env)
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual("120\n", p.stdout)
        self.assertEqual(["layout %d %d" % (k, v) for k, v in enumerate([130, 120, 125, 140, 121])],
                         p.stderr.splitlines())
        runs = log.read_text().splitlines()
        self.assertEqual(10, len(runs))
        for k, run in enumerate(runs):
            fields = run.split()
            # two runs per layout, N=2000 then N=22000, with the build as the library path, in the given mode
            self.assertEqual(["fc", "2000" if k % 2 == 0 else "22000", "/no/build"], fields[:3])
            # the environment pad of layout k // 2: k // 2 * 816 characters "e"; the other variables (the caller's,
            # the tool's own and those the shell adds) are the same in every run
            self.assertEqual("%d:True" % (k // 2 * 816), fields[3])
            keys = fields[4].split(",")
            self.assertTrue(set(self.env_keys) | {"L", "N", "LD_LIBRARY_PATH", "QORE_JIT_SYNC_COMPILE"} <= set(keys))
            self.assertEqual(runs[0].split()[4], fields[4])
            self.assertIn("--tool=callgrind", fields)
            self.assertIn("/no/build/qore", fields)
            self.assertIn("--exec-mode=jit", fields)
            self.assertIn("-b", fields)

    def test_decimals_and_iterations(self):
        # (1000 + 3000 * 7) - (1000 + 1000 * 7) over 2000 iterations; the costs have no fraction, so check the format
        p = self.run_tool(["/no/build", BENCH / "bench-func.q", "fc", "ast"],
                          self.counting_env("7 7", CGROW_LAYOUTS="2", CGROW_N0="1000", CGROW_N1="3000",
                                            CGROW_DECIMALS="2"))
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual("7.00\n", p.stdout)

    def test_rounding(self):
        # 825.99 per iteration is printed as 826 (the old tool truncated it to 825), 826.4 as 826
        env = self.counting_env("826.4 825.99 830 830 830", CGROW_VERBOSE="1")
        p = self.run_tool(["/no/build", BENCH / "bench-member.q", "mr", "jit"], env)
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual("826\n", p.stdout)
        self.assertEqual(["layout 0 826", "layout 1 826", "layout 2 830", "layout 3 830", "layout 4 830"],
                         p.stderr.splitlines())
        p = self.run_tool(["/no/build", BENCH / "bench-member.q", "mr", "jit"], dict(env, CGROW_DECIMALS="2"))
        self.assertEqual("825.99\n", p.stdout)

    def test_aot_modes(self):
        self.stand_in("qcc", COUNTING_QCC)
        p = self.run_tool([self.tmp, BENCH / "bench-method.q", "m", "aot3"],
                          self.counting_env("50 40 45 41 60", CGROW_VERBOSE="1"))
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual("40\n", p.stdout)

    def test_errors(self):
        script = BENCH / "bench.q"
        cases = [
            (["/no/build", script, "fs"], {}, "usage:"),
            (["/no/build", script, "fs", "fast"], {}, "unknown mode"),
            (["/no/build", self.tmp / "none.q", "fs", "ast"], {}, "no script"),
            (["/no/build", script, "fs", "ast"], {"CGROW_LAYOUTS": "0"}, "at least 1"),
            (["/no/build", script, "fs", "ast"], {"CGROW_N1": "100"}, "greater than"),
            (["/no/build", script, "fs", "ast"], {"CGROW_PAD_STEP": "x"}, "non-negative integers"),
            (["/no/build", script, "fs", "ast"], {"CGROW_ENV_PAD_STEP": "-1"}, "non-negative integers"),
            (["/no/build", script, "fs", "ast"], {"CGROW_LOOP_PAD_STEP": "1.5"}, "non-negative integers"),
        ]
        for args, env, message in cases:
            with self.subTest(message=message):
                p = self.run_tool(args, env)
                self.assertEqual(2, p.returncode)
                self.assertIn(message, p.stderr)
                self.assertEqual("", p.stdout)
        # a script that does not start with %modern
        bad = self.tmp / "bad.q"
        bad.write_text("# no directive\n%modern\n")
        p = self.run_tool(["/no/build", bad, "fs", "ast"])
        self.assertEqual(2, p.returncode)
        self.assertIn("not %modern", p.stderr)
        # a run that fails, and one whose output has no count: the error and the run's output, no value
        for name, text in (("fail", "#!/bin/sh\necho 'it broke' >&2\nexit 3\n"),
                           ("nocount", "#!/bin/sh\necho 'no count here' >&2\n")):
            with self.subTest(valgrind=name):
                vg = self.stand_in(name, text)
                p = self.run_tool(["/no/build", script, "fs", "ast"], {"CGROW_VALGRIND": str(vg)})
                self.assertEqual(1, p.returncode)
                self.assertEqual("", p.stdout)
                self.assertIn("the run with N=2000 of layout 0 failed", p.stderr)
                self.assertIn("it broke" if name == "fail" else "no count here", p.stderr)
        # a qcc that fails
        self.stand_in("qcc", "#!/bin/sh\necho 'qcc broke' >&2\nexit 1\n")
        p = self.run_tool([self.tmp, script, "fs", "aot0"], {"CGROW_VALGRIND": "/bin/false"})
        self.assertEqual(1, p.returncode)
        self.assertIn("qcc broke", p.stderr)
        # no temporary directory is left behind
        self.assertEqual([], [f.name for f in self.tmp.iterdir() if f.name.startswith("cgmin.")])

    def test_rows(self):
        env = self.counting_env("9 8 9 9 9", ROWS="bench.q:fs bench-member.q:mr", MODES="ast jit", P="3")
        p = self.run_tool(["/no/new", "/no/base"], env, tool=ROWS)
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual(["bench.q:fs ast 8 8 0", "bench.q:fs jit 8 8 0", "bench-member.q:mr ast 8 8 0",
                          "bench-member.q:mr jit 8 8 0"], p.stdout.splitlines())
        p = self.run_tool(["/no/new"], dict(env, ROWS="standard", MODES="ir"), tool=ROWS)
        self.assertEqual(0, p.returncode, p.stderr)
        self.assertEqual(["bench.q:fs ir 8", "bench-func.q:fc ir 8", "bench-func.q:sc ir 8", "bench-method.q:m ir 8",
                          "bench-map.q:mp ir 8", "bench-map.q:mm ir 8"], p.stdout.splitlines())
        # a failing cell: its error, "-" for its value, and exit code 1
        p = self.run_tool(["/no/new"], {"CGROW_VALGRIND": "/bin/false", "ROWS": "bench.q:fs", "MODES": "ast"},
                          tool=ROWS)
        self.assertEqual(1, p.returncode)
        self.assertEqual("bench.q:fs ast -\n", p.stdout)
        self.assertIn("the cell bench.q:fs ast of the new build failed", p.stderr)
        for rows, message in (("bench.q", "is not <script>:<row>"), ("nope.q:x", "no bench script")):
            p = self.run_tool(["/no/new"], dict(env, ROWS=rows), tool=ROWS)
            self.assertEqual(2, p.returncode)
            self.assertIn(message, p.stderr)

    def test_bench_scripts(self):
        scripts = bench_scripts()
        self.assertGreaterEqual(len(scripts), 11)
        for script in scripts:
            with self.subTest(script=script.name):
                lines = script.read_text().splitlines()
                self.assertEqual("%modern", lines[0], "the pad goes after the first line")
                self.assertNotIn("CGROW_PAD", script.read_text())
                self.assertTrue(any(re.match(r"int sub f\(int a\) \{", line) for line in lines),
                                "the dead statements call f()")
                self.assertGreater(sum(line.count(ANCHOR) for line in lines), 0)
                # sed replaces the first anchor of a line only
                self.assertTrue(all(line.count(ANCHOR) <= 1 for line in lines))
                self.assertIn("ENV.N.toInt()", script.read_text())
                self.assertIn("switch (ENV.L)", script.read_text())
                self.assertTrue(rows_of(script))

    def test_readme_row_list(self):
        rows = readme_rows()
        listed = {}
        for script, row in rows:
            listed.setdefault(script, []).append(row)
        # the list has every row of every bench script, in the order of the script
        self.assertEqual(sorted(s.name for s in bench_scripts()), sorted(listed))
        for script in bench_scripts():
            with self.subTest(script=script.name):
                self.assertEqual(rows_of(script), listed[script.name])
        # the rows of rows.sh's "standard" and "all" sets are in the list
        text = ROWS.read_text()
        for name in ("STANDARD", "ALL"):
            words = re.search(r'^%s="([^"]*)"' % name, text, re.M).group(1)
            words = words.replace("$STANDARD", "").replace("\\\n", " ").split()
            for word in words:
                self.assertIn(tuple(word.split(":")), rows)

    def qore_build(self):
        binary = os.environ.get("QORE_BINARY")
        if not binary:
            built = ROOT / "build/qore"
            if built.exists():
                return built.parent
            self.skipTest("no qore: set QORE_BINARY or build build/qore")
        build = self.tmp / "build"
        build.mkdir()
        (build / "qore").symlink_to(binary)
        return build

    def test_rows_run_in_every_layout(self):
        build = self.qore_build()
        env = {"CGROW_VALGRIND": str(self.stand_in("valgrind", PASSING_VALGRIND)), "CGROW_N0": "1", "CGROW_N1": "2",
               "HOME": str(self.tmp)}
        for script in bench_scripts():
            rows = rows_of(script)
            # every layout of the script (each parses the whole script) in ast and ir with its first row; its other
            # rows in ast
            cells = [(rows[0], mode, "5") for mode in ("ast", "ir")] + [(row, "ast", "1") for row in rows[1:]]
            for row, mode, layouts in cells:
                with self.subTest(script=script.name, row=row, mode=mode):
                    p = self.run_tool([build, script, row, mode], dict(env, CGROW_LAYOUTS=layouts))
                    self.assertEqual(0, p.returncode, p.stderr)
                    self.assertEqual("10\n", p.stdout)


if __name__ == "__main__":
    unittest.main()
