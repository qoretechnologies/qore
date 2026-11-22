# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Code that sandboxed code calls in an update scope of a shared object acts for sandboxed code: it cannot change the
# object, open another update scope or share objects, also when it is code of the object's class; only trusted code
# shares objects; SandboxManager::isCalledBySandboxedCode() gives the sandbox of the code that called a method; the
# per-Program cache keeps the values of an object for one Program, for trusted code of the Program of a class of the
# object only, bounded and released with the Program (qore#6843); see shared-object-scope-frames.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection

public class SharedCache {
    public {
        string cached = "orig";
    }

    #! updates the cache in an update scope
    fill() {
        SandboxManager::updateSharedObject(self, sub () { cached = "filled"; });
    }

    #! changes the cache without an update scope
    fillUnscoped() {
        cached = "unscoped";
    }

    #! runs a callback of its caller in an update scope, and changes the object after it
    callBack(code callback) {
        SandboxManager::updateSharedObject(self, sub () {
            callback();
            cached += " after the callback";
        });
    }

    #! releases the value that it is given in an update scope
    dropInScope(*object o) {
        SandboxManager::updateSharedObject(self, sub () { remove o; });
    }

    #! returns whether the code that called this method is sandboxed
    string who() {
        return SandboxManager::isCalledBySandboxedCode() ? "sandboxed" : "trusted";
    }

    #! runs a callback of its caller
    auto runCallback(code callback) {
        return callback();
    }

    string snapshot() {
        return sprintf("cached: %y", cached);
    }
}

#! trusted code that calls a method for its caller
public class TrustedHelper {
    static string who(SharedCache t) {
        return t.who();
    }
}

#! a class that keeps values in the per-Program cache of its callers
public class ProgramCacheOwner {
    set(string key, auto value, int max = 64) {
        SandboxManager::setProgramCacheValue(self, key, value, max);
    }

    auto get(string key) {
        return SandboxManager::getProgramCacheValue(self, key);
    }
}

#! a value that reports when it is released
public class Tracker {
    private {
        Queue q;
        string name;
        bool fail;
    }

    constructor(Queue q, string name, bool fail = False) {
        self.q = q;
        self.name = name;
        self.fail = fail;
    }

    destructor() {
        q.push(name);
        if (fail) {
            throw "TRACKER-ERROR", sprintf("%s is released with an error", name);
        }
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires reflection

our list<string> events = ();

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

class TenantObject {
    private {
        SharedCache t;
    }
    constructor(SharedCache t) {
        self.t = t;
    }
    destructor() {
        events += \"the destructor in the scope: \" + try_change(sub () { t.fillUnscoped(); });
    }
}

class TenantSubclass inherits ProgramCacheOwner {
    auto readOther(ProgramCacheOwner o, string key) {
        return SandboxManager::getProgramCacheValue(o, key);
    }
}

string sub callback_writes(SharedCache t) {
    return try_change(sub () { t.callBack(sub () { t.fillUnscoped(); }); });
}

string sub callback_opens_scope(SharedCache t) {
    return try_change(sub () { t.callBack(sub () { t.fill(); }); });
}

string sub callback_shares(SharedCache t) {
    string rv;
    t.callBack(sub () {
        rv = sprintf(\"can share: %y, \", SandboxManager::canShareObjects())
            + try_change(sub () { SandboxManager::shareObject(new SharedCache()); });
    });
    return rv;
}

string sub callback_returns(SharedCache t) {
    return try_change(sub () { t.callBack(sub () {}); });
}

list<string> sub destructor_in_scope(SharedCache t) {
    events = ();
    t.dropInScope(new TenantObject(t));
    return events;
}

string sub share_own() {
    SharedCache own();
    string rv = try_change(sub () { SandboxManager::shareObject(own); });
    return sprintf(\"%s, can share: %y, shared: %y\", rv, SandboxManager::canShareObjects(),
        SandboxManager::isSharedObject(own));
}

list<string> sub who(SharedCache t) {
    return (
        \"a direct call: \" + t.who(),
        \"a trusted helper: \" + TrustedHelper::who(t),
        \"Reflection: \" + (string sub () {
            try {
                return cast<Reflection::NormalMethod>(Reflection::Class::getClass(t).getMethod(\"who\")).call(t);
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
        })(),
        \"call_object_method(): \" + call_object_method(t, \"who\"),
        \"a tenant callback that trusted code runs: \" + t.runCallback(string sub () { return t.who(); }),
    );
}

auto sub cache_get(ProgramCacheOwner o, string key) {
    return o.get(key);
}

string sub cache_set(ProgramCacheOwner o, string key, auto value, int max = 64) {
    return try_change(sub () { o.set(key, value, max); });
}

list<string> sub cache_direct(ProgramCacheOwner o) {
    return (
        try_change(sub () { SandboxManager::getProgramCacheValue(o, \"k\"); }),
        try_change(sub () { SandboxManager::setProgramCacheValue(o, \"k\", \"tenant\"); }),
        try_change(sub () {
            Reflection::Class c = Reflection::Class::forName(\"SandboxManager\");
            c.getStaticMethod(\"getProgramCacheValue\").call(o, \"k\");
        }),
        try_change(sub () {
            TenantSubclass ts();
            ts.readOther(o, \"k\");
        }),
    );
}

int sub cache_threads(ProgramCacheOwner o) {
    Counter c();
    int errors = 0;
    Mutex m();
    for (int t = 0; t < 8; ++t) {
        c.inc();
        background sub () {
            on_exit c.dec();
            for (int i = 0; i < 200; ++i) {
                string key = sprintf(\"k%d\", i % 16);
                o.set(key, key, 16);
                auto v = o.get(key);
                if (exists v && v != key) {
                    m.lock();
                    ++errors;
                    m.unlock();
                }
            }
        }();
    }
    c.waitForZero();
    return errors;
}
";

Program sub tenant() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.setSandboxManager(new SandboxManager());
    p.parse(TenantCode, "tenant");
    return p;
}

SharedCache sub shared_cache() {
    SharedCache t();
    SandboxManager::shareObject(t);
    return t;
}

Program p = tenant();

# code that sandboxed code calls in an update scope
{
    SharedCache t = shared_cache();
    printf("a tenant callback in a scope calls a method that changes the object: %s, %s\n",
        p.callFunction("callback_writes", t), t.snapshot());
    t = shared_cache();
    printf("a tenant callback in a scope calls a method that opens a scope: %s, %s\n",
        p.callFunction("callback_opens_scope", t), t.snapshot());
    t = shared_cache();
    printf("a tenant callback in a scope shares an object: %s\n", p.callFunction("callback_shares", t));
    t = shared_cache();
    printf("the scope changes the object after a tenant callback returns: %s, %s\n",
        p.callFunction("callback_returns", t), t.snapshot());
    t = shared_cache();
    printf("a tenant destructor in a scope: %y, %s\n", p.callFunction("destructor_in_scope", t), t.snapshot());
}

# sharing objects
printf("a tenant shares its own object: %s\n", p.callFunction("share_own"));
printf("trusted code: can share: %y\n", SandboxManager::canShareObjects());

# the sandbox of the code that called a method
{
    SharedCache t = shared_cache();
    map printf("the caller of a method, from the tenant: %s\n", $1), p.callFunction("who", t);
    printf("the caller of a method, from trusted code: %s\n", t.who());
}

# the per-Program cache
{
    Program p2 = tenant();
    ProgramCacheOwner o();
    ProgramCacheOwner other();
    printf("a tenant sets a value: %s\n", p.callFunction("cache_set", o, "k", "tenant 1"));
    printf("the tenant reads it: %y; another tenant: %y; trusted code: %y; another owner: %y\n",
        p.callFunction("cache_get", o, "k"), p2.callFunction("cache_get", o, "k"), o.get("k"),
        p.callFunction("cache_get", other, "k"));
    o.set("k", "trusted");
    printf("trusted code sets a value: trusted code reads %y, the tenant reads %y\n", o.get("k"),
        p.callFunction("cache_get", o, "k"));
    printf("the tenant reads or sets the values directly: %y\n", p.callFunction("cache_direct", o));
    {
        # trusted code of another Program, as of another library module
        Program other_pgm(PO_NEW_STYLE);
        other_pgm.parse("%modern
string sub read(object o) {
    try {
        SandboxManager::getProgramCacheValue(o, \"k\");
        return \"read\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}", "other");
        printf("trusted code of another Program reads them directly: %s\n", other_pgm.callFunction("read", o));
    }
    printf("trusted code of the Program of the class reads them directly: %s\n", (string sub () {
        try {
            SandboxManager::getProgramCacheValue(o, "k");
            return "read";
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    })());

    # the bound
    for (int i = 0; i < 5; ++i) {
        p.callFunction("cache_set", o, sprintf("b%d", i), i, 3);
    }
    printf("5 values with a bound of 3: %y\n", map p.callFunction("cache_get", o, sprintf("b%d", $1)), xrange(5));
    printf("a bound of 0: %s\n", p.callFunction("cache_set", o, "z", 1, 0));
    for (int i = 0; i < 1030; ++i) {
        p.callFunction("cache_set", other, sprintf("c%d", i), i, 5000);
    }
    printf("1030 values with a bound of 5000: first %y, 6th %y, 7th %y, last %y\n",
        p.callFunction("cache_get", other, "c0"), p.callFunction("cache_get", other, "c5"),
        p.callFunction("cache_get", other, "c6"), p.callFunction("cache_get", other, "c1029"));

    # replacing and releasing values
    Queue q();
    p.callFunction("cache_set", o, "t", new Tracker(q, "replaced"));
    p.callFunction("cache_set", o, "t", "new value");
    printf("a replaced value is released: %y\n", q.get(0));
    ProgramCacheOwner gone();
    p.callFunction("cache_set", gone, "t", new Tracker(q, "of a deleted owner"));
    delete gone;
    printf("a value of a deleted owner before the next change: %y\n", q.size());
    p.callFunction("cache_set", o, "u", 1);
    printf("after the next change: %y\n", q.get(0));

    # concurrent use
    printf("8 threads set and read values: errors: %d\n", p.callFunction("cache_threads", o));

    # values released with the Program, with the exception of a value
    p2.callFunction("cache_set", o, "t1", new Tracker(q, "released with the Program"));
    p2.callFunction("cache_set", o, "t2", new Tracker(q, "released with an error", True));
    string err;
    try {
        delete p2;
    } catch (hash<ExceptionInfo> ex) {
        err = ex.err;
    }
    list<string> released = ();
    while (q.size()) {
        push released, q.get(0);
    }
    printf("deleting the Program releases %y, exception: %y\n", sort(released), err);
}
