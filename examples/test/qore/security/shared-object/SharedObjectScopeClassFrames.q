# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The code of a class that restricted code declared acts for restricted code in an update scope at any depth: the
# code that it calls, also trusted methods of the object's own class, cannot change the object, share objects or
# open another update scope, also when the scope's own code runs the class's code (static methods, closures and call
# references, constructors, member initializers, destructors, copy methods, serialization hooks); a thread that the
# scope's code starts is not in the scope; the scope's own code, also when restricted code opened the scope or runs a
# library's object built for it, still can (qore#7795); see shared-object-scope-class-frames.qtest
#
# usage: SharedObjectScopeClassFrames.q [<path of SharedObjectScopeClassFramesLib.q>]

%modern
%enable-all-warnings
%no-child-restrictions

%include SharedObjectScopeClassFramesLib.q

#! the tenant's code: a class whose methods call the methods of a shared object of the library
const TenantSource = "
class TenantBox {
    #! a method of the object's class directly below the tenant's frame
    string poke(object c) {
        return c.set('tenant');
    }

    #! a method of the object's class that calls another one
    string relay(object c) {
        return c.relay('tenant');
    }

    #! a plain function of the tenant between its class and the method of the object's class
    string deep(object c) {
        return tenant_set(c);
    }

    #! another class of the library between the tenant's class and the method of the object's class
    string library(object c, object helper) {
        return helper.run(c);
    }

    #! a method of the object's class that shares an object
    string share(object c, object o) {
        return c.shareIt(o);
    }

    #! a method of the object's class that opens a nested update scope
    string scope(object c) {
        return c.nested();
    }

    #! calls a method of the object's class that raises an exception
    boom(object c) {
        c.throwIt();
    }

    #! the tenant opens an update scope through the object's class, whose own code calls the tenant's code that
    #! raises an exception, and then changes the object
    string ownAfterException(object c) {
        return c.ownAfterException(self);
    }

    #! the tenant opens an update scope through the object's class, whose own code runs in it
    string own(object c, *object helper) {
        return c.own(helper);
    }

    #! the tenant opens an update scope through the object's class, whose own code starts a thread that changes the
    #! object
    string ownThread(object c) {
        return c.ownThread();
    }

    #! a static method of the class that changes the object
    static string staticSet(object c) {
        return c.set('tenant');
    }

    #! a call reference to the static method
    code staticRef() {
        return \\TenantBox::staticSet();
    }

    #! a closure that the class's code creates
    code closure() {
        return string sub (object c) { return c.set('tenant'); };
    }

    #! a call reference to a method of the object that the class's code creates
    code methodRef() {
        return \\self.poke();
    }

    #! an object whose destructor changes the object
    TenantTrap trapObject(object c) {
        trap_result = '';
        return new TenantTrap(c);
    }

    #! an object whose copy method changes the object
    TenantCopy copyObject(object c) {
        return new TenantCopy(c);
    }

    #! an object whose serialization hook changes the object
    TenantSerializable serializableObject(object c) {
        trap_result = '';
        return new TenantSerializable(c);
    }

    #! a reference to a plain function that creates an object whose constructor changes the object
    code makerRef() {
        return \\tenant_make();
    }

    #! a reference to a plain function that creates an object whose member initializer changes the object
    code initRef() {
        return \\tenant_init();
    }

    #! the result that the last destructor or serialization hook recorded
    string trap() {
        return trap_result;
    }
}

our string trap_result = '';
our object trap_target;

class TenantTrap {
    private {
        object c;
    }

    constructor(object c) {
        self.c = c;
    }

    destructor() {
        trap_result = c.set('tenant');
    }
}

class TenantCopy {
    public {
        object c;
        string r = '';
    }

    constructor(object c) {
        self.c = c;
    }

    copy() {
        r = c.set('tenant');
    }
}

class TenantSerializable inherits Serializable {
    public {
        string x = 'x';
        transient object c;
    }

    constructor(object c) {
        self.c = c;
    }

    private:internal *hash<auto> serializeMembers(*hash<auto> members) {
        trap_result = c.set('tenant');
        return members;
    }
}

class TenantMaker {
    public {
        string r;
    }

    constructor(object c) {
        r = c.set('tenant');
    }
}

class TenantInit {
    public {
        string r = trap_target.set('tenant');
    }
}

TenantMaker sub tenant_make(object c) {
    return new TenantMaker(c);
}

TenantInit sub tenant_init(object c) {
    trap_target = c;
    return new TenantInit();
}

string sub tenant_set(object c) {
    return c.set('tenant');
}

TenantBox sub tenant_box() {
    return new TenantBox();
}
";

#! the methods of the tenant's class that run in an update scope
const Methods = ("poke", "relay", "deep", "library", "share", "scope");

SandboxManager sub manager(bool trusted) {
    SandboxManager sm();
    sm.setFilesystemDefaultPolicy(True);
    sm.setNetworkDefaultPolicy(True);
    if (trusted) {
        sm.setUncontrolledApiPolicy(True);
    }
    return sm;
}

Program sub tenant(SandboxManager sm) {
    Program p(PO_NEW_STYLE | PO_ALLOW_REPARSE);
    p.setSandboxManager(sm);
    p.parse(TenantSource, "tenant");
    return p;
}

#! runs each method of the tenant's object in an update scope of a shared object that \a new_cache creates
sub check(string what, object box, code new_cache, code new_relay) {
    foreach string method in (Methods) {
        object c = new_cache();
        string rv = c.update(box, method, new_relay());
        printf("%s: %s: %s, the object: %y\n", what, method, rv, c.v);
    }
    # outside a scope
    object c = new_cache();
    printf("%s: outside a scope: %s, the object: %y\n", what, box.poke(c), c.v);
    # the tenant opens a scope whose own code changes the object, also with a library object that the tenant created
    c = new_cache();
    printf("%s: the tenant opens a scope: %s, the object: %y\n", what, box.own(c), c.v);
    c = new_cache();
    printf("%s: the tenant opens a scope with its library object: %s, the object: %y\n", what, box.own(c,
        new_relay()), c.v);
    c = new_cache();
    printf("%s: the tenant opens a scope and its code raises an exception in it: %s, the object: %y\n", what,
        box.ownAfterException(c), c.v);
    # a thread that the scope's own code starts is not in the scope
    c = new_cache();
    printf("%s: the tenant opens a scope whose code starts a thread: %s, the object: %y\n", what, box.ownThread(c),
        c.v);
    # code of the tenant's class that the scope's own code runs
    run_code(what, "a static method", box, new_cache, "code", box.staticRef());
    run_code(what, "a closure of the class", box, new_cache, "code", box.closure());
    run_code(what, "a call reference of the class", box, new_cache, "code", box.methodRef());
    run_code(what, "a constructor", box, new_cache, "construct", box.makerRef());
    run_code(what, "a member initializer", box, new_cache, "construct", box.initRef());
    c = new_cache();
    printf("%s: a destructor: %s, the object: %y\n", what, c.update(box, "delete", box.trapObject(c)), c.v);
    c = new_cache();
    printf("%s: a copy method: %s, the object: %y\n", what, c.update(box, "copy", box.copyObject(c)), c.v);
    c = new_cache();
    printf("%s: a serialization hook: %s, the object: %y\n", what, c.update(box, "serialize",
        box.serializableObject(c)), c.v);
}

#! runs code of the tenant's class in an update scope
sub run_code(string what, string desc, object box, code new_cache, string method, code code) {
    object c = new_cache();
    string rv;
    if (method == "construct") {
        rv = c.update(box, "code", string sub (object cache) { return code(cache).r; });
    } else {
        rv = c.update(box, method, code);
    }
    printf("%s: %s: %s, the object: %y\n", what, desc, rv, c.v);
}

string lib_path = ARGV[0] ?? get_script_dir() + "/SharedObjectScopeClassFramesLib.q";
string lib_source = ReadOnlyFile::readTextFile(lib_path);

# the tenant's Program is restricted: its Program is entered in the scope
{
    Program p = tenant(manager(False));
    check("restricting", p.callFunction("tenant_box"), \new_cache(), \new_relay());
}

# the tenant's Program is no longer restricted, but the class that restricted code declared is
{
    SandboxManager sm = manager(False);
    Program p = tenant(sm);
    object box = p.callFunction("tenant_box");
    sm.setUncontrolledApiPolicy(True);
    check("relaxed", box, \new_cache(), \new_relay());
}

# the class that restricted code declared is in the Program of the scope, whose manager was relaxed before the
# library's code was parsed into it
{
    SandboxManager sm = manager(False);
    Program p = tenant(sm);
    object box = p.callFunction("tenant_box");
    sm.setUncontrolledApiPolicy(True);
    p.parse(lib_source, "library");
    check("same Program", box, object sub () { return p.callFunction("new_cache"); },
        object sub () { return p.callFunction("new_relay"); });
}

# the class that the code of a trusted (governed) sandbox declared is not restricted
{
    Program p = tenant(manager(True));
    check("trusted", p.callFunction("tenant_box"), \new_cache(), \new_relay());
}

# as above, in the Program of the scope
{
    Program p = tenant(manager(True));
    p.parse(lib_source, "library");
    check("trusted, same Program", p.callFunction("tenant_box"), object sub () { return p.callFunction("new_cache"); },
        object sub () { return p.callFunction("new_relay"); });
}
