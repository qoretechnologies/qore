# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The update scopes of shared objects: sandboxed code, or code that acts for it, changes a shared object only in an
# update scope that a method of a class of the object opens with SandboxManager::updateSharedObject(), with a closure
# that the class created; a closure of other code, a callback of the caller, another object, a thread started in the
# scope and code after the scope cannot change it; SandboxManager::checkSharedObjectCallback() accepts only closures
# of trusted classes from sandboxed code (qore#6843); see shared-object-update-scope.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Logger
%requires reflection

public class SharedCache {
    public {
        string cached = "orig";
        int count = 1;
    }

    #! updates the cache from the object itself, in an update scope
    fill() {
        SandboxManager::updateSharedObject(self, sub () { cached = sprintf("filled %d", count); });
    }

    #! updates the cache without an update scope
    fillUnscoped() {
        cached = "unscoped";
    }

    #! opens an update scope with code that its caller gives
    auto forward(code update) {
        return SandboxManager::updateSharedObject(self, update);
    }

    #! returns a closure of this class that changes the object
    code getUpdate() {
        return sub () { cached = "handed out"; };
    }

    #! changes the object in an update scope that raises an exception
    fillThenThrow() {
        SandboxManager::updateSharedObject(self, sub () {
            cached = "before the exception";
            throw "UPDATE-ERROR", "the update raises an exception";
        });
    }

    #! changes the object in a thread that the update scope starts, and returns the result of the change
    string fillInThread() {
        return SandboxManager::updateSharedObject(self, string sub () {
            Counter done(1);
            string rv;
            background sub () {
                on_exit done.dec();
                try {
                    cached = "thread";
                    rv = "changed";
                } catch (hash<ExceptionInfo> ex) {
                    rv = ex.err;
                }
            }();
            done.waitForZero();
            return rv;
        });
    }

    #! changes another object in an update scope for this one
    fillOther(SharedCache other) {
        SandboxManager::updateSharedObject(self, sub () { other.cached = "other"; });
    }

    #! runs a callback of its caller in an update scope
    callBack(code callback) {
        SandboxManager::updateSharedObject(self, sub () { callback(); });
    }

    #! runs code that its caller gives while it holds the state of the object
    auto runLocked(code c) {
        SandboxManager::checkSharedObjectCallback(self, c, "run code locked");
        return c();
    }

    string snapshot() {
        return sprintf("cached: %y count: %y", cached, count);
    }
}

#! a trusted class that is not a class of the object
public class OtherClass {
    static tryUpdate(SharedCache t) {
        SandboxManager::updateSharedObject(t, sub () { t.cached = "other class"; });
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires Logger
%requires reflection

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, code> sub changes() {
    return {
        \"a method that updates in a scope\": sub (SharedCache t) { t.fill(); },
        \"a method that changes without a scope\": sub (SharedCache t) { t.fillUnscoped(); },
        \"a scope that the tenant opens with its own closure\": sub (SharedCache t) {
            SandboxManager::updateSharedObject(t, sub () { t.cached = \"tenant\"; });
        },
        \"a scope that the tenant opens with a closure of the class\": sub (SharedCache t) {
            SandboxManager::updateSharedObject(t, t.getUpdate());
        },
        \"a tenant closure that a trusted helper forwards\": sub (SharedCache t) {
            t.forward(sub () { t.cached = \"forwarded\"; });
        },
        \"a call reference of the tenant that a trusted helper forwards\": sub (SharedCache t) {
            t.forward(\\t.fillUnscoped());
        },
        \"a change after a scope that raised an exception\": sub (SharedCache t) {
            try {
                t.fillThenThrow();
            } catch (hash<ExceptionInfo> ex) {
            }
            t.fillUnscoped();
        },
        \"a callback of the tenant in a scope\": sub (SharedCache t) {
            t.callBack(sub () { t.cached = \"callback\"; });
        },
        \"a tenant closure run while the state is held\": sub (SharedCache t) {
            t.runLocked(sub () {});
        },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, SharedCache t) {
    return try_change(sub () { changes(){name}(t); });
}

string sub fill_in_thread(SharedCache t) {
    return t.fillInThread();
}

string sub fill_other(SharedCache t, SharedCache other) {
    return try_change(sub () { t.fillOther(other); });
}

string sub other_class(SharedCache t) {
    return try_change(sub () { OtherClass::tryUpdate(t); });
}

string sub run_locked_trusted(SharedCache t) {
    return try_change(sub () { t.runLocked(t.getUpdate()); });
}

# replaces the logger of a logger wrapper, directly and through Reflection
string sub set_logger(Logger::LoggerWrapper w) {
    list<string> l = (
        try_change(sub () { w.setLogger(new Logger::Logger(\"tenant\")); }),
        try_change(sub () {
            cast<Reflection::NormalMethod>(Reflection::Class::getClass(w).getMethod(\"setLogger\")).call(w,
                new Logger::Logger(\"tenant\"));
        }),
    );
    return join(\" \", l);
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

SharedCache sub shared_cache() {
    SharedCache t();
    SandboxManager::shareObject(t);
    return t;
}

foreach string name in (p.callFunction("change_names")) {
    SharedCache shared = shared_cache();
    string rv = p.callFunction("try_one", name, shared);
    SharedCache plain();
    string plain_rv = p.callFunction("try_one", name, plain);
    printf("%s: shared: %s, %s; not shared: %s\n", name, rv, shared.snapshot(), plain_rv);
}

SharedCache shared = shared_cache();
SharedCache other = shared_cache();
printf("a thread that a scope starts: %s, %s\n", p.callFunction("fill_in_thread", shared), shared.snapshot());
printf("another object in a scope: %s, %s\n", p.callFunction("fill_other", shared, other), other.snapshot());
printf("a scope of another trusted class: %s, %s\n", p.callFunction("other_class", shared), shared.snapshot());
printf("a closure of the class run while the state is held: %s, %s\n", p.callFunction("run_locked_trusted", shared),
    shared.snapshot());
# a process-wide object of a library class that is not a schema: a logger wrapper with the host's logger
{
    Logger host_logger("host");
    LoggerWrapper shared_wrapper(host_logger);
    SandboxManager::shareObject(shared_wrapper);
    LoggerWrapper plain_wrapper(host_logger);
    printf("a tenant replaces the logger of a shared logger wrapper: %s, host logger kept: %y\n",
        p.callFunction("set_logger", shared_wrapper), shared_wrapper.isLogger(host_logger));
    printf("a tenant replaces the logger of a logger wrapper that is not shared: %s, host logger kept: %y\n",
        p.callFunction("set_logger", plain_wrapper), plain_wrapper.isLogger(host_logger));
}
printf("trusted code updates an object that is not shared: %s\n", (string sub () {
    try {
        (new SharedCache()).fill();
        return "changed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
printf("trusted code updates a shared object: %s\n", (string sub () {
    SharedCache t = shared_cache();
    t.fill();
    t.fillUnscoped();
    t.runLocked(sub () { t.count = 2; });
    return t.snapshot();
})());
