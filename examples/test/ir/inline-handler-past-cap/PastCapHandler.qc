# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The cases of the PastCapHandler-<n>.qtest tests (see addPartCase()): a qcc executable compiled with -O0, and one
# compiled with -O3, each compared with the AST interpreter

#! a debugger's request in a handler that runs out of line past the inline-handler budget (qore#7531)
/** A handler of a nested block in a handler of a function: a break or a continue requested in the nested block's
    handler abandons the nested block's return; a later handler of the function takes the request, and the function's
    pending return resumes: its data is restored, and an object of it was released when the nested return replaced it
    (qore#7022).  A return request, and a request that no later handler or loop takes, go on with the nested return
    (qore#6949).

    With 67 more handlers in the function, the nested handler runs out of line, past the budget of the handlers that
    the lowering runs inline (QORE_IR_PATH_INLINE_HANDLER_BUDGET), through a ScopeExit; a qcc executable gave the
    abandoned return (-5) to the caller.  The test makes the program with 67 extra handlers and with none (inline) from
    past-cap.q.in, and requires the results of the AST interpreter, and those of a qcc executable to be the same.
*/
class PastCapHandlerBase inherits QUnit::Test {
    public {
        #! the numbers of extra handlers of the functions: past the budget, and inline
        const ExtraHandlers = (67, 0);

        #! the functions: the name, whether it has a later handler, the nested return, and the tag of the request
        const Functions = (
            ("capPending", True, "-5", "req_pending"),
            ("capNoLast", False, "-5", "req_nolast"),
            ("capInnerObj", True, "new InnerMarker()", "req_innerobj"),
            ("capInnerObjNoLast", False, "new InnerMarker()", "req_innerobj_nolast"),
        );

        #! the kinds of the function's own pending return: an integer (-7), an object, a string (a data value)
        const Kinds = ("", "Object", "Data");

        const Flows = ("break", "continue", "return");

        #! the results of the AST interpreter in the issue's table (qore#7531)
        const IssueResults = {
            "capPending break": "RESULT:-7 LOG:[\"last\"]",
            "capPendingData break": "RESULT:\"outer value 3\" LOG:[\"last\"]",
            "capPendingObject break": "RESULT:null LOG:[\"released\", \"last\"]",
            "capHandOver break": "RESULT:-7 LOG:[\"last\"]",
            "capInnerObj break": "RESULT:-7 LOG:[\"inner released\", \"last\"]",
            "capNoLast break": "RESULT:-5 LOG:[]",
        };
    }

    private {
        string qore_bin;
        string libqore_dir;
        string qcc_bin;
        string env;
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        addPartCase(1, "a request in a handler past the budget, qcc -O0", \testQcc(), 0);
        addPartCase(2, "a request in a handler past the budget, qcc -O3", \testQcc(), 3);
        set_return_value(main());
    }

    #! adds a test case that runs only in the given part
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the source of the functions with the given number of extra handlers
    static string functions(int n) {
        string rv = "class CapMarker {\n    destructor() { push log, \"released\"; }\n}\n"
            "class InnerMarker {\n    destructor() { push log, \"inner released\"; }\n}\n";
        string extra = strmul("    on_exit { n += 0; }\n", n);
        string tail = "    if (ARGV[0] =~ /Object$/) { return new CapMarker(); }\n"
            "    if (ARGV[0] =~ /Data$/) { return sprintf(\"outer value %d\", n); }\n"
            "    return n - 10;\n}\n";
        foreach list<auto> f in (Functions) {
            rv += sprintf("auto sub %s(int n) {\n", f[0]);
            if (f[1]) {
                rv += "    on_exit { push log, \"last\"; }\n";
            }
            rv += sprintf("    on_exit {\n        {\n            on_exit {\n                push log, \"inner\";"
                "      ### %s\n            }\n            return %s;\n        }\n    }\n", f[3], f[2]);
            rv += extra + tail;
        }
        # a handler's return in the nested block, then the request in the block's last handler (qore#7022)
        rv += "auto sub capHandOver(int n) {\n    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
            "            on_exit {\n                push log, \"inner\";      ### req_handover\n            }\n"
            "            on_exit { return -9; }\n        }\n    }\n" + extra + tail;
        # a loop in the handler's body takes the break: the nested return is abandoned, and the code after the loop runs
        rv += "auto sub capLoop(int n) {\n    on_exit { push log, \"last\"; }\n    on_exit {\n"
            "        for (int j = 0; j < 1; ++j) {\n            {\n                on_exit {\n"
            "                    push log, \"inner\";      ### req_loop\n                }\n"
            "                return -5;\n            }\n        }\n        push log, \"after loop\";\n    }\n"
            + extra + tail;
        # the request in a handler that is not the last of its run: the normal end of the next one replaces it
        rv += "auto sub capNotLast(int n) {\n    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
            "            on_exit { push log, \"next\"; }\n            on_exit {\n"
            "                push log, \"inner\";      ### req_not_last\n            }\n            return -5;\n"
            "        }\n    }\n" + extra + tail;
        foreach string name in ((map $1[0], Functions) + ("capHandOver", "capLoop", "capNotLast")) {
            foreach string kind in (("Object", "Data")) {
                rv += sprintf("auto sub %s%s(int n) { return %s(n); }\n", name, kind, name);
            }
        }
        return rv;
    }

    #! writes the program with the given number of extra handlers, and returns its path and the lines of its tags
    private hash<auto> program(string dir, int n) {
        string src = ReadOnlyFile::readTextFile(get_script_dir() + "/past-cap.q.in");
        src = replace(src, "%%FUNCTIONS%%", functions(n));
        string path = join_paths(dir, sprintf("past-cap-%d.q", n));
        File f();
        f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(src);
        f.close();
        hash<string, int> tags = {};
        list<string> lines = src.split("\n");
        for (int i = 0; i < lines.size(); ++i) {
            if (*string tag = (lines[i] =~ x/### (\w+)$/)[0]) {
                tags{tag} = i + 1;
            }
        }
        return {"path": path, "tags": tags};
    }

    #! the requests: "<function><kind> <flow>" -> the arguments of the program
    static hash<string, string> requests(hash<string, int> tags) {
        hash<string, string> rv = {};
        foreach list<auto> f in (Functions + (("capHandOver", True, "", "req_handover"), ("capLoop", True, "", "req_loop"),
                ("capNotLast", True, "", "req_not_last"))) {
            foreach string kind in (Kinds) {
                foreach string flow in (Flows) {
                    rv{sprintf("%s%s %s", f[0], kind, flow)} = sprintf("%s%s %s %d 1", f[0], kind, flow, tags{f[3]});
                }
            }
        }
        return rv;
    }

    testQcc(int opt) {
        TmpDir dir("qore-inline-handler-past-cap-");
        # the results of the AST interpreter, which do not depend on the number of handlers
        hash<string, hash<string, string>> ast = {};
        hash<string, hash<auto>> programs = {};
        foreach int n in (ExtraHandlers) {
            programs{n} = program(dir.path, n);
            foreach hash<auto> r in (requests(programs{n}.tags).pairIterator()) {
                ast{n}{r.key} = ExecModeHarness::run(sprintf("%s %s -b --exec-mode=ast %s %s", env, qore_bin,
                    programs{n}.path, r.value), ExecModeHarness::NativeCheck::None).split(" STEPS:")[0];
            }
        }
        foreach hash<auto> r in (IssueResults.pairIterator()) {
            assertEq(r.value, ast{ExtraHandlers[0]}{r.key}, "AST: " + r.key);
        }
        assertEq(ast{ExtraHandlers[1]}, ast{ExtraHandlers[0]}, "AST: the same results inline and past the budget");
        foreach int n in (ExtraHandlers) {
            string exe = join_paths(dir.path, sprintf("past-cap-%d-O%d", n, opt));
            int rc;
            string out = backquote(sprintf("%s %s -O%d -o %s %s 2>&1", env, qcc_bin, opt, exe, programs{n}.path),
                \rc);
            assertEq(0, rc, sprintf("qcc -O%d (%d extra handlers): %s", opt, n, out.substr(-2000)));
            if (rc) {
                continue;
            }
            foreach hash<auto> r in (requests(programs{n}.tags).pairIterator()) {
                string q = ExecModeHarness::run_any(sprintf("%s %s -b %s", env, exe, r.value)).split(" STEPS:")[0];
                assertEq(ast{n}{r.key}, q, sprintf("qcc -O%d, %d extra handlers: %s", opt, n, r.key));
            }
        }
    }
}
