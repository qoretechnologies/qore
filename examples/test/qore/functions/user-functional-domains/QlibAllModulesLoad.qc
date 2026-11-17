# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# The cases of the QlibAllModulesLoad-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase())

#! Every qlib module loads first in a restricted Program, except the modules that are not meant for it
/** Part 1 (#5803): a Program restricted with \c PO_NO_PROCESS_CONTROL and \c PO_NO_THREAD_CONTROL, like a Qorus
    interface, can load a qlib module only if the module declares the functional domains its own code uses in its
    module header.  The modules that are not meant for restricted Programs (servers, the debugger, the language server,
    the REPL and the modules built on them) stay undeclared; see design/user-functional-domains.md.  Both sides are
    checked, so a new qlib module that needs a domain but does not declare it fails here, and so does a refused module
    that was changed to load without an entry in the list.

    Part 2 (#6438): a Program restricted with \c PO_NO_TERMINAL_IO can load every qlib module first, except the
    modules that are terminal modules by design (QUnit, TerminalFormat): a library module must not call a terminal I/O
    function, which such a Program refuses at parse time, anywhere in its code, but write to the \c stdout or
    \c stderr constants, which need terminal I/O only when they are written.  So a new qlib module that calls
    \c printf() fails here, and so does a terminal module that was changed to load without terminal I/O.

    A module that does not load for another reason (a missing binary module) is skipped.  A module that a Program has
    loaded is not checked again when another Program loads it, so each module is loaded in a child process of its own.
*/
class QlibAllModulesLoadBase inherits QUnit::Test {
    private {
        string qore_bin;
        string libqore_dir;
        string qlib_dir;

        #! the part of the cases that this test runs (see addPartCase())
        int part;

        #! holds the child scripts and the list of modules
        TmpDir tmp("qore-qlib-all-modules-load-");

        #! the modules that are not meant for restricted Programs and must stay refused
        const NotForRestricted = (
            "QLS", "QoreRepl", "DebugUtil", "DebugCmdLine", "DebugLinenoiseCmdLine", "DebugProgramControl",
            "DebugHandler", "DebugMcpServer", "VscDebugAdapter", "HttpServer", "HttpServerAsyncIo", "MllpUtil",
            "A2aServerHandler", "GraphQLHandler", "McpServerHandler", "ServerSentEventHandler", "UrlDownloader",
            "QoreMLUtils",
        );

        #! the terminal modules, which a Program without terminal I/O cannot load
        const TerminalModules = ("QUnit", "TerminalFormat");

        #! the child script: loads the module in a Program restricted with the given options and prints \c "loaded"
        #! or the error
        const ChildScript = "%%new-style
%%strict-args
Program p(%s | PO_NEW_STYLE);
try {
    p.parse(\"%%requires \" + ARGV[0] + \"\\n\", \"restricted\");
    printf(\"loaded\\n\");
} catch (hash<ExceptionInfo> ex) {
    printf(\"%%s: %%s\\n\", ex.err, ex.desc);
}
";
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qlib_dir = realpath(get_script_dir() + "/../../../../../qlib");

        addPartCase(1, "every qlib module loads in a restricted Program or is not meant for it", \testAllModules());
        addPartCase(1, "the list of modules that are not meant for restricted Programs names existing modules",
            \testListNamesModules());
        addPartCase(2, "every qlib module but the terminal modules loads first in a Program without terminal I/O",
            \testTerminalIo());
        addPartCase(2, "the list of terminal modules names existing modules", \testListNamesTerminalModules());

        set_return_value(main());
    }

    #! adds the case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the names of the qlib modules: Name.qm and Name/Name.qm
    private list<string> getModules() {
        list<string> rv = ();
        Dir d();
        d.chdir(qlib_dir);
        foreach string f in (d.listFiles()) {
            if (f =~ /\.qm$/) {
                rv += f.substr(0, -3);
            }
        }
        foreach string dn in (d.listDirs()) {
            if (is_file(qlib_dir + "/" + dn + "/" + dn + ".qm")) {
                rv += dn;
            }
        }
        return rv;
    }

    #! loads all modules in parallel, each first in a process of its own in a Program restricted with the given
    #! options, and returns the first output line by module
    private hash<string, string> loadAll(list<string> mods, string options) {
        string script = tmp.path + "/restricted.q";
        File f();
        f.open2(script, O_CREAT | O_TRUNC | O_WRONLY);
        f.write(sprintf(ChildScript, options));
        f.close();
        string list_file = tmp.path + "/modules";
        f.open2(list_file, O_CREAT | O_TRUNC | O_WRONLY);
        f.write(mods.join("\n") + "\n");
        f.close();
        *string env_dir = ENV.QORE_MODULE_DIR;
        string mod_dir = env_dir ? qlib_dir + ":" + env_dir : qlib_dir;
        # one line per module: <module>|<first line of the child's output>
        string worker = sprintf("out=$(LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s QORE_LIBDIR=%s QORE_MODULE_DIR=%s %s -b "
            "%s \"$1\" 2>/dev/null | head -1); echo \"$1|$out\"", libqore_dir, libqore_dir, libqore_dir, mod_dir,
            qore_bin, script);
        string cmd = sprintf("xargs -P 8 -n 1 sh -c %s sh < %s", shellQuote(worker), list_file);
        hash<string, string> rv();
        foreach string line in (backquote(cmd).split("\n")) {
            if (!line) {
                continue;
            }
            int i = line.find("|");
            rv{line.substr(0, i)} = line.substr(i + 1);
        }
        return rv;
    }

    private static string shellQuote(string str) {
        return "'" + replace(str, "'", "'\\''") + "'";
    }

    testAllModules() {
        list<string> mods = getModules();
        assertGt(100, mods.size());
        hash<string, string> res = loadAll(mods, "PO_NO_PROCESS_CONTROL | PO_NO_THREAD_CONTROL");
        assertEq(mods.size(), res.size());
        foreach string mod in (mods) {
            string out = res{mod};
            bool restricted = out =~ /parse options do not allow|conflicts with parse option|PO_NO_/;
            if (inlist(mod, NotForRestricted)) {
                # a refused module stays refused, unless a missing binary module stops it first
                assertFalse(out == "loaded", sprintf("%s is not meant for restricted Programs but loads: remove it "
                    "from the list", mod));
            } else if (out == "loaded" || restricted) {
                assertEq("loaded", out, sprintf("%s must declare the functional domains its own code uses in its "
                    "module header", mod));
            }
        }
    }

    testListNamesModules() {
        list<string> mods = getModules();
        foreach string mod in (NotForRestricted) {
            assertTrue(inlist(mod, mods), mod);
        }
    }

    testTerminalIo() {
        list<string> mods = getModules();
        assertGt(100, mods.size());
        hash<string, string> res = loadAll(mods, "PO_NO_TERMINAL_IO");
        assertEq(mods.size(), res.size());
        foreach string mod in (mods) {
            string out = res{mod};
            # refused for terminal I/O (the only restriction of the child): a terminal I/O function or class at
            # parse time, or a reflection access that the restriction makes the caller subject to
            bool restricted = out =~ /TERMINAL_IO|parse options do not allow|capabilities that are not allowed/
                || out =~ /REFLECTION-ACCESS-ERROR/;
            if (inlist(mod, TerminalModules)) {
                # a terminal module stays refused, unless a missing binary module stops it first
                assertFalse(out == "loaded", sprintf("%s is a terminal module but loads without terminal I/O: remove "
                    "it from the list", mod));
            } else if (out == "loaded" || restricted) {
                assertEq("loaded", out, sprintf("%s must not call terminal I/O functions; write to the stdout or "
                    "stderr constants, which need terminal I/O only when they are written", mod));
            }
        }
    }

    testListNamesTerminalModules() {
        list<string> mods = getModules();
        foreach string mod in (TerminalModules) {
            assertTrue(inlist(mod, mods), mod);
        }
    }
}
