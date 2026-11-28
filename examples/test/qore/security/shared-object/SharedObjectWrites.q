# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Sandboxed code cannot change the members of an object that trusted code marked as shared with
# SandboxManager::shareObject(), or delete it, in any form of write, also through Reflection and the methods of the
# object's own class; reads work, trusted callers can change it, and an object that is not marked is not affected
# (qore#6843); see shared-object-writes.qtest.  The objects that are not shared that the tenant changes are its own
# (its sandbox created them), as the owner rules of tenant isolation (qore#6842) refuse it the deletion of an object
# that it did not create and the change of its non-public members; the order of the two rules is checked at the end:
# the shared-object rule answers first, then the owner rules

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

    #! changes a member in the code of the object's own class without an update scope
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
%requires reflection

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
        \"Reflection NormalMember::setValue()\": sub (auto o, SharedTarget t) {
            Reflection::Class c = Reflection::Class::getClass(o);
            cast<Reflection::NormalMember>(c.getMember(\"s\")).setValue(o, \"x\");
        },
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

# a copy of a shared object whose class declares no copy() method is refused (qore#7667; see
# shared-object-copy.qtest)
string sub change_copy(SharedTarget t) {
    return try_change(sub () { SharedTarget c = t.copy(); c.s = \"copy\"; });
}

# an object that the tenant creates: its sandbox owns it (qore#6842)
SharedTarget sub create_target() {
    return new SharedTarget();
}

# an object of a class of the tenant, whose destructor the tenant can call with Reflection
class TenantObject {
    public {
        string s = \"orig\";
    }

    destructor() {
    }
}

TenantObject sub create_tenant_object() {
    return new TenantObject();
}

string sub reflection_delete(object o) {
    return try_change(sub () {
        Reflection::DestructorMethod d = Reflection::Class::getClass(o).getDestructorMethod();
        d.call(o);
    });
}

string sub delete_object(object o) {
    return try_change(sub () { delete o; });
}

string sub delete_thread_data_object(object o) {
    save_thread_data({\"tenant-object\": o});
    string rv = try_change(sub () { delete_thread_data(\"tenant-object\"); });
    remove_thread_data(\"tenant-object\");
    return rv;
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
    # the same change of an object that is not marked, which the tenant created: neither the shared-object rule nor
    # the owner rules refuse it
    SharedTarget plain = p.callFunction("create_target");
    string plain_before = snapshot(plain);
    string plain_rv = p.callFunction("try_one", name, plain);
    string plain_after = snapshot(plain);
    printf("%s: shared: %s, %s; not shared: %s, %s\n", name, rv, after == before ? "unchanged" : "CHANGED", plain_rv,
        plain_after == plain_before ? "unchanged" : "changed");
}

SharedTarget shared = shared_target();
printf("reads: %s\n", p.callFunction("reads", shared));
printf("the object's own class code for the tenant: %s, s: %y\n", p.callFunction("own_class_code", shared), shared.s);
printf("the tenant copies it: %s, s: %y\n", p.callFunction("change_copy", shared), shared.s);
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

# the order of the checks: the shared-object rule answers first, whoever created the object, then the owner rules of
# tenant isolation, which refuse an object that is not shared that the tenant did not create

# the result of a check by the code of a Program, and whether the object still exists
string sub check(Program pgm, string f, SharedTarget t) {
    string rv = pgm.callFunction(f, t);
    return sprintf("%s (%s)", rv, snapshot(t) == "OBJECT-ALREADY-DELETED" ? "deleted" : "kept");
}

SharedTarget sub shared_own() {
    SharedTarget t = p.callFunction("create_target");
    SandboxManager::shareObject(t);
    return t;
}

foreach string f in ("delete_object", "delete_thread_data_object") {
    printf("order: %s: a shared object of the host: %s; a shared object of the tenant: %s; one of the host that is "
        "not shared: %s; one of the tenant that is not shared: %s\n", f, check(p, f, shared_target()),
        check(p, f, shared_own()), check(p, f, new SharedTarget()), check(p, f, p.callFunction("create_target")));
}
{
    # the explicit deletion with Reflection, which can only be of an object of the tenant's own classes
    object shared_own = p.callFunction("create_tenant_object");
    SandboxManager::shareObject(shared_own);
    object plain_own = p.callFunction("create_tenant_object");
    # a deleted object is passed as no value
    code exists_str = string sub (auto o) {
        try {
            return exists o.s ? "kept" : "deleted";
        } catch (hash<ExceptionInfo> ex) {
            return ex.err == "OBJECT-ALREADY-DELETED" ? "deleted" : ex.err;
        }
    };
    printf("order: Reflection destructor call: a shared object of the tenant: %s (%s); one of the tenant that is not "
        "shared: %s (%s)\n", p.callFunction("reflection_delete", shared_own), exists_str(shared_own),
        p.callFunction("reflection_delete", plain_own), exists_str(plain_own));
}
# at the enforcement level of code safety, the owner rules do not apply; the shared-object rule does
{
    Program cs(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    SandboxManager sm();
    sm.setEnforcementLevel(QSEL_CODE_SAFETY);
    cs.setSandboxManager(sm);
    cs.parse(TenantCode, "tenant-code-safety");
    printf("order at code safety: delete: a shared object of the host: %s; one of the host that is not shared: %s\n",
        check(cs, "delete_object", shared_target()), check(cs, "delete_object", new SharedTarget()));
}
