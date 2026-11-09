# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the forms of write and read that module-public-var-readonly.qtest checks, shared with its probe scripts (qore#5664)

namespace ModVarForms {
    #! the exception code of the refusal
    const ErrCode = "MODULE-VARIABLE-READ-ONLY";

    #! the state of the module after OurVarMod::reset(), as OurVarMod::state() gives it
    const InitialState = "1|[1, 2, 3]|{a: 1, b: 2}|abc|1|{a: {b: 1}}|1|[1, 2, 3]";

    #! every form of write: restricted code may use none of them; each is the body of a function
    const WriteForms = {
        "assign": "OurVarMod::i = 99;",
        "add-assign": "OurVarMod::i += 5;",
        "mul-assign": "OurVarMod::i *= 2;",
        "post-incr": "OurVarMod::i++;",
        "pre-incr": "++OurVarMod::i;",
        "post-decr": "OurVarMod::i--;",
        "pre-decr": "--OurVarMod::i;",
        "push": "push OurVarMod::l, 9;",
        "pop": "pop OurVarMod::l;",
        "shift": "shift OurVarMod::l;",
        "unshift": "unshift OurVarMod::l, 0;",
        "splice": "splice OurVarMod::l, 0, 1;",
        "extract": "extract OurVarMod::l, 0, 1;",
        "list-add": "OurVarMod::l += 4;",
        "remove": "remove OurVarMod::i;",
        "delete": "delete OurVarMod::h;",
        "remove-key": "remove OurVarMod::h.a;",
        "delete-key": "delete OurVarMod::h.a;",
        "hash-elem": "OurVarMod::h.c = 3;",
        "nested-elem": "OurVarMod::hn.a.b = 3;",
        "list-elem": "OurVarMod::l[0] = 9;",
        "member": "OurVarMod::o.x = 9;",
        "string-add": "OurVarMod::s += \"x\";",
        "regex-subst": "OurVarMod::s =~ s/a/x/;",
        "typed-ref": "reference<int> r = \\OurVarMod::i; r = 7;",
        "untyped-ref": "reference r = \\OurVarMod::i; r = 7;",
        "ref-arg": "set_ref(\\OurVarMod::i);",
        "elem-ref": "reference r = \\OurVarMod::h.a; r = 9;",
        "foreach-ref": "foreach int x in (\\OurVarMod::l) { x = 0; }",
        "closure": "code c = sub () { OurVarMod::i = 10; }; c();",
        # a reference created in the restricted code, written by a function of the trusted host
        "host-ref": "host_write(\\OurVarMod::i);",
        # the static members of a class of the module
        "static-assign": "OurVarMod::Obj::sx = 5;",
        "static-add-assign": "OurVarMod::Obj::sx += 2;",
        "static-incr": "OurVarMod::Obj::sx++;",
        "static-remove": "remove OurVarMod::Obj::sx;",
        "static-list-elem": "OurVarMod::Obj::sl[0] = 9;",
        "static-push": "push OurVarMod::Obj::sl, 4;",
        "static-ref": "reference r = \\OurVarMod::Obj::sx; r = 6;",
        "static-closure": "code c = sub () { OurVarMod::Obj::sx = 7; }; c();",
        "static-host-ref": "host_write(\\OurVarMod::Obj::sx);",
    };

    #! every form of read: (code, value in restricted code, value in a trusted importer)
    const ReadForms = {
        "read": ("return OurVarMod::i;", "1", "1"),
        "read-list-elem": ("return OurVarMod::l[1];", "2", "2"),
        "read-member": ("return OurVarMod::o.x;", "1", "1"),
        "read-elements": ("return elements OurVarMod::l;", "3", "3"),
        "read-foreach": ("int t = 0; foreach int x in (OurVarMod::l) { t += x; } return t;", "6", "6"),
        "read-typed-ref": ("reference<int> r = \\OurVarMod::i; return r;", "1", "1"),
        "read-untyped-ref": ("reference r = \\OurVarMod::i; return r;", "1", "1"),
        "read-elem-ref": ("reference r = \\OurVarMod::h.b; return r;", "2", "2"),
        "read-member-ref": ("reference r = \\OurVarMod::o.x; return r;", "1", "1"),
        "read-ref-arg": ("return get_ref(\\OurVarMod::i);", "1", "1"),
        # a missing element reads as no value; the restricted code does not create it
        "read-missing-key-ref": ("reference r = \\OurVarMod::hn.a.zz; return r;", "null", "null"),
        "read-list-oor-ref": ("reference r = \\OurVarMod::l[5]; return r;", "null", "null"),
        # a member gate is not called to resolve a reference, which a missing member does not have
        "read-gate-ref": ("reference r = \\OurVarMod::g.nope; return r;", "\"gate:nope\"", "\"gate:nope\""),
        "read-gate-typed-ref": ("return get_string_ref(\\OurVarMod::g.nope);", "INVALID-MEMBER", "INVALID-MEMBER"),
        # a typed reference to a value that its type rejects raises the type error
        "read-typed-mismatch": ("return get_string_ref(\\OurVarMod::hn.a.b);", "RUNTIME-TYPE-ERROR",
            "RUNTIME-TYPE-ERROR"),
        # a typed reference whose type the value already has
        "read-typed-hash": ("return get_hash_ref(\\OurVarMod::hn.a);", "{b: 1}", "{b: 1}"),
        # a typed reference that would convert the value would have to change it
        "read-typed-conv": ("return get_softstring_ref(\\OurVarMod::hn.a.b);", ErrCode, "\"1\""),
        "read-static": ("return OurVarMod::Obj::sx;", "1", "1"),
        "read-static-ref": ("reference r = \\OurVarMod::Obj::sx; return r;", "1", "1"),
        "read-static-elem-ref": ("reference r = \\OurVarMod::Obj::sl[1]; return r;", "2", "2"),
        "read-static-typed-ref": ("return get_ref(\\OurVarMod::Obj::sx);", "1", "1"),
    };

    #! code that restricted code calls, which may write: (code, result in every restricted Program)
    /** The writes are made by code of other Programs that are not restricted, which decide what may change
    */
    const CalledForms = {
        # a function of the trusted host imported into the restricted Program
        "host-func": ("host_set_i();", "ok"),
        # a closure that a function of the trusted host creates
        "host-closure": ("get_host_closure()();", "ok"),
        # a function of a library module that sets another module's variable
        "lib-setter": ("LibMod::set_other(14);", "ok"),
        # a function of a module that the host loaded by path, outside the module path
        "path-setter": ("PathMod::set_other(15);", "ok"),
        # the module's own setters
        "module-setter": ("OurVarMod::set_i(42); OurVarMod::push_l(5);", "ok"),
        "static-setter": ("OurVarMod::Obj::set_sx(42);", "ok"),
        # a function of the module runs in the Program that calls it, with its restrictions: a static member is set
        # by a method of its class
        "module-func-static": ("OurVarMod::set_sx_func(42);", ErrCode),
    };

    #! the state after each of CalledForms, as "<OurVarMod::i>:<OurVarMod::Obj::sx>", when it is allowed
    const CalledState = {
        "host-func": "13:1",
        "host-closure": "13:1",
        "lib-setter": "14:1",
        "path-setter": "15:1",
        "module-setter": "42:1",
        "static-setter": "1:42",
        "module-func-static": "1:42",
    };

    #! the state after one of CalledForms that is refused
    const CalledRefusedState = "1:1";

    #! the helpers of the restricted code
    const HelperSource = "sub set_ref(reference r) { r = 8; }\n"
        "auto sub get_ref(reference r) { return r; }\n"
        "auto sub get_string_ref(reference<string> r) { return r; }\n"
        "auto sub get_softstring_ref(reference<softstring> r) { return r; }\n"
        "auto sub get_hash_ref(reference<hash<string, int>> r) { return r; }\n"
        "code sub get_closure() { return sub () { OurVarMod::i = 11; }; }\n";

    #! returns the name of a function of the restricted code
    string sub fname(string prefix, string form) {
        return prefix + replace(form, "-", "_");
    }

    #! returns the source of the restricted code: one function per form
    /** @param called_forms the keys of CalledForms whose functions are included, or all of them if not given
    */
    string sub get_writer_source(*list<string> called_forms) {
        string src = HelperSource;
        foreach hash<auto> i in (WriteForms.pairIterator()) {
            src += sprintf("sub %s() { %s }\n", fname("w_", i.key), i.value);
        }
        foreach hash<auto> i in (ReadForms.pairIterator()) {
            src += sprintf("auto sub %s() { %s }\n", fname("r_", i.key), i.value[0]);
        }
        foreach string form in (called_forms ?? keys CalledForms) {
            src += sprintf("sub %s() { %s }\n", fname("c_", form), CalledForms{form}[0]);
        }
        return src;
    }
}
