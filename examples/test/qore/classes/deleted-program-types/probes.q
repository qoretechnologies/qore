%modern

# values whose types name a class, hashdecl or enum declared in a Program that has been deleted (qore#5790)

const DeletedProgramTypesOpts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION;

public hashdecl DeletedProgramTypesHBox<T> {
    T v;
}

public class DeletedProgramTypesBox<T> {
    public {
        T v;
    }

    set(T x) {
        v = x;
    }

    T get() {
        return v;
    }
}

public class DeletedProgramTypesOther {
}

string sub deleted_program_types_try(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! values with the types of a class and an enum of a child Program that is deleted when this returns
hash<auto> sub deleted_program_types_make() {
    Program p(DeletedProgramTypesOpts);
    p.parse("class Foo { public { int x = 1; } }
        enum Color { Red = 1, Green = 2 }
        # declarations whose member types name Foo directly, with no derived type
        hashdecl DHD { int x = 1; *Foo f; }
        class DQ { public { *Foo f; } }
        hash<auto> sub make() {
            DeletedProgramTypesBox<*Foo> b();
            hash<DeletedProgramTypesHBox<*Foo>> h();
            list<*Foo> ol = (NOTHING,);
            list<Foo> l = ();
            hash<string, *Foo> sh = {};
            list<Color> cl = (Color::Red,);
            return {
                \"b\": b,
                \"h\": h,
                \"ol\": ol,
                \"l\": l,
                \"sh\": sh,
                \"cl\": cl,
                \"dh\": new hash<DHD>(),
                \"dq\": new DQ(),
            };
        }", "child");
    return p.callFunction("make");
}

#! a class imported from a Program that is deleted
Program sub deleted_program_types_import() {
    Program a(DeletedProgramTypesOpts);
    a.parse("class Q {
            private { int p = 4; }
            private:internal { int pi = 5; hash<string, int> h = {\"a\": 1}; }
            code mk(code c = auto sub () { return pi; }) { return c; }
            int get() { return p + pi; }
        }
        Program sub mkb(int opts) {
            Program b(opts);
            b.importClass(\"Q\");
            b.parse(\"our Q kq; our *Q oq; our list<*Q> lq = (NOTHING,);
                auto sub t() { kq = new Q(); return kq.get(); }
                auto sub u() { oq = NOTHING; lq += NOTHING; return lq.size(); }
                auto sub v() { return kq ? kq.mk()() : 0; }\", \"b\");
            return b;
        }", "a");
    return a.callFunction("mkb", DeletedProgramTypesOpts);
}

#! a hashdecl imported from a Program that is deleted
Program sub deleted_program_types_import_hashdecl() {
    Program a(DeletedProgramTypesOpts);
    a.parse("class Foo { }
        hashdecl HD { int x = 1; *Foo f; }
        Program sub mkb(int opts) {
            Program b(opts);
            b.importHashDecl(\"HD\");
            b.parse(\"auto sub t() { hash<HD> h(); h.f = NOTHING; return (h.x, h.f); }
                auto sub u(object o) { hash<HD> h(); h.f = o; return h.x; }\", \"b\");
            return b;
        }", "a");
    return a.callFunction("mkb", DeletedProgramTypesOpts);
}

string sub probes() {
    list<string> rv;
    hash<auto> r = deleted_program_types_make();
    # the generic object
    rv += deleted_program_types_try(auto sub () { r.b.set(NOTHING); return r.b.get(); });
    rv += deleted_program_types_try(auto sub () { r.b.set(new DeletedProgramTypesOther()); return r.b.get(); });
    # the hash of the concrete hashdecl
    rv += deleted_program_types_try(auto sub () { r.h.v = NOTHING; return r.h.fullType(); });
    rv += deleted_program_types_try(auto sub () { r.h.v = new DeletedProgramTypesOther(); return r.h.fullType(); });
    # the lists and the hash
    rv += deleted_program_types_try(auto sub () { r.ol += NOTHING; return (r.ol.size(), r.ol.fullType()); });
    rv += deleted_program_types_try(auto sub () { r.ol += new DeletedProgramTypesOther(); return r.ol.size(); });
    rv += deleted_program_types_try(auto sub () { r.l += NOTHING; return r.l.size(); });
    rv += deleted_program_types_try(auto sub () { return r.l.fullType(); });
    rv += deleted_program_types_try(auto sub () { r.sh.a = NOTHING; return (r.sh.size(), r.sh.fullType()); });
    rv += deleted_program_types_try(auto sub () { r.sh.b = 1; return r.sh.size(); });
    # the list of enum values
    rv += deleted_program_types_try(auto sub () { return (r.cl.size(), r.cl.fullType()); });
    rv += deleted_program_types_try(auto sub () { r.cl += 1; return r.cl.size(); });
    # the hash and the object whose member types name Foo directly
    rv += deleted_program_types_try(auto sub () { r.dh.f = NOTHING; return (r.dh.x, r.dh.f); });
    rv += deleted_program_types_try(auto sub () { r.dh.f = new DeletedProgramTypesOther(); return r.dh.f; });
    rv += deleted_program_types_try(auto sub () { r.dq.f = NOTHING; return r.dq.f; });
    rv += deleted_program_types_try(auto sub () { r.dq.f = new DeletedProgramTypesOther(); return r.dq.f; });
    remove r;

    # the imported class: its code is the code of the deleted Program
    Program b = deleted_program_types_import();
    foreach string f in ("u", "t", "v", "u") {
        rv += f + ": " + deleted_program_types_try(auto sub () { return b.callFunction(f); });
    }
    delete b;

    # the imported hashdecl
    Program hb = deleted_program_types_import_hashdecl();
    rv += deleted_program_types_try(auto sub () { return hb.callFunction("t"); });
    rv += deleted_program_types_try(auto sub () { return hb.callFunction("u", new DeletedProgramTypesOther()); });
    delete hb;

    return foldl $1 + "\n" + $2, rv;
}
