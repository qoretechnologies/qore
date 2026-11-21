# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the AsciiStringIndex-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase())


#! character offsets of ASCII text in UTF-8 strings (issue #6138)
class AsciiStringIndexBase inherits QUnit::Test {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string binary = TestInterpreter::qore_interpreter();
        # QORE_BINARY can be the Debug/Release selector script; native tools belong beside the running interpreter
        # (a compiled test has none: then beside the interpreter that it runs)
        string tools_dir = dirname(TestInterpreter::running_compiled() ? TestInterpreter::qore_interpreter()
            : TestInterpreter::own_executable());
        string source = get_script_dir() + "/ascii-string-index.q";
        string scan_source = get_script_dir() + "/ascii-string-index-scan.q";

        #! the lengths of the strings whose characters the counted loops read
        const LoopLengths = (50000, 200000);
        #! the offsets of the counted reads
        const ReadOffsets = (1000, 10000, 100000);
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        addPartCase(1, "native character offsets after the mutators of the C++ API", \native());
        addPartCase(2, "character offsets after the mutators of the language in all execution modes", \modes());
        addPartCase(3, "indexing the characters of an ASCII string is linear in all execution modes", \scan());
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    private native() {
        string helper = ENV.QORE_ASCII_STRING_INDEX_TEST ?? tools_dir + "/qore-ascii-string-index-test";
        if (!is_file(helper)) {
            testSkip("native ASCII string index test executable is not available");
            return;
        }
        auto result = ExecModeHarness::run_command(quote(helper));
        assertEq(0, result.rc, result.out);
        assertRegex("^PASS: [0-9]+ ASCII prefix cache checks\n$", result.stdout, result.out);
    }

    private modes() {
        foreach string mode in (ExecModeHarness::ExecModes) {
            string out = ExecModeHarness::run(quote(binary) + " -b --enable-debug --exec-mode=" + mode + " "
                + quote(source));
            assertRegex("^PASS: [0-9]+ language ASCII prefix cache checks\n$", out, mode);
        }
    }

    #! counts the bytes that the character offset functions walk or scan (dbg_get_string_scan_bytes())
    /** Deterministic: a loop over the characters of a string scans each byte about once, and a read at any offset of
        a string whose ASCII text is known scans nothing; with the cache disabled (as before issue #6138), a read at
        offset i walks i bytes, which shows that the counter sees an O(i) walk
    */
    private scan() {
        foreach string mode in (ExecModeHarness::ExecModes) {
            string out = ExecModeHarness::run(quote(binary) + " -b --exec-mode=" + mode + " " + quote(scan_source));
            # the build that runs the script decides: the counter is only available in debug builds
            if (out =~ /^skip: /) {
                testSkip(trim(out));
            }
            hash<string, int> counts;
            # each line is "<what> <length, offset or length()> <bytes scanned>"
            foreach string line in (out.split("\n")) {
                *list<*string> f = (line =~ x/^([a-z-]+) ([a-z0-9]+) ([0-9]+)$/);
                if (f) {
                    counts{f[0] + " " + f[1]} = f[2].toInt();
                }
            }
            foreach int n in (LoopLengths) {
                foreach string loop in (("index", "length", "backward")) {
                    *int bytes = counts{loop + " " + n};
                    assertTrue(exists bytes, sprintf("%s: %s %d: %s", mode, loop, n, out));
                    # linear: each byte about once (the scan runs ahead geometrically); a quadratic loop walks about
                    # n * n / 2 bytes
                    assertTrue(bytes <= 2 * n + 64, sprintf("%s: %s: %d characters: %d bytes scanned", mode, loop,
                        n, bytes));
                }
            }
            foreach int n in (LoopLengths) {
                # an ASCII string literal is scanned at most once, not at every evaluation (qore#6625)
                *int bytes = counts{"literal " + n};
                assertTrue(exists bytes, sprintf("%s: literal %d: %s", mode, n, out));
                assertTrue(bytes <= 64, sprintf("%s: literal: %d characters: %d bytes scanned", mode, n, bytes));
            }
            foreach int i in (ReadOffsets) {
                assertEq(0, counts{"read " + i}, sprintf("%s: a read at offset %d of known ASCII text scans nothing",
                    mode, i));
            }
            assertEq(0, counts."read length", mode + ": length() of known ASCII text scans nothing");
            # the counter sees the walk of the uncached code: a read at a later offset walks more by the difference
            for (int j = 1; j < ReadOffsets.size(); ++j) {
                int lo = ReadOffsets[j - 1];
                int hi = ReadOffsets[j];
                assertTrue(counts{"uncached-read " + hi} - counts{"uncached-read " + lo} >= hi - lo,
                    sprintf("%s: uncached reads at %d and %d: %d and %d bytes", mode, lo, hi,
                    counts{"uncached-read " + lo}, counts{"uncached-read " + hi}));
            }
        }
    }

    private static string quote(string value) {
        return "'" + replace(value, "'", "'\"'\"'") + "'";
    }
}
