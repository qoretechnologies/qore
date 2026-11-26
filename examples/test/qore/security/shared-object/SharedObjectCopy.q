# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Code that is sandboxed or acts for sandboxed code cannot copy an object that trusted code marked as shared with
# SandboxManager::shareObject() unless a class of the copy declares a copy() method in Qore code, which resets what the
# class guards; trusted code copies as before, and copies are not marked as shared (qore#7667); see
# shared-object-copy.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection

#! guards a private member with isSharedObject(self) and declares no copy() method
public class GuardedNoCopy {
    public {
        static int destroyed = 0;
    }

    private {
        string secret = "secret";
    }

    destructor() {
        ++destroyed;
    }

    #! the secret, which the class gives only for an object that is not shared
    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : secret;
    }

    #! copies the object in the code of its own class
    GuardedNoCopy copySelf() {
        return copy();
    }
}

#! guards a private member with isSharedObject(self) and resets it in its own copy() method
public class GuardedOwnCopy {
    private {
        *string secret = "secret";
    }

    copy() {
        remove secret;
    }

    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : (secret ?? "no secret");
    }
}

#! inherits the copy() method of its base class
public class GuardedInheritedCopy inherits GuardedOwnCopy {
}

#! inherits only the builtin copy method of Counter
public class GuardedCounter inherits Qore::Thread::Counter {
    private {
        string secret = "secret";
    }

    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : secret;
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# the result of a copy, or its exception
string sub try_copy(code c) {
    try {
        object o = c();
        return sprintf(\"copied %s, shared: %y, secret: %s\", o.className(), SandboxManager::isSharedObject(o),
            o.useSecret());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub copy_it(object o) {
    return try_copy(object sub () { return o.copy(); });
}

string sub copy_self(GuardedNoCopy o) {
    return try_copy(object sub () { return o.copySelf(); });
}

# trusted code that the tenant calls copies the object for it
string sub copy_deputy(code deputy, object o) {
    return try_copy(object sub () { return deputy(o); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

object sub shared(object o) {
    SandboxManager::shareObject(o);
    return o;
}

# trusted code that copies the object that it is given
code deputy = object sub (object o) { return o.copy(); };

# trusted code that copies the object that it is given with Reflection, with the copy method of the given class
code sub reflection_deputy(string cls) {
    return object sub (object o) { return Reflection::Class::forName(cls).getCopyMethod().call(o); };
}

{
    GuardedNoCopy o = shared(new GuardedNoCopy());
    int destroyed = GuardedNoCopy::destroyed;
    printf("tenant copies a shared object without copy(): %s\n", p.callFunction("copy_it", o));
    # a refused copy makes no object, so no destructor runs
    printf("no object was made: %y\n", GuardedNoCopy::destroyed == destroyed);
}
printf("tenant has the class copy it: %s\n", p.callFunction("copy_self", shared(new GuardedNoCopy())));
printf("trusted deputy copies it for the tenant: %s\n", p.callFunction("copy_deputy", deputy,
    shared(new GuardedNoCopy())));
printf("tenant copies one that is not shared: %s\n", p.callFunction("copy_it", new GuardedNoCopy()));
{
    GuardedNoCopy o = shared(new GuardedNoCopy());
    GuardedNoCopy c = o.copy();
    printf("trusted code copies it: shared: %y, secret: %s\n", SandboxManager::isSharedObject(c), c.useSecret());
}

printf("tenant copies a shared object with its own copy(): %s\n", p.callFunction("copy_it",
    shared(new GuardedOwnCopy())));
printf("trusted deputy copies it for the tenant: %s\n", p.callFunction("copy_deputy", deputy,
    shared(new GuardedOwnCopy())));
printf("tenant copies a shared object with an inherited copy(): %s\n", p.callFunction("copy_it",
    shared(new GuardedInheritedCopy())));
printf("tenant copies a shared object with a builtin copy only: %s\n", p.callFunction("copy_it",
    shared(new GuardedCounter())));
printf("trusted deputy copies it with Reflection for the tenant: %s\n", p.callFunction("copy_deputy",
    reflection_deputy("Counter"), shared(new GuardedCounter())));
printf("trusted deputy copies one with copy() with Reflection for the tenant: %s\n", p.callFunction("copy_deputy",
    reflection_deputy("GuardedOwnCopy"), shared(new GuardedOwnCopy())));
{
    GuardedCounter o = shared(new GuardedCounter());
    printf("trusted code copies it with Reflection: shared: %y\n",
        SandboxManager::isSharedObject(Reflection::Class::forName("Counter").getCopyMethod().call(o)));
}
{
    GuardedCounter o = shared(new GuardedCounter());
    GuardedCounter c = o.copy();
    printf("trusted code copies it: shared: %y, secret: %s\n", SandboxManager::isSharedObject(c), c.useSecret());
}
