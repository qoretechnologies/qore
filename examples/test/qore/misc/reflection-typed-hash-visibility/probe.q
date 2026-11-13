#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probe of reflection-typed-hash-visibility.qtest: a child Program uses the typed hash of a value that its
# parent created through reflection; prints one result per case

%modern
%requires reflection

our int init_count = 0;
public int sub parent_init() { return ++init_count; }

#! the parent's typed hash; its initializer runs the parent's code
public hashdecl ParentHash {
    int a = parent_init();
}

our string probe_file = get_script_path();
#! reads a file: a sandbox refuses it, also in code of the parent that runs for the child
public int sub parent_read() {
    try {
        ReadOnlyFile f(probe_file);
        return 100;
    } catch (hash<ExceptionInfo> ex) {
        return 200;
    }
}

#! a typed hash whose initializer reads a file: 100 if it was read, 200 if the sandbox refused it
public hashdecl ReadHash {
    int a = parent_read();
}

const ChildSource = "%requires reflection
auto sub cast_it(hash<auto> v) { return Type::getType(v).getTypedHash().doCast({}).a; }
auto sub default_it(hash<auto> v) { return Type::getType(v).getTypedHash().getMember('a').getDefaultValue(); }
auto sub read_it(hash<auto> v) { return Type::getType(v).getTypedHash().doCast({}).a; }
auto sub system_it() {
    TypedHash th = Type::getType(<SourceLocationInfo>{}).getTypedHash();
    return th.getName() + ':' + th.doCast({}).size();
}
";

#! the direct cast, which a child that does not inherit ReadHash cannot parse
const ReadSource = "auto sub direct_read_it() { return cast<hash<ReadHash>>({}).a; }
";

sub run(string label, int po, bool sandbox) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS | po);
    p.parse(ChildSource + (po & PO_NO_INHERIT_USER_HASHDECLS ? "" : ReadSource), "child");
    if (sandbox) {
        p.setSandboxManager(new SandboxManager());
    }
    hash<ParentHash> v = <ParentHash>{"a": 0};
    hash<ReadHash> rv = <ReadHash>{"a": 0};
    list<string> funcs = ("cast_it", "default_it", "system_it");
    # the initializer runs under the child's sandbox and restrictions, as in a cast that the child makes itself
    if (!(po & PO_NO_INHERIT_USER_HASHDECLS)) {
        funcs += ("read_it", "direct_read_it");
    }
    foreach string f in (funcs) {
        string res;
        try {
            auto r = f == "system_it" || f == "direct_read_it"
                ? p.callFunction(f)
                : p.callFunction(f, f == "read_it" ? rv : v);
            res = sprintf("%y", r);
        } catch (hash<ExceptionInfo> ex) {
            res = ex.err;
        }
        printf("%s %s: %s\n", label, f, res);
    }
}

# the child inherits the parent's typed hash, so it could name it, or it does not
run("trusted-inherit", 0, False);
run("trusted-no-inherit", PO_NO_INHERIT_USER_HASHDECLS, False);
run("sandbox-inherit", 0, True);
run("sandbox-no-inherit", PO_NO_INHERIT_USER_HASHDECLS, True);
run("restricted-inherit", PO_NO_PROCESS_CONTROL, False);
run("restricted-no-inherit", PO_NO_INHERIT_USER_HASHDECLS | PO_NO_PROCESS_CONTROL, False);
printf("initializer calls: %d\n", init_count);
