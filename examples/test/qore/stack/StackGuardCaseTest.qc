# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

# included by the stack guard tests; the including test requires QUnit, FsUtil and ExecModeHarness

#! a recursion path: code that recurses until it exhausts the stack of the thread that runs it
hashdecl StackCase {
    #! code run before the operation, at top level on the main thread, such as declarations and code that declares
    #! and initializes a value "v" (as "<type> v = <value>;"); "v" is declared without a value if it does not
    string setup;
    #! the operation; an expression of the value "v", run on a thread with a small stack
    string op;
    #! modules the script requires
    list<string> requires = ();
    #! the stack size of the thread that runs the operation
    int stack_size = 256 * 1024;
    #! if set, the operation may return this value (formatted with %y) in modes other than "ast", whose execution
    #! does not recurse with the nesting of the operation
    *string native_value;
    #! if set, the case is skipped for this reason
    *string skip;
}

#! runs recursion paths in every execution mode and checks that each raises STACK-LIMIT-EXCEEDED
/** Each case runs a script in a child process in each execution mode (see ExecModeHarness): the script runs an
    operation that recurses without bound on a thread with a small stack.  The operation must raise a catchable
    \c STACK-LIMIT-EXCEEDED exception before it overflows the stack, and the process must run more code afterwards;
    an operation that misses the stack guard overflows the stack instead, which crashes the process, or hangs it
    while fault signals are blocked, so each run has a timeout (\c QORE_STACK_TEST_TIMEOUT seconds, 60 by default).
*/
class StackGuardCaseTest inherits QUnit::Test {
    private {
        string qore_bin;
        string timeout_cmd;
        TmpDir tmp("qore-stack-guard-");
        #! code included in every script before the setup of the case
        string common;
    }

    constructor(string name, hash<string, hash<StackCase>> cases, string common = "")
            : QUnit::Test(name, "1.0", \ARGV) {
        self.common = common;
        qore_bin = ENV.QORE_BIN ?? ENV.QORE_BINARY ?? QORE_ARGV[0] ?? "qore";
        timeout_cmd = ENV.QORE_TIMEOUT_EXECUTABLE ?? backquote(
            "command -v timeout 2>/dev/null || command -v gtimeout 2>/dev/null || true");
        trim timeout_cmd;

        foreach hash<auto> i in (cases.pairIterator()) {
            string case_name = i.key;
            hash<StackCase> c = i.value;
            addTestCase(case_name, sub () { testCase(case_name, c); });
        }
    }

    setUp() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("Qore library was not built with stack protection support");
        }
    }

    #! returns the script of a case
    string getScript(hash<StackCase> c) {
        string script = "%modern\n";
        map script += sprintf("%%requires %s\n", $1), c.requires;
        script += common;
        script += c.setup + "\n";
        if (c.setup !~ /\bv = /) {
            script += "auto v;\n";
        }
        script += sprintf("auto sub op(auto v) {\n    return %s;\n}\n", c.op);
        script += sprintf("set_default_thread_stack_size(%d);\n", c.stack_size);
        script += "Queue q();
background sub () {
    try {
        auto rv = op(v);
        q.push(sprintf(\"VALUE: %y\", rv));
    } catch (hash<ExceptionInfo> ex) {
        q.push(ex.err);
    }
}();
printf(\"RESULT: %s\\n\", q.get());
# the process is still healthy: it runs more code on a new thread and on this one
background sub () { q.push(foldl $1 + $2, (map $1 * 2, (1, 2, 3))); }();
printf(\"HEALTHY: %d %d\\n\", q.get(), foldl $1 + $2, (1, 2, 3));
";
        return script;
    }

    #! runs a case in every execution mode
    testCase(string name, hash<StackCase> c) {
        if (c.skip) {
            testSkip(c.skip);
        }
        string path = sprintf("%s/case-%s.q", tmp.path, regex_subst(name, "[^a-zA-Z0-9]+", "_", RE_Global));
        File f();
        f.open2(path, O_WRONLY | O_CREAT | O_TRUNC);
        f.write(getScript(c));
        f.close();

        string prefix = timeout_cmd ? sprintf("%s -s KILL %s ", timeout_cmd, ENV.QORE_STACK_TEST_TIMEOUT ?? "60") : "";
        foreach string mode in (ExecModeHarness::ExecModes) {
            string out = ExecModeHarness::run(sprintf("%s%s --enable-debug --exec-mode=%s %s; echo EXIT=$?",
                prefix, qore_bin, mode, path), ExecModeHarness::NativeCheck::Jit);
            string label = sprintf("%s (%s)", name, mode);
            if (c.native_value && mode != "ast") {
                assertRegex(sprintf("RESULT: (STACK-LIMIT-EXCEEDED|VALUE: %s)\n", c.native_value), out,
                    label + ": exception raised or value returned");
            } else {
                assertRegex("RESULT: STACK-LIMIT-EXCEEDED\n", out, label + ": exception raised");
            }
            assertRegex("HEALTHY: 12 6\n", out, label + ": the process is healthy afterwards");
            assertRegex("EXIT=0\n", out, label + ": exit status");
        }
    }
}
