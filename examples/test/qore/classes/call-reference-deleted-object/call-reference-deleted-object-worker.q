#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls through method call references on objects deleted before the call or by its arguments (issue #6083); prints
# the error of each call

%modern

class U {
    m(auto a) {
        printf("U::m(%y)\n", a);
    }
}

class V inherits U {
}

class DelMutex inherits Mutex {
    int kill() {
        delete self;
        return 1;
    }

    callRefSelf() {
        code c = \lock();
        c(kill());
    }

    callRefSelfUser() {
        code c = \n();
        c(kill());
    }

    n(*int a) {
        printf("DelMutex::n(%y)\n", a);
    }
}

int sub delete_object(object o) {
    delete o;
    return 1;
}

sub check(string name, code c) {
    try {
        c();
        printf("%s: no exception\n", name);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
        # one exception for a call on a deleted object, with nothing chained
        while (ex.next) {
            ex = ex.next;
            printf("  next %s: %s\n", ex.err, ex.desc);
        }
    }
}

check("resolved, deleted before the call", sub () { U o(); code c = \o.m(); delete o; c(1); });
check("resolved, deleted by the arguments", sub () { U o(); code c = \o.m(); c(delete_object(o)); });
check("resolved, subclass, deleted by the arguments", sub () { V o(); code c = \o.m(); c(delete_object(o)); });
check("by name, deleted before the call", sub () { object o = new V(); code c = \o.m(); delete o; c(1); });
check("by name, deleted by the arguments", sub () { object o = new V(); code c = \o.m(); c(delete_object(o)); });
check("builtin, deleted before the call", sub () { Mutex o(); code c = \o.lock(); delete o; c(); });
check("builtin, deleted by the arguments", sub () { Mutex o(); code c = \o.lock(); c(delete_object(o)); });
check("self call reference, builtin", sub () { DelMutex o(); o.callRefSelf(); });
check("self call reference, user method", sub () { DelMutex o(); o.callRefSelfUser(); });
check("named arguments, deleted by the arguments",
    sub () { U o(); code c = \o.m(); c(a: delete_object(o)); });
