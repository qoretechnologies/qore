# -*- mode: qore; indent-tabs-mode: nil -*-
# the checks of class-signature-member-types.qtest; included by probe.q, which declares the classes of classes.ql
# itself, and by module-user.q, which loads them from the module MemberTypeClasses; the first argument is the
# directory of classes.ql

# the options of the Programs that parse the declarations, those of this program (%modern): nothing of this program
# is imported into them
const PgmOpts = PO_MODERN | PO_NO_USER_API | PO_NO_CHILD_PO_RESTRICTIONS;

#! the declarations of two classes MTP::Foo that differ only in one declared type (qore#7457)
const Pairs = (
    {"label": "list<int> vs hash<string, int> member", "a": "public { list<int> v; }",
        "b": "public { hash<string, int> v; }"},
    {"label": "list<int> vs list<string> member", "a": "public { list<int> v; }", "b": "public { list<string> v; }"},
    {"label": "list<int> vs *list<int> member", "a": "public { list<int> v; }", "b": "public { *list<int> v; }"},
    {"label": "hashdecl member", "decls": "hashdecl HA { int a; } hashdecl HB { int a; }",
        "a": "public { hash<HA> v; }", "b": "public { hash<HB> v; }"},
    {"label": "class member", "decls": "class X {} class Y {}", "a": "public { *X v; }", "b": "public { *Y v; }"},
    {"label": "class member, same name in other namespaces",
        "decls": "namespace N1 { class X {} } namespace N2 { class X {} }", "a": "public { *N1::X v; }",
        "b": "public { *N2::X v; }"},
    {"label": "class member, class declared later", "after": "class X {} class Y {}", "a": "public { *X v; }",
        "b": "public { *Y v; }"},
    {"label": "enum member", "decls": "enum E1 { A = 1 } enum E2 { A = 1 }", "a": "public { E1 v = E1::A; }",
        "b": "public { E2 v = E2::A; }"},
    {"label": "union member", "a": "public { union<int, string> v; }", "b": "public { union<int, bool> v; }"},
    {"label": "private member", "a": "private { list<int> v; }", "b": "private { hash<string, int> v; }"},
    {"label": "static variable", "a": "public { static list<int> v; }",
        "b": "public { static hash<string, int> v; }"},
    {"label": "method return type", "a": "int m() { return 1; }", "b": "string m() { return \"1\"; }"},
    {"label": "static method return type", "a": "static list<int> m() { return (); }",
        "b": "static *list<int> m() {}"},
    {"label": "generic parent class, type arguments of the same name in other namespaces",
        "decls": "namespace N1 { class X {} } namespace N2 { class X {} } class Box<T> { public { *T v; } }",
        "a": "", "ia": "inherits Box<N1::X>", "b": "", "ib": "inherits Box<N2::X>"},
);

#! returns the source of a Program with the class MTP::Foo declared with the given parent classes and members
string sub pair_source(hash<auto> pair, *string parents, string members) {
    return sprintf("%s namespace MTP { class Foo %s { %s } } %s MTP::Foo sub make() { return new MTP::Foo(); } "
        "string sub take(MTP::Foo f) { return \"accepted\"; }", pair.decls ?? "", parents ?? "", members,
        pair.after ?? "");
}

#! returns a new Program that parsed the given source
Program sub parse_program(string src, string label) {
    Program p(PgmOpts);
    p.parse(src, label);
    return p;
}

#! returns the result of passing an object to take() of a Program, or the error raised
string sub pass(Program p, object o) {
    try {
        return p.callFunction("take", o);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub check_pairs() {
    foreach hash<auto> pair in (Pairs) {
        Program a = parse_program(pair_source(pair, pair.ia, pair.a), "A");
        Program b = parse_program(pair_source(pair, pair.ib, pair.b), "B");
        Program a2 = parse_program(pair_source(pair, pair.ia, pair.a), "A2");
        object o = a.callFunction("make");
        printf("%s: other declaration: %s\n", pair.label, pass(b, o));
        printf("%s: same declaration: %s\n", pair.label, pass(a2, o));
        printf("%s: other declaration (reverse): %s\n", pair.label, pass(a, b.callFunction("make")));
    }
}

#! the same declaration parsed with and without PO_REQUIRE_TYPES or PO_REQUIRE_PROTOTYPES is the same class: the
#! nothing return type that the options imply for a method without a declared return type is not in the signature
#! (qore#7457)
sub check_parse_options() {
    string src = "namespace MTP { class Foo { public { int i = 1; } constructor() {} m() {} int n() { return 1; } } } "
        "MTP::Foo sub make() { return new MTP::Foo(); } string sub take(MTP::Foo f) { return \"accepted\"; }";
    hash<string, int> opts = {
        "no option": 0,
        "PO_REQUIRE_TYPES": PO_REQUIRE_TYPES,
        "PO_REQUIRE_PROTOTYPES": PO_REQUIRE_PROTOTYPES,
    };
    hash<string, Program> pgms;
    foreach hash<auto> i in (opts.pairIterator()) {
        pgms{i.key} = new Program(PO_NEW_STYLE | PO_NO_USER_API | PO_NO_CHILD_PO_RESTRICTIONS | i.value);
        pgms{i.key}.parse(src, i.key);
    }
    foreach string from in (keys opts) {
        object o = pgms{from}.callFunction("make");
        foreach string to in (keys opts) {
            if (from == to) {
                continue;
            }
            printf("parsed with %s, taken by a parse with %s: %s\n", from, to, pass(pgms{to}, o));
        }
    }
}

#! true if the signature hash of a class of this program is that of the class of a Program (true without the debug
#! hook dbg_class_signature() of a debug build)
bool sub same_hash(Program p, string path) {
    if (!exists_function("dbg_class_signature")) {
        return True;
    }
    *string h = call_function("dbg_class_signature", path).hash;
    return exists h && h == p.callFunction("sig_hash", path);
}

#! the classes of this program (compiled by qcc in an executable or a module) are those of the same source parsed at
#! run time
sub check_same_source(string dir) {
    string src = ReadOnlyFile::readTextFile(dir + "/classes.ql")
        + "string sub take(MTC::Full f) { return \"accepted \" + f.count(); } "
        + "string sub take_later(MTC::Later l) { return \"accepted\"; } "
        + "string sub take_derived(MTC::Derived d) { return \"accepted\"; } "
        + "string sub take_many(MTC::Many m) { return \"accepted \" + m.alpha(1); } "
        + "MTC::Full sub make() { MTC::Full f(); f.li = (1, 2); return f; } "
        + "*string sub sig_hash(string path) { return call_function(\"dbg_class_signature\", path).hash; }";
    Program p = parse_program(src, "classes.ql");
    foreach string path in (("MTC::Full", "MTC::Many", "MTC::Later", "MTC::Base", "MTC::Derived")) {
        printf("the signature hash of %s in a runtime parse: %s\n", path, same_hash(p, path) ? "same" : "different");
    }
    printf("this program's Many in a runtime parse: %s\n", p.callFunction("take_many", new MTC::Many()));
    MTC::Full f();
    f.li = (1, 2, 3);
    printf("this program's Full in a runtime parse: %s\n", p.callFunction("take", f));
    printf("this program's Later in a runtime parse: %s\n", p.callFunction("take_later", new MTC::Later()));
    # the subclass that the static variable initializer of its parent class initializes (qore#7457)
    printf("this program's Derived in a runtime parse: %s\n", p.callFunction("take_derived", new MTC::Derived()));
    MTC::Full rf = p.callFunction("make");
    printf("the runtime parse's Full in this program: %d\n", rf.count());
    *MTC::Later l = MTC::Full::make();
    printf("this program's static method: %s\n", l.className());
}

check_pairs();
check_parse_options();
check_same_source(ARGV[0]);
