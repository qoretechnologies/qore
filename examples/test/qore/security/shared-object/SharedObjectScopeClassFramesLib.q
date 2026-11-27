# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The trusted library of SharedObjectScopeClassFrames.q: a shared object whose class updates it in an update scope;
# included by the fixture and parsed into a tenant's Program by it (see shared-object-scope-class-frames.qtest)

#! a shared object whose own methods change it
class SharedCache {
    public {
        string v = "orig";
    }

    #! runs a method of the tenant's object in an update scope, and changes the object after it returns
    string update(object t, string method, *object helper) {
        return SandboxManager::updateSharedObject(self, string sub () {
            v = "scope";
            string rv;
            switch (method) {
                case "poke": rv = t.poke(self); break;
                case "relay": rv = t.relay(self); break;
                case "deep": rv = t.deep(self); break;
                case "library": rv = t.library(self, helper); break;
                case "share": rv = t.share(self, new Plain()); break;
                case "scope": rv = t.scope(self); break;
                default: throw "UNKNOWN-METHOD", method;
            }
            # the scope's own code changes the object again once the tenant's code has returned
            v = "scope again";
            return rv;
        });
    }

    #! the scope's own code calls the methods of the class that change the object and share an object
    string own(*object helper) {
        return SandboxManager::updateSharedObject(self, string sub () {
            string rv = set("own") + ", " + relay("own relay") + ", " + shareIt(new Plain());
            if (helper) {
                rv += ", " + helper.run(self);
            }
            return rv;
        });
    }

    #! the scope's own code changes the object after the tenant's code that it called raised an exception through
    #! the frames of the tenant and of the class
    string ownAfterException(object t) {
        return SandboxManager::updateSharedObject(self, string sub () {
            string err;
            try {
                t.boom(self);
            } catch (hash<ExceptionInfo> ex) {
                err = ex.err;
            }
            return err + ", " + set("own after the exception");
        });
    }

    #! raises an exception
    throwIt() {
        throw "CACHE-ERROR", "an error";
    }

    #! a method of the object's own class that changes it
    string set(string val) {
        try {
            v = val;
            return "changed";
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    #! changes the object with another method of its own class
    string relay(string val) {
        return set(val);
    }

    #! shares an object
    string shareIt(object o) {
        try {
            SandboxManager::shareObject(o);
            return "shared";
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    #! opens a nested update scope
    string nested() {
        try {
            return SandboxManager::updateSharedObject(self, string sub () { return set("nested"); });
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }
}

#! another class of the library, whose methods change the object
class CacheRelay {
    string run(SharedCache c) {
        return c.set("relay");
    }
}

#! an object that is shared in a scope
class Plain {
}

SharedCache sub new_cache() {
    SharedCache c();
    SandboxManager::shareObject(c);
    return c;
}

CacheRelay sub new_relay() {
    return new CacheRelay();
}
