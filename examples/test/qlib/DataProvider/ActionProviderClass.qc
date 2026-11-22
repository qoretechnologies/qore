# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

# The cases of the ActionProviderClass-<n>.qtest tests (see addPartCase()): parts 1 and 2 check the applications that
# the modules of this source tree register, taken alternately in the order of their names, and part 3 those of the
# other modules in the module path (skipped where there are none, as in CI); loading and initializing every application
# of every module took more
# than two minutes in one test, and longer than the time limit of run_tests.sh under the load of a parallel run
# (qore#7253)

#! An application action whose provider cannot be created
hashdecl UnresolvableActionInfo {
    #! "app/action"
    string label;
    #! the modules that registered the application
    list<string> modules;
}

# An action names a connection scheme through its application, or the class of its data provider; an action with
# neither has a provider implied by the context where it is used.  Only the TypeScript action interface registers
# such actions in a Qore environment (Qorus's built-in applications are not loaded here); any other application
# whose actions have neither cannot be used: Qorus refuses its tool calls and Qog actions.
class ActionProviderClassBase inherits QUnit::Test {
    public {
        #! the module that registers the applications of TypeScript actions, which name no class
        const TypeScriptActionInterface = "TypeScriptActionInterface";
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0") {
        self.part = part;
        addPartCase(1, "every action of half of the modules of this source tree names a scheme or a class",
            \treeTest(), 0);
        addPartCase(2, "every action of the other half of the modules of this source tree names a scheme or a class",
            \treeTest(), 1);
        addPartCase(3, "every action of the other modules in the module path names a scheme or a class",
            \otherModulesTest());
        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs: each case is in one part, and each part is a test
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! the qlib directory of this source tree
    private static string treeDir() {
        return normalize_dir(get_script_dir() + "/../../../../qlib");
    }

    #! every other element of \a l, from index \a first: the parts take the modules alternately, as the costly ones
    #! are not spread evenly over the names
    private static list<string> alternate(list<string> l, int first) {
        list<string> rv = ();
        for (int i = first; i < l.size(); i += 2) {
            push rv, l[i];
        }
        return rv;
    }

    #! the modules of this source tree that register an application, sorted by name
    private static list<string> treeModules() {
        return sort(ActionProviderClassBase::getAppModules(treeDir()));
    }

    #! checks every other module of this source tree, from index \a first
    treeTest(int first) {
        list<string> tree = ActionProviderClassBase::treeModules();
        assertTrue(inlist("DataProvider", tree), "the scan finds the modules that register applications");
        checkModules(ActionProviderClassBase::alternate(tree, first));
    }

    otherModulesTest() {
        # the modules that register an application anywhere in the module path, except those of this source tree
        hash<string, bool> tree = map {$1: True}, ActionProviderClassBase::treeModules();
        list<string> dirs = (ENV.QORE_MODULE_DIR ?? "").split(":");
        dirs += get_qore_library_info().ModulePath.split(":");
        hash<string, bool> modules;
        foreach string dir in (dirs) {
            if (!dir.val() || !is_dir(dir) || normalize_dir(dir) == treeDir()) {
                continue;
            }
            map modules{$1} = True, ActionProviderClassBase::getAppModules(dir), !tree{$1} && !modules{$1};
        }
        if (!modules) {
            testSkip("no module outside this source tree registers an application");
        }
        # the other modules may register only applications with a connection scheme
        checkModules(keys modules, False);
    }

    #! loads the modules and checks the actions of the applications without a connection scheme
    private checkModules(list<string> mods, bool require_checked = True) {
        list<string> load_errors = ();
        foreach string mod in (sort(mods)) {
            try {
                load_module(mod);
            } catch (hash<ExceptionInfo> ex) {
                # a module that cannot be loaded here (ex: its binary module is not installed) registers nothing
                push load_errors, sprintf("%s: %s", mod, ex.err);
            }
        }
        if (m_options.verbose > 1 && load_errors) {
            printf("modules not loaded: %y\n", load_errors);
        }

        list<hash<UnresolvableActionInfo>> unresolvable = ();
        int checked = 0;
        foreach hash<DataProviderAppInfo> app in (DataProviderActionCatalog::getAllApps()) {
            if (app.scheme.val()) {
                continue;
            }
            list<string> owners = DataProviderActionCatalog::getModulePathsForApp(app.name) ?? ();
            # the applications that the TypeScript action interface registers, by the name of their owner module: the
            # catalog records a separated module by its directory and the module hash a module by its main file, so
            # their paths differ for the same module
            if (inlist(TypeScriptActionInterface, map ActionProviderClassBase::moduleName($1), owners)) {
                continue;
            }
            foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActions(app.name)) {
                ++checked;
                if (!action.cls.val()) {
                    push unresolvable, <UnresolvableActionInfo>{
                        "label": app.name + "/" + action.action,
                        "modules": owners,
                    };
                }
            }
        }
        if (require_checked) {
            assertTrue(checked > 0, "actions of applications without a connection scheme are checked");
        }
        assertEq((), unresolvable, "every action of an application without a connection scheme names its class");
    }

    #! Returns the name of the module at a module path: a module file or the directory of a separated module
    private static string moduleName(string path) {
        path =~ s/\/+$//;
        string name = basename(path);
        name =~ s/\.qmod?$//;
        return name;
    }

    #! Returns the modules in the directory whose sources register an application
    private static list<string> getAppModules(string dir) {
        list<string> rv = ();
        Dir top();
        top.chdir(dir);
        foreach string file in (top.listFiles("\\.qm$")) {
            if (ActionProviderClassBase::registersApp(dir + DirSep + file)) {
                push rv, file.substr(0, -3);
            }
        }
        foreach string name in (top.listDirs()) {
            string path = dir + DirSep + name;
            if (!is_file(path + DirSep + name + ".qm")) {
                continue;
            }
            Dir sub_dir();
            sub_dir.chdir(path);
            foreach string file in (sub_dir.listFiles("\\.q[cm]$")) {
                if (ActionProviderClassBase::registersApp(path + DirSep + file)) {
                    push rv, name;
                    break;
                }
            }
        }
        return rv;
    }

    #! Returns True if the source registers an application
    private static bool registersApp(string path) {
        return ReadOnlyFile::readTextFile(path).find("registerApp(") >= 0;
    }
}
