#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the script of reflection-typed-hash-module-caller.qtest: library module code (DataProvider and a test module) uses
# typed hash handles for the Program that calls it; prints one result per case

%modern
%requires DataProvider
%requires reflection
%requires ThReflectFixture
%requires Util

our int init_count = 0;
public int sub parent_init() { return ++init_count; }

#! the trusted Program's typed hashes; the initializer of the first runs the trusted Program's code
public hashdecl ParentDecl {
    int m = parent_init();
}
public hashdecl ParentNdDecl {
    string m;
}

# a module that is not library code (loaded by its path, from outside the module path) that calls library module code
# (the test gives its directory, as the script's directory is another one for a compiled executable)
load_module((ENV.TH_REFLECT_PATH_MODULE_DIR
    ?? normalize_dir(get_script_dir() + "/../reflection-typed-hash-module-caller-path")) + "/ThReflectPathFixture.qm");

# the functions of every tenant Program; typed hashes without member defaults are used with DataProvider's
# HashDataType (which reads the defaults), those with defaults with the test module, which returns the values
const TenantFunctions = "%modern
%requires DataProvider
%requires reflection
%requires ThReflectFixture
%requires ThReflectPathFixture
string sub run(code c) {
    try {
        return sprintf('ok: %y', c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
Reflection::TypedHash sub typed_hash(string decl) {
    return new Reflection::TypedHash(decl);
}
Reflection::TypedHashMember sub member(string decl) {
    return (new Reflection::TypedHash(decl)).getMember('m');
}
string sub add_field(Reflection::TypedHashMember mem) {
    return run(auto sub () {
        DataProvider::HashDataType h('h');
        h.addField(mem);
        return h.getFields().keys();
    });
}
string sub add_own(string decl) {
    return add_field(member(decl));
}
string sub add_own_for_name(string decl) {
    return add_field(Reflection::TypedHash::forName(decl).getMember('m'));
}
string sub hash_type(Reflection::TypedHash th) {
    return run(auto sub () { return (new DataProvider::HashDataType(th)).getFields().keys(); });
}
string sub hash_type_own(string decl) {
    return hash_type(new Reflection::TypedHash(decl));
}
string sub module_default(Reflection::TypedHashMember mem) {
    return ThReflectFixture::default_of(mem);
}
string sub module_default_own(string decl) {
    return module_default(member(decl));
}
string sub module_cast(Reflection::TypedHash th) {
    return ThReflectFixture::cast(th);
}
string sub module_cast_own(string decl) {
    return module_cast(new Reflection::TypedHash(decl));
}
# the typed hash of a value: its handle is one of the Program that declares the typed hash
string sub module_default_of_value(hash<auto> v) {
    return ThReflectFixture::default_of(Reflection::Type::getType(v).getTypedHash().getMember('m'));
}
string sub module_stored(string decl) {
    ThReflectFixture::store(member(decl));
    return ThReflectFixture::default_of_stored();
}
string sub module_thread(string decl) {
    ThReflectFixture::store(member(decl));
    return ThReflectFixture::default_of_stored_in_thread();
}
string sub module_init(string decl) {
    ThReflectFixture::store(member(decl));
    return ThReflectFixture::default_of_stored_in_module_init();
}
string sub path_module_own(string decl) {
    return ThReflectPathFixture::default_of(member(decl));
}
string sub path_module_direct_own(string decl) {
    return ThReflectPathFixture::default_of_direct(member(decl));
}
# a closure of this Program that has module code use the member
code sub module_default_closure(Reflection::TypedHashMember mem) {
    return string sub () { return ThReflectFixture::default_of(mem); };
}
# module code calls the closure of another Program back for this Program
string sub module_call_back(code c) {
    return ThReflectFixture::call_back(c);
}
";

# the typed hashes of a tenant Program that does not inherit the trusted Program's typed hashes
const TenantSource = TenantFunctions + "
hashdecl NdDecl { string m; }
public hashdecl PublicNdDecl { string m; }
hashdecl DefDecl { string m = 'd'; }
public hashdecl PublicDefDecl { string m = 'p'; }
# the name of the trusted Program's typed hash
hashdecl ParentDecl { string m = 'tenant'; }
";

string sub call(Program p, string label, string func, auto arg) {
    try {
        return sprintf("%s: %s", label, p.callFunction(func, arg));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", label, ex.err);
    }
}

#! a tenant Program; it inherits the trusted Program's typed hashes only if \a inherit is set
Program sub tenant(*SandboxManager sm, bool inherit = False) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION
        | PO_NO_CHILD_PO_RESTRICTIONS | (inherit ? 0 : PO_NO_INHERIT_USER_HASHDECLS));
    if (sm) {
        p.setSandboxManager(sm);
    }
    p.parse(inherit ? TenantFunctions : TenantSource, "tenant");
    return p;
}

sub run(string label, bool sandbox) {
    SandboxManager sm();
    Program a = tenant(sandbox ? sm : NOTHING);
    # another Program with typed hashes of the same names and members, governed by the same sandbox manager
    Program b = tenant(sandbox ? sm : NOTHING);
    # a Program with a sandbox manager of its own
    Program c = tenant(sandbox ? new SandboxManager() : NOTHING);
    # a Program that inherits the trusted Program's typed hashes
    Program i = tenant(sandbox ? sm : NOTHING, True);
    Reflection::TypedHash parent_th("ParentDecl");
    Reflection::TypedHashMember parent_mem = parent_th.getMember("m");
    Reflection::TypedHash parent_nd_th("ParentNdDecl");
    hash<ParentDecl> parent_value = <ParentDecl>{"m": 0};
    list<string> out = (
        # the Program's own typed hashes, used by library module code for it
        call(a, "own addField", "add_own", "NdDecl"),
        call(a, "own public addField", "add_own", "PublicNdDecl"),
        call(a, "own forName addField", "add_own_for_name", "NdDecl"),
        call(a, "own public forName addField", "add_own_for_name", "PublicNdDecl"),
        call(a, "own HashDataType", "hash_type_own", "NdDecl"),
        call(a, "own public HashDataType", "hash_type_own", "PublicNdDecl"),
        call(a, "own module default", "module_default_own", "DefDecl"),
        call(a, "own public module default", "module_default_own", "PublicDefDecl"),
        call(a, "own module cast", "module_cast_own", "DefDecl"),
        call(a, "own handle module default", "module_default", a.callFunction("member", "DefDecl")),
        call(a, "own stored module default", "module_stored", "DefDecl"),
        # another Program's typed hashes of the same names: another Program's handles
        call(a, "other same-manager addField", "add_field", b.callFunction("member", "NdDecl")),
        call(a, "other same-manager module default", "module_default", b.callFunction("member", "DefDecl")),
        call(a, "other same-manager module cast", "module_cast", b.callFunction("typed_hash", "DefDecl")),
        call(a, "other other-manager addField", "add_field", c.callFunction("member", "NdDecl")),
        call(a, "other other-manager module default", "module_default", c.callFunction("member", "DefDecl")),
        # module code called by one Program calls back a closure of another, which has module code use a handle: the
        # module code acts for the Program of the closure
        call(b, "other closure own handle via callback", "module_call_back",
            a.callFunction("module_default_closure", a.callFunction("member", "DefDecl"))),
        call(b, "other closure caller's handle via callback", "module_call_back",
            a.callFunction("module_default_closure", b.callFunction("member", "DefDecl"))),
        # the trusted Program's typed hashes: their initializers are not run for a sandbox, also not when the value
        # or the name comes from the tenant
        call(a, "trusted addField", "add_field", parent_nd_th.getMember("m")),
        call(a, "trusted HashDataType", "hash_type", parent_nd_th),
        call(a, "trusted module default", "module_default", parent_mem),
        call(a, "trusted module cast", "module_cast", parent_th),
        call(a, "trusted module default of value", "module_default_of_value", parent_value),
        # the tenant's own typed hash of the trusted typed hash's name
        call(a, "own same-name module default", "module_default_own", "ParentDecl"),
        # an inherited typed hash: the Program's own handle is one it could cast itself, the trusted Program's is not
        call(i, "inherited own handle addField", "add_own", "ParentNdDecl"),
        call(i, "inherited own handle module default", "module_default_own", "ParentDecl"),
        call(i, "inherited trusted handle addField", "add_field", parent_nd_th.getMember("m")),
        call(i, "inherited trusted handle module default", "module_default", parent_mem),
        # library module code that runs for no application Program
        call(a, "own module thread", "module_thread", "DefDecl"),
        # module code that is not library code is calling code: when it calls library module code, and when it uses
        # the handle itself
        call(a, "own path module default", "path_module_own", "DefDecl"),
        call(a, "own path module direct default", "path_module_direct_own", "DefDecl"),
    );
    if (sandbox) {
        # the initialization of a library module runs for no application Program (a module is initialized once)
        out += call(a, "own module init", "module_init", "DefDecl");
    }
    foreach string line in (out) {
        printf("%s %s\n", label, line);
    }
}

run("sandbox", True);
run("trusted", False);
printf("trusted initializer calls: %d\n", init_count);
