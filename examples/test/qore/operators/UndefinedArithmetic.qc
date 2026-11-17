# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of the UndefinedArithmetic-<n>.qtest tests (qore#1034): each runs one part of them, so that each takes a
# short time, and together they run every case (see addPartCase())

#! Undefined arithmetic has one rule (qore#1034): a division by zero raises DIVISION-BY-ZERO, any other undefined
#! result is NaN
/** The probes (undefined-arithmetic/probes.q) divide (/, /=) and take the modulo (%, %=) of int, float and number
    values, typed, soft and untyped, by zero, by -0.0, by NaN and by infinity, also of 0, infinity and NaN, and with a
    divisor in a local holding a constant zero; they call the float and number math functions with arguments outside
    their domain, as typed locals and as constants, and check that the MPFR flags that an operation leaves set do not
    raise an exception in the next one.  They run in each execution mode in a child process, with tiered execution at
    the IR tier and promoted to native code, as qcc-compiled executables (-O0, -O3) and in a qcc-compiled module; the
    expected output is undefined-arithmetic/expected.txt.  Constant expressions are checked when they are parsed.

    The cases are in three parts (see addPartCase()), UndefinedArithmetic-<n>.qtest, so that each runs for a short
    time (the whole test took 175 s in a debug build on a busy machine): part 1 runs the execution modes, part 2 the
    qcc executables, and part 3 the qcc module and the constant expressions.
*/
class UndefinedArithmeticBase inherits QUnit::Test {
    public {
        const ExecModes = ("ast", "ir", "jit", "tiered");

        #! constant expressions: the code and the error that parsing it raises, or the value it prints
        const ConstantCases = (
            ("1 / 0", "DIVISION-BY-ZERO"),
            ("1.0 / 0.0", "DIVISION-BY-ZERO"),
            ("0.0 / 0.0", "DIVISION-BY-ZERO"),
            ("1.0 / -0.0", "DIVISION-BY-ZERO"),
            ("1n / 0n", "DIVISION-BY-ZERO"),
            ("0n / 0n", "DIVISION-BY-ZERO"),
            ("@nan@n / 0n", "DIVISION-BY-ZERO"),
            ("@inf@n / 0n", "DIVISION-BY-ZERO"),
            ("@inf@ / @inf@", ".nan"),
            ("@inf@n / @inf@n", ".nan"),
            ("1.0 / @inf@", "0.0"),
            ("pow(-1.0, 0.5)", ".nan"),
            ("pow(-1n, 0.5n)", ".nan"),
            ("sqrt(-1)", ".nan"),
            ("nlog(-1.0)", ".nan"),
            ("asin(-2)", ".nan"),
        );
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string libqore_dir;
        string qcc_bin;
        string expected;
        string probes;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        string dir = get_script_dir() + DirSep + "undefined-arithmetic" + DirSep;
        string lines = ReadOnlyFile::readTextFile(dir + "expected.txt");
        for (int i = 0; i < 2; ++i) {
            expected += sprintf("%d:\n%s", i, lines);
        }
        probes = ReadOnlyFile::readTextFile(dir + "probes.q");

        addPartCase(1, "execution modes", \testModes());
        addPartCase(2, "qcc executables", \testExecutables());
        addPartCase(3, "qcc module", \testModule());
        addPartCase(3, "constant expressions", \testConstants());
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    testModes() {
        TmpDir tmp("qore-undefined-arithmetic-");
        string file = writeScript(tmp.path);
        string env = sprintf("QORE_JIT_SYNC_COMPILE=1 QORE_JIT_THRESHOLD=2 LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s",
            libqore_dir, libqore_dir);
        foreach string mode in (ExecModes) {
            assertEq(expected, backquote(sprintf("%s %s --exec-mode=%s %s 2>&1", env, qore_bin, mode, file)), mode);
        }
        assertEq(expected, backquote(sprintf("%s %s --exec-mode=tiered --jit-ir-threshold=1 "
            "--jit-jit-threshold=1000000 %s 2>&1", env, qore_bin, file)), "tiered, IR tier");
        assertEq(expected, backquote(sprintf("%s %s --exec-mode=tiered --jit-ir-threshold=1 --jit-jit-threshold=1 "
            "%s 2>&1", env, qore_bin, file)), "tiered, promoted");
    }

    testExecutables() {
        TmpDir tmp("qore-undefined-arithmetic-");
        string file = writeScript(tmp.path);
        foreach string opt in (("-O0", "-O3")) {
            string exe = file + opt;
            string out = backquote(sprintf("QORE_LIBDIR=%s %s %s -o %s %s 2>&1", libqore_dir, qcc_bin, opt, exe,
                file));
            assertTrue(is_file(exe), sprintf("qcc %s compiled the executable: %s", opt, out));
            assertEq(expected, backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s 2>&1", libqore_dir,
                libqore_dir, exe)), "qcc " + opt);
        }
    }

    testModule() {
        TmpDir tmp("qore-undefined-arithmetic-");
        string qm = tmp.path + DirSep + "UndefinedArithmeticProbes.qm";
        string qmod = tmp.path + DirSep + "UndefinedArithmeticProbes.qmod";
        File f();
        f.open2(qm, O_CREAT | O_WRONLY | O_TRUNC);
        f.write("module UndefinedArithmeticProbes {\n    version = \"1.0\";\n    desc = \"qore#1034 probes\";\n"
            "    author = \"Qore Technologies, s.r.o.\";\n    url = \"https://qore.org\";\n}\n\n"
            + replace(probes, "\nstring sub probes()", "\npublic string sub probes()"));
        f.close();
        string out = backquote(sprintf("QORE_LIBDIR=%s %s -m -o %s %s 2>&1", libqore_dir, qcc_bin, qmod, qm));
        assertTrue(is_file(qmod), "qcc compiled the module: " + out);
        string code = "for (int i = 0; i < 2; ++i) { printf(\"%d:\\n%s\", i, probes()); }";
        assertEq(expected, backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -l %s -ne '%s' 2>&1",
            libqore_dir, libqore_dir, qore_bin, qmod, code)), "qcc module");
    }

    testConstants() {
        TmpDir tmp("qore-undefined-arithmetic-");
        foreach list<string> c in (ConstantCases) {
            string file = tmp.path + DirSep + sprintf("c%d.q", $#);
            File f();
            f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
            f.write(sprintf("%%modern\nprintf(\"%%y\\n\", %s);\n", c[0]));
            f.close();
            string out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s %s 2>&1", libqore_dir,
                libqore_dir, qore_bin, file));
            if (c[1] =~ /^[A-Z]/) {
                assertRegex(c[1], out, c[0]);
            } else {
                assertEq(c[1] + "\n", out, c[0]);
            }
        }
    }

    private string writeScript(string dir) {
        string file = dir + DirSep + "prog.q";
        File f();
        f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(probes + "\nfor (int i = 0; i < 2; ++i) {\n    printf(\"%d:\\n%s\", i, probes());\n}\n");
        f.close();
        return file;
    }
}
