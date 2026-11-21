# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Sandboxed code cannot change the members of an object that trusted code marked as shared with
# SandboxManager::shareObject(), or delete it, in any form of write; reads work, the object's own class code and
# trusted callers can change it, and an object that is not marked is not affected (qore#6843); see
# shared-object-writes.qtest

%modern
%enable-all-warnings
%no-child-restrictions

public class SharedInner {
    public {
        string x = "inner";
    }
}

public class SharedTarget {
    public {
        string s = "orig";
        int i = 1;
        hash<auto> h = {"k": "v"};
        list<auto> l = (1, 2, 3);
        SharedInner obj();
    }

    #! changes a member in the code of the object's own class, as a cache that it builds would be
    cacheValue(string v) {
        s = v;
    }

    #! changes a member on behalf of the caller, which it refuses for sandboxed code
    setValue(string v) {
        SandboxManager::checkSharedObjectChange(self, "set the value");
        s = v;
    }

    string snapshot() {
        return sprintf("s: %y i: %y h: %y l: %y obj.x: %y members: %y", s, i, h, l, obj.x, keys self);
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# a class that sandboxed code declares, which inherits the class of the shared object
class TenantSubclass inherits SharedTarget {
    static poke(SharedTarget other) {
        other.s = \"subclass\";
    }
}

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

sub write_through(reference<auto> r) {
    r = \"through a reference\";
}

auto sub read_through(reference<auto> r) {
    return r;
}

# the changes, by form; o is the object untyped, t the same object typed
hash<string, code> sub changes() {
    return {
        \"assignment\": sub (auto o, SharedTarget t) { o.s = \"x\"; },
        \"typed assignment\": sub (auto o, SharedTarget t) { t.s = \"x\"; },
        \"typed scalar assignment\": sub (auto o, SharedTarget t) { t.i = 5; },
        \"+=\": sub (auto o, SharedTarget t) { o.i += 2; },
        \"typed +=\": sub (auto o, SharedTarget t) { t.i += 2; },
        \"++\": sub (auto o, SharedTarget t) { o.i++; },
        \"typed ++\": sub (auto o, SharedTarget t) { t.i++; },
        \"nested hash element\": sub (auto o, SharedTarget t) { o.h.k = \"x\"; },
        \"typed nested hash element\": sub (auto o, SharedTarget t) { t.h{\"new\"} = \"x\"; },
        \"list element\": sub (auto o, SharedTarget t) { o.l[0] = 9; },
        \"push\": sub (auto o, SharedTarget t) { push o.l, 4; },
        \"typed push\": sub (auto o, SharedTarget t) { push t.l, 4; },
        \"splice\": sub (auto o, SharedTarget t) { splice o.l, 0, 1; },
        \"extract\": sub (auto o, SharedTarget t) { auto v = extract o.l, 0, 1; },
        \"remove\": sub (auto o, SharedTarget t) { auto v = remove o.s; },
        \"typed remove\": sub (auto o, SharedTarget t) { auto v = remove t.s; },
        \"delete member\": sub (auto o, SharedTarget t) { delete o.h; },
        \"remove slice\": sub (auto o, SharedTarget t) { auto v = remove o{\"s\", \"i\"}; },
        \"delete slice\": sub (auto o, SharedTarget t) { delete o{\"s\", \"i\"}; },
        \"reference\": sub (auto o, SharedTarget t) { reference<auto> r = \\o.s; r = \"x\"; },
        \"reference argument\": sub (auto o, SharedTarget t) { write_through(\\o.s); },
        \"+= hash\": sub (auto o, SharedTarget t) { o += {\"s\": \"x\"}; },
        \"member of a held object through the object\": sub (auto o, SharedTarget t) { o.obj.x = \"x\"; },
        \"method of a tenant subclass\": sub (auto o, SharedTarget t) { TenantSubclass::poke(t); },
        \"guarded method\": sub (auto o, SharedTarget t) { t.setValue(\"x\"); },
        \"delete\": sub (auto o, SharedTarget t) { delete o; },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, object o) {
    return try_change(sub () { changes(){name}(o, o); });
}

# reads of a shared object
string sub reads(SharedTarget t) {
    auto o = t;
    return sprintf(\"%y %y %y %y %y %y %y\", o.s, t.i, o.h.k, t.l[1], o.obj.x, read_through(\\o.s), keys o);
}

# the code of the object's own class, called by the tenant
string sub own_class_code(SharedTarget t) {
    return try_change(sub () { t.cacheValue(\"cached\"); });
}

# a copy of a shared object is not shared
string sub change_copy(SharedTarget t) {
    SharedTarget c = t.copy();
    return try_change(sub () { c.s = \"copy\"; });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

SharedTarget sub shared_target() {
    SharedTarget t();
    SandboxManager::shareObject(t);
    return t;
}

printf("a new object is shared: %y; a marked one: %y\n", SandboxManager::isSharedObject(new SharedTarget()),
    SandboxManager::isSharedObject(shared_target()));

# the state of an object, or the exception if it was deleted
string sub snapshot(SharedTarget t) {
    try {
        return t.snapshot();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string name in (p.callFunction("change_names")) {
    SharedTarget target = shared_target();
    string before = snapshot(target);
    string rv = p.callFunction("try_one", name, target);
    string after = snapshot(target);
    # the same change of an object that is not marked
    SharedTarget plain();
    string plain_before = snapshot(plain);
    string plain_rv = p.callFunction("try_one", name, plain);
    string plain_after = snapshot(plain);
    printf("%s: shared: %s, %s; not shared: %s, %s\n", name, rv, after == before ? "unchanged" : "CHANGED", plain_rv,
        plain_after == plain_before ? "unchanged" : "changed");
}

SharedTarget shared = shared_target();
printf("reads: %s\n", p.callFunction("reads", shared));
printf("the object's own class code for the tenant: %s, s: %y\n", p.callFunction("own_class_code", shared), shared.s);
printf("the tenant changes its copy: %s, s: %y\n", p.callFunction("change_copy", shared), shared.s);
printf("a trusted caller: %s\n", (string sub () {
    try {
        shared.s = "trusted";
        shared.i += 1;
        push shared.l, 4;
        remove shared.h.k;
        shared += {"i": 10};
        shared.setValue("trusted method");
        return sprintf("changed: %s", shared.snapshot());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
SharedTarget deleted = shared_target();
SharedTarget other_ref = deleted;
delete deleted;
printf("trusted code deletes a shared object: %s\n", snapshot(other_ref));
