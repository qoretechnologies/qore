#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probe of callref-object-method-program.qtest (argument: a directory for the file that it writes): a call
# reference to a method of an object runs as the direct call of the method in the Program that created the reference
# would; prints one line per kind of child Program and class of the object

%modern

#! a user class inheriting a builtin class: \o.open2() resolves to the inherited builtin method at run time
class PFile inherits File {
    #! a direct call of the inherited builtin method in the class's own code
    selfOpen(string path, int flags) {
        open2(path, flags);
    }

    #! a reference to the inherited builtin method created in the class's own code
    code selfRef() {
        return \open2();
    }
}

#! a user class overriding a builtin method
class OFile inherits File {
    open2(string path, int flags) {
        File::open2(path, flags);
    }
}

#! inherits the user method that overrides the builtin method
class OFile2 inherits OFile {
}

#! a user class whose method does something that a restricted Program cannot do itself
class UBase {
    act(string path, int flags) {
        File f();
        f.open2(path, flags);
    }
}

#! inherits the user method
class UDerived inherits UBase {
}

#! overrides the user method
class UOverride inherits UBase {
    act(string path, int flags) {
        UBase::act(path, flags);
    }
}

const ChildSource = "
auto sub direct_open2(object o, string path, int flags) { return o.open2(path, flags); }
code sub ref_open2(object o) { return \\o.open2(); }
auto sub direct_act(object o, string path, int flags) { return o.act(path, flags); }
code sub ref_act(object o) { return \\o.act(); }
auto sub call(code c, string path, int flags) { return c(path, flags); }
auto sub self_direct(object o, string path, int flags) { return o.selfOpen(path, flags); }
auto sub self_ref(object o, string path, int flags) { code c = o.selfRef(); return c(path, flags); }
";

const Targets = (
    ("name": "file", "method": "open2", "new": object sub () { return new File(); }),
    ("name": "pfile", "method": "open2", "new": object sub () { return new PFile(); }),
    ("name": "ofile", "method": "open2", "new": object sub () { return new OFile(); }),
    ("name": "ofile2", "method": "open2", "new": object sub () { return new OFile2(); }),
    ("name": "ubase", "method": "act", "new": object sub () { return new UBase(); }),
    ("name": "uderived", "method": "act", "new": object sub () { return new UDerived(); }),
    ("name": "uoverride", "method": "act", "new": object sub () { return new UOverride(); }),
);

const Flags = O_CREAT | O_WRONLY | O_TRUNC;

#! the file that the calls create, in the directory given as the only argument
string path = ARGV[0] + DirSep + "f.txt";

#! calls the code with a new object, which it keeps while the code runs: a call reference does not keep its object
string sub res(code new_object, code c) {
    object o = new_object();
    try {
        c(o);
        return "ok";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

code sub parent_ref(object o, string method) {
    return method == "open2" ? \o.open2() : \o.act();
}

auto sub parent_direct(object o, string method) {
    return method == "open2" ? o.open2(path, Flags) : o.act(path, Flags);
}

sub run(string kind) {
    int po = PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS;
    if (kind == "restricted") {
        po |= PO_NO_FILESYSTEM;
    }
    Program p(po);
    p.parse(ChildSource, "child");
    if (kind == "sandbox") {
        p.setSandboxManager(new SandboxManager());
    }
    foreach hash<auto> t in (Targets) {
        string m = t.method;
        code n = t."new";
        list<string> l = (
            "direct-child=" + res(n, sub (object o) { p.callFunction("direct_" + m, o, path, Flags); }),
            "ref-child-child=" + res(n, sub (object o) {
                p.callFunction("call", p.callFunction("ref_" + m, o), path, Flags);
            }),
            "ref-child-parent=" + res(n, sub (object o) { code c = p.callFunction("ref_" + m, o); c(path, Flags); }),
            "direct-parent=" + res(n, sub (object o) { parent_direct(o, m); }),
            "ref-parent-parent=" + res(n, sub (object o) { code c = parent_ref(o, m); c(path, Flags); }),
            "ref-parent-child=" + res(n, sub (object o) {
                p.callFunction("call", parent_ref(o, m), path, Flags);
            }),
        );
        if (t.name == "pfile") {
            l += (
                "self-direct-child=" + res(n, sub (object o) { p.callFunction("self_direct", o, path, Flags); }),
                "self-ref-child=" + res(n, sub (object o) { p.callFunction("self_ref", o, path, Flags); }),
            );
        }
        printf("%s %s: %s\n", kind, t.name, foldl $1 + " " + $2, l);
    }
}

on_exit {
    unlink(path);
}

map run($1), ("trusted", "restricted", "sandbox");
