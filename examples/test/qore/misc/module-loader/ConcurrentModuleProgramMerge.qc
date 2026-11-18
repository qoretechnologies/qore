# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the ConcurrentModuleProgramMerge-<n>.qtest tests: each runs one part of them, so that each takes a short
# time (the three rounds of the race took about 220 s of CPU time and up to 250 s with 16 copies beside each other on a
# 32-core host, past the 300 s limit of run_tests.sh on a loaded host: qore#6377), and together they run every case
# (see addPartCase()); each part runs one round, which takes about a third of the time

#! Importing a module while its own Program is the target of a runtime module load
/** Code in an AOT module runs in the module's own Program, so a \c load_module() call in it merges
    another module into the namespace that every import of the module copies from.  The import read
    that namespace with no lock while the loading thread held the module Program's namespace write
    lock and appended to it, and the importer crashed in \c ConstantList::mergeUserPublic() on a
    reallocated constant list:

    @verbatim
ConstantList::mergeUserPublic(ConstantList const&)
qore_ns_private::copyMergeCommittedNamespace(qore_ns_private const&)
qore_ns_private::copyMergeCommittedNamespace(qore_ns_private const&)
qore_aot_module_ns_init_impl(QoreNamespace*, QoreNamespace*, ExceptionSink*)
QoreBuiltinModule::addToProgramImpl(QoreProgram*, ExceptionSink&) const
...
Program_loadModule_VsVi(...)
    @endverbatim

    That is how a Qorus server crashed at startup: services importing ProviderIndexUtil into their
    new Programs while ProviderIndexUtil loaded application modules into its own.

    The race is reproduced in a child process, so a crash fails an assertion rather than the test
    run.  Each importer imports the module a bounded number of times while each module is loaded and
    then waits for the next one: importers that imported without pause for the whole round took the CPU
    from the loading thread, so a round that takes about 25 s on a machine with idle cores took about
    2 minutes on two cores, and the test ran past its timeout on a loaded machine; a bounded round
    takes about 21 s, and 29 s on two cores.  Without the fix,
    a round crashed in 4 of 4 runs importing without pause and in 9 of 10 bounded (Linux x86_64 debug
    build); three rounds are run, one in each part.
*/
class ConcurrentModuleProgramMergeBase inherits QUnit::Test {
    public {
        #! the number of modules the module's own code loads into its Program in one round
        const LoadCount = 120;

        #! the number of threads importing the module into new Programs while it loads them
        const ImportThreads = 8;

        #! the number of times each importing thread imports the module while one module is loaded
        const ImportsPerLoad = 32;

        #! printed by the child when a round completes
        const Done = "ROUND-DONE";
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;

        string qore_bin;
        string qlib_dir;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        qlib_dir = normalize_dir(get_script_dir() + "/../../../../../qlib");
        addPartCase(1, "module code loads modules into the module's own Program", \moduleProgramIsTheTarget());
        # one round of the race in each part: a round takes nearly all of the time of the test
        addPartCase(1, "importing a module while its Program is merged into: round 1", \importDuringRuntimeLoad(), 1);
        addPartCase(2, "importing a module while its Program is merged into: round 2", \importDuringRuntimeLoad(), 2);
        addPartCase(3, "importing a module while its Program is merged into: round 3", \importDuringRuntimeLoad(), 3);
        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! The premise of the race: a module loaded by module code lands in the module's Program
    moduleProgramIsTheTarget() {
        if (!isAotModule()) {
            testSkip("ProviderIndexUtil was loaded from source; its code runs in the importing Program");
        }
        assertFalse(inlist("Mime", get_feature_list()), "the test Program has not loaded Mime itself");
        assertTrue(ProviderIndexUtil::tryLoadModule("Mime"), "ProviderIndexUtil loads Mime");
        assertFalse(inlist("Mime", get_feature_list()),
            "and Mime is merged into ProviderIndexUtil's own Program, not into the test Program");
    }

    #! runs one round of the race
    /** @param round the number of the round, from 1
    */
    importDuringRuntimeLoad(int round) {
        if (!isAotModule()) {
            testSkip("ProviderIndexUtil was loaded from source; no module Program is merged into at runtime");
        }
        list<string> modules = loadableModules();
        assertEq(LoadCount, modules.size(), "enough modules to load: " + modules.join(", "));

        TmpDir tmp("qore-concurrent-module-merge-");
        string list_file = tmp.path + "/modules.txt";
        File f();
        f.open2(list_file, O_CREAT | O_TRUNC | O_WRONLY);
        f.write(modules.join("\n") + "\n");
        f.close();
        string script = tmp.path + "/race.q";
        f.open2(script, O_CREAT | O_TRUNC | O_WRONLY);
        f.write(sprintf("%%modern
%%prepend-module-path %s
%%requires ProviderIndexUtil

list<string> mods = (map trim($1), ReadOnlyFile::readTextFile(ARGV[0]).split(\"\\n\"), trim($1));
int threads = %d;
int per_load = %d;
# each importer gets the number of imports to make for the module being loaded, and 0 at the end
list<Queue> go = ();
Counter imported();
Counter running();
int imports = 0;
for (int t = 0; t < threads; ++t) {
    Queue q();
    push go, q;
    running.inc();
    background sub () {
        on_exit running.dec();
        while (True) {
            int n = q.get();
            if (!n) {
                break;
            }
            for (int i = 0; i < n; ++i) {
                Program p(PO_NEW_STYLE);
                p.loadModule(\"ProviderIndexUtil\");
                ++imports;
            }
            imported.dec();
        }
    }();
}
int loaded = 0;
foreach string m in (mods) {
    for (int t = 0; t < threads; ++t) {
        imported.inc();
    }
    # the importers start while the module is loaded
    map $1.push(per_load), go;
    if (ProviderIndexUtil::tryLoadModule(m)) {
        ++loaded;
    }
    imported.waitForZero();
}
map $1.push(0), go;
running.waitForZero();
printf(\"%s loaded=%%d imports=%%d\\n\", loaded, imports);
", qlib_dir, ImportThreads, ImportsPerLoad, Done));
        f.close();

        {
            string output = backquote(sprintf("%s %s %s 2>/dev/null; echo \"rc=$?\"", qore_bin, script,
                list_file));
            *list<*string> result = (output =~ x/rc=([0-9]+)\s*$/);
            assertEq("0", result[0], sprintf("round %d: the child exits normally: %s", round, output));
            *list<*string> counts = (output =~ x/ROUND-DONE loaded=([0-9]+) imports=([0-9]+)/);
            assertTrue(exists counts, sprintf("round %d: the child completes the round: %s", round, output));
            assertTrue(counts[0].toInt() > 0, sprintf("round %d: module code loaded modules: %s", round,
                output));
            assertTrue(counts[1].toInt() > 0, sprintf("round %d: other threads imported the module meanwhile: %s",
                round, output));
        }
    }

    #! True when ProviderIndexUtil was loaded from its AOT-compiled module
    private bool isAotModule() {
        *hash<auto> info = get_module_hash().ProviderIndexUtil;
        return info.filename =~ /\.qmod$/;
    }

    #! AOT-compiled modules in qlib other than ProviderIndexUtil, in name order
    private list<string> loadableModules() {
        list<string> names = ();
        foreach string path in ((glob(qlib_dir + "/*.qmod") ?? ()) + (glob(qlib_dir + "/*/*.qmod") ?? ())) {
            string name = basename(path);
            name = name.substr(0, name.size() - 5);
            if (name == "ProviderIndexUtil" || inlist(name, names)) {
                continue;
            }
            push names, name;
        }
        list<string> rv = ();
        foreach string name in (sort(names)) {
            if (rv.size() == LoadCount) {
                break;
            }
            push rv, name;
        }
        return rv;
    }
}
