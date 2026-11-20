# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the SharedTypeObjectsModules-<n>.qtest tests (qore#6843): each runs one part of the library modules,
# so that each takes a short time, and together they check every module (see addPartCase())
#
# The modules are found in the source tree, not listed: every module of qlib whose sources use data types, data
# fields or REST schema action sets.  For each module, tenant A, a sandboxed Program, calls every public static
# getter without arguments of the module's public classes, which can build a value that the module shares on first
# use, and reads every public constant that holds an object; tenant B, another sandboxed Program, then tries to
# change each data type, data field and REST schema action set (through its request fields) that it got: every change
# must be refused, as the objects are shared by every Program of the process.  A module that cannot be loaded here
# (a missing binary module or external dependency) is reported as a skip with its reason.

const SharedTypeObjectsParts = 4;

#! the code of tenant B: tries to change the objects that it is given
const TenantChangeCode = "
%new-style
%requires DataProvider
%requires RestSchemaActions

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# tries to change an object, and returns what was tried and its result
list<string> sub change(string label, auto v, int depth = 0) {
    list<string> rv = ();
    if (depth > 2) {
        return rv;
    }
    switch (v.typeCode()) {
        case NT_HASH:
        case NT_LIST: {
            foreach auto e in (v.typeCode() == NT_HASH ? v.iterator() : v) {
                rv += change(label, e, depth + 1);
            }
            break;
        }
        case NT_OBJECT: {
            if (v instanceof AbstractDataProviderType) {
                rv += sprintf(\"%s: type %s: %s\", label, v.getName(), try_change(sub () { v.setOptions({}); }));
            } else if (v instanceof AbstractDataField) {
                rv += sprintf(\"%s: field %s: %s\", label, v.getName(), try_change(sub () { v.setAttributes({}); }));
            } else if (v instanceof RestSchemaActionSet) {
                foreach string action in (v.getActionNames()) {
                    *hash<string, AbstractDataField> fields = v.getOperationEx(action).getRequestFields();
                    if (fields) {
                        AbstractDataField f = fields.firstValue();
                        rv += sprintf(\"%s: action %s field %s: %s\", label, action, f.getName(),
                            try_change(sub () { f.setAttributes({}); }));
                        break;
                    }
                }
            }
            break;
        }
    }
    return rv;
}
";

class SharedTypeObjectsModulesBase inherits QUnit::Test {
    private {
        #! the part of the modules that this test checks
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        addPartCase(1, "shared objects of the modules of part 1", sub () { checkPart(1); });
        addPartCase(2, "shared objects of the modules of part 2", sub () { checkPart(2); });
        addPartCase(3, "shared objects of the modules of part 3", sub () { checkPart(3); });
        addPartCase(4, "shared objects of the modules of part 4", sub () { checkPart(4); });
        set_return_value(main());
    }

    #! checks the modules of a part: every SharedTypeObjectsParts-th module of the sorted list
    private checkPart(int p) {
        list<string> skipped = ();
        list<string> changed = ();
        int checked = 0;
        int i = 0;
        foreach string mod in (SharedTypeObjectsModulesBase::getModules()) {
            if ((i++ % SharedTypeObjectsParts) + 1 != p) {
                continue;
            }
            *string skip = checkModule(mod, \changed, \checked);
            if (skip) {
                skipped += skip;
            }
        }
        if (skipped) {
            # the modules that this environment cannot check are reported, so that the gap is visible
            printf("SKIPPED (%d):\n%s\n", skipped.size(), foldl $1 + "\n" + $2, skipped);
        }
        # every change must have been refused
        assertEq((), changed, sprintf("%d objects checked", checked));
    }

    #! adds a case if it belongs to this test's part
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the qlib modules whose sources use data types, data fields or REST schema action sets
    static list<string> getModules() {
        string qlib = normalize_dir(get_script_dir() + "/../../../../../qlib");
        list<string> rv = ();
        foreach string e in (sort(map basename($1), glob(qlib + DirSep + "*"))) {
            string path = qlib + DirSep + e;
            if (e =~ /\.qm$/ && is_file(path)) {
                if (SharedTypeObjectsModulesBase::usesTypes((path,))) {
                    rv += e.substr(0, -3);
                }
            } else if (is_dir(path) && is_file(path + DirSep + e + ".qm")) {
                if (SharedTypeObjectsModulesBase::usesTypes(glob(path + DirSep + "*.q[mc]"))) {
                    rv += e;
                }
            }
        }
        return rv;
    }

    #! returns True if the sources use data types, data fields or REST schema action sets
    private static bool usesTypes(list<string> files) {
        foreach string f in (files) {
            if (File::readTextFile(f) =~ /(DataProviderType|HashDataType|AbstractDataField|RestSchemaActionSet)/) {
                return True;
            }
        }
        return False;
    }

    #! returns the public static getters without arguments and the public object constants of a module's sources
    private static list<string> getAccessors(string mod) {
        string qlib = normalize_dir(get_script_dir() + "/../../../../../qlib");
        list<string> files = is_dir(qlib + DirSep + mod)
            ? glob(qlib + DirSep + mod + DirSep + "*.q[mc]")
            : (qlib + DirSep + mod + ".qm",);
        list<string> rv = ();
        foreach string f in (files) {
            *string cls;
            foreach string line in (File::readTextFile(f).split("\n")) {
                if (*string c = (line =~ x/^\s*public\s+class\s+([A-Za-z0-9_]+)/)[0]) {
                    cls = c;
                } else if (line =~ /^\s*(class|namespace)\s/ || line =~ /^\s*public\s+namespace\s/) {
                    remove cls;
                }
                if (!cls) {
                    continue;
                }
                if (*list<*string> g = (line =~ x/^\s{4}static\s+\*?([A-Za-z0-9_:<>, ]+)\s+(get[A-Za-z0-9_]*)\(\)\s*\{/)) {
                    if (g[0] !~ /^(string|int|bool|float|number|date|binary|code|softlist<string>|list<string>)$/) {
                        rv += sprintf("%s::%s()", cls, g[1]);
                    }
                } else if (*string k = (line =~ x/^\s{8}const\s+([A-Za-z0-9_]+)\s*=\s*new\s/)[0]) {
                    rv += sprintf("%s::%s", cls, k);
                }
            }
        }
        return rv;
    }

    #! checks the shared objects of a module, and returns the reason why it was skipped, if it was
    private *string checkModule(string mod, reference<list<string>> changed, reference<int> checked) {
        try {
            load_module(mod);
        } catch (hash<ExceptionInfo> ex) {
            return sprintf("module %y cannot be loaded here: %s: %s", mod, ex.err, ex.desc);
        }
        list<string> accessors = SharedTypeObjectsModulesBase::getAccessors(mod);
        if (!accessors) {
            return;
        }
        # tenant A gets the values
        Program a(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_NO_INHERIT_USER_FUNC_VARIANTS);
        a.setSandboxManager(new SandboxManager());
        string get_code = sprintf("%%new-style\n%%requires %s\nhash<auto> sub get() {\n    hash<auto> rv = {};\n",
            mod);
        foreach string acc in (accessors) {
            get_code += sprintf("    try { rv{%y} = %s; } catch (hash<ExceptionInfo> ex) { rv{%y} = ex.err; }\n", acc,
                acc, acc);
        }
        get_code += "    return rv;\n}\n";
        hash<auto> values;
        *string skip;
        try {
            a.parse(get_code, "tenant-a");
            values = a.callFunction("get");
        } catch (hash<ExceptionInfo> ex) {
            # an accessor that the tenant cannot reach (in a namespace that the module does not export, for example):
            # each accessor is tried in a tenant of its own, and those that cannot be used are reported
            list<string> unusable = ();
            foreach string acc in (accessors) {
                Program a1(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_NO_INHERIT_USER_FUNC_VARIANTS);
                a1.setSandboxManager(new SandboxManager());
                try {
                    a1.parse(sprintf("%%new-style\n%%requires %s\nauto sub get() { return %s; }\n", mod, acc),
                        "tenant-a");
                    values{acc} = a1.callFunction("get");
                } catch (hash<ExceptionInfo> ex1) {
                    unusable += sprintf("%s (%s: %s)", acc, ex1.err, ex1.desc);
                }
            }
            if (unusable) {
                skip = sprintf("module %y: accessors that a sandboxed Program cannot use here: %y", mod, unusable);
            }
        }
        # tenant B tries to change them
        Program b(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_NO_INHERIT_USER_FUNC_VARIANTS);
        b.setSandboxManager(new SandboxManager());
        b.parse(TenantChangeCode, "tenant-b");
        foreach hash<auto> v in (values.pairIterator()) {
            foreach string r in (b.callFunction("change", v.key, v.value)) {
                ++checked;
                if (r =~ /: changed$/) {
                    changed += sprintf("%s: %s", mod, r);
                }
            }
        }
        return skip;
    }
}
