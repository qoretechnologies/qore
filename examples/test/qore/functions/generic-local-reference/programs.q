# Copyright (C) 2026 Qore Technologies, s.r.o.
# Bound constraints remain valid after the Program defining their type arguments is deleted.
%modern
%no-child-restrictions
const LibrarySource = "%modern\npublic code sub make_setter<T>() {\n    *T value;\n    return auto sub(auto input) { value = input; return value; };\n}\npublic code sub make_ref_setter<T>() {\n    *T value;\n    reference r = \\value;\n    return auto sub(auto input) { r = input; return r; };\n}\nsub export_to(Program pgm) {\n    pgm.importFunction(\"make_setter\");\n    pgm.importFunction(\"make_ref_setter\");\n}\n";
const UserSource = "%modern\nclass Foo { }\nhashdecl H { int x; }\nenum E { ONE = 1 }\nlist<auto> sub make() {\n    return (make_setter<Foo>(), make_ref_setter<Foo>(),\n        make_setter<hash<H>>(), make_ref_setter<hash<H>>(),\n        make_setter<E>(), make_ref_setter<E>());\n}\n";


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
        if (i < 4) {
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
