# Copyright (C) 2026 Qore Technologies, s.r.o.
# Bound constraints remain valid after the Program defining their type arguments is deleted; also for unions, lists
# and typed code that name a type argument, and typed references (reference<*T>, *list<reference<*T>>), bound
# alternately with a builtin type (int) and a class (qore#6126).
%modern
%no-child-restrictions
const LibrarySource = "%modern\npublic code sub make_setter<T>() {\n    *T value;\n    return auto sub(auto input) { value = input; return value; };\n}\npublic code sub make_ref_setter<T>() {\n    *T value;\n    reference r = \\value;\n    return auto sub(auto input) { r = input; return r; };\n}\npublic code sub make_union_setter<T>() {\n    *union<T, bool> value;\n    return auto sub(auto input) { value = input; return value; };\n}\npublic code sub make_list_setter<T>() {\n    *list<T> value;\n    return auto sub(auto input) { value = input; return value; };\n}\npublic code sub make_code_setter<T>() {\n    *code<int(T)> value;\n    return auto sub(auto input) { value = input; return value ? \"set\" : value; };\n}\npublic code sub make_typed_ref_setter<T>() {\n    *T value;\n    reference<*T> r = \\value;\n    *list<reference<*T>> refs = (\\value,);\n    return auto sub(auto input) { r = input; return refs ? value : NOTHING; };\n}\nsub export_to(Program pgm) {\n    pgm.importFunction(\"make_typed_ref_setter\");\n    pgm.importFunction(\"make_setter\");\n    pgm.importFunction(\"make_ref_setter\");\n    pgm.importFunction(\"make_union_setter\");\n    pgm.importFunction(\"make_list_setter\");\n    pgm.importFunction(\"make_code_setter\");\n}\n";
const UserSource = "%modern\nclass Foo { }\nhashdecl H { int x; }\nenum E { ONE = 1 }\nlist<auto> sub make() {\n    return (make_setter<Foo>(), make_ref_setter<Foo>(),\n        make_setter<hash<H>>(), make_ref_setter<hash<H>>(),\n        make_setter<E>(), make_ref_setter<E>(),\n        make_union_setter<int>(), make_union_setter<Foo>(), make_list_setter<int>(), make_list_setter<Foo>(),\n        make_code_setter<int>(), make_code_setter<Foo>(),\n        make_typed_ref_setter<int>(), make_typed_ref_setter<Foo>());\n}\n";


Program lib(PO_NO_CHILD_PO_RESTRICTIONS);
lib.parse(LibrarySource, "generic-storage-library");
list<auto> snapshots;
for (int generation = 0; generation < 3; ++generation) {
    Program user(PO_NO_CHILD_PO_RESTRICTIONS);
    lib.callFunction("export_to", user);
    string source = replace(replace(replace(UserSource, "Foo", "Foo" + string(generation)),
        "hashdecl H", "hashdecl H" + string(generation)), "hash<H>", "hash<H" + string(generation) + ">");
    user.parse(source, "generic-storage-user-" + string(generation));
    push snapshots, user.callFunction("make");
    delete user;
}
for (int generation = 0; generation < snapshots.size(); ++generation) {
    list<auto> setters = snapshots[generation];
    for (int i = 0; i < setters.size(); ++i) {
        code setter = setters[i];
        try { printf("%d/%d empty: %y\n", generation, i, setter(NOTHING)); }
        catch (hash<ExceptionInfo> ex) { printf("%d/%d empty: %s: %s\n", generation, i, ex.err, ex.desc); }
        # Enum value acceptance is intentionally unchanged; these two cells test declaration lifetime only.
        if (i != 4 && i != 5) {
            try { printf("%d/%d bad: %y\n", generation, i, setter("x")); }
            catch (hash<ExceptionInfo> ex) { printf("%d/%d bad: %s: %s\n", generation, i, ex.err, ex.desc); }
        }
    }
}
code kept = snapshots[0][0];
snapshots = ();
delete lib;
try { kept(NOTHING); printf("deleted-library: ran\n"); }
catch (hash<ExceptionInfo> ex) { printf("deleted-library: %s\n", ex.err); }

# A bound that mentions T is not a bare T alternative. Parse it in its own Program so
# this checks runtime specialization independently of qcc's encoding of wildcard bounds.
Program bounds(PO_NO_CHILD_PO_RESTRICTIONS);
bounds.parse("%modern\nclass Box<T> {}\n"
    "auto sub check<T>(auto input) { union<Box<T>, ? super T> value = input; return value; }\n"
    "auto sub run(auto input) { return check<string>(input); }\n", "wildcard-bound");
try { bounds.callFunction("run", NOTHING); printf("wildcard bound: accepted NOTHING\n"); }
catch (hash<ExceptionInfo> ex) { printf("wildcard bound: %s\n", ex.err); }
