#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a worker of foreign-program-code.qtest: closures and call references run in the Program that compiled their code,
# also when they are called from another Program, so the names that their code looks up at run time resolve there
# (qore#7202); prints one "<case>=ok" line per case, or "<case>=FAIL: <details>"

%modern
%no-child-restrictions

#! the names that the code looks up at run time: a function, a class, a call reference, a global variable of the
#! Program (which only the tenant's Program has) and a namespace path; spliced into the code of both Programs, so
#! that each closure looks them up itself
const Lookups = "{\"function\": call_function(\"f\"), \"object\": (create_object(\"O\")).n(),
    \"call-ref\": get_call_reference(\"f\")(), \"global\": get_global_vars().hasKey(\"global_A\"),
    \"namespace\": call_function(\"N::nf\") + \"/\" + call_static_method(\"N::C\", \"s\")}";

#! the declarations that the lookups find, with the name of the Program in their values
string sub declarations(string name) {
    return sprintf("string sub f() { return \"%s\"; }
our *string global_%s;
namespace N { string sub nf() { return \"%s-N\"; } class C { static string s() { return \"%s-NC\"; } } }
class O { string n() { return \"%s-O\"; } }
", name, name, name, name, name);
}

#! the code of the tenant, whose class runs in a Program of the host
const TenantCode = "%modern
DECLARATIONS
public class TenantClass {
    #! the lookups of a method (which runs in the tenant's Program)
    hash<auto> method() {
        return LOOKUPS;
    }

    #! a closure
    code closure() {
        return hash<auto> sub () { return LOOKUPS; };
    }

    #! a closure with an argument
    code closureArg() {
        return hash<auto> sub (int a) { return LOOKUPS + {\"a\": a}; };
    }

    #! a closure with a parameter that a call can name
    code closureNamed() {
        return hash<auto> sub (string tag = \"default\") { return LOOKUPS + {\"tag\": tag}; };
    }

    #! a closure that calls a closure that it creates
    code nested() {
        return hash<auto> sub () {
            code c = hash<auto> sub () { return LOOKUPS; };
            return c();
        };
    }

    #! a closure that returns a closure that it creates, which its caller calls
    code nestedReturned() {
        return code sub () {
            return hash<auto> sub () { return LOOKUPS; };
        };
    }

    #! a closure that looks the names up in a thread that it starts
    code inBackground() {
        return hash<auto> sub () {
            Queue q();
            background q.push(LOOKUPS);
            return q.get();
        };
    }

    #! a closure that gives the lookups to a queue, for callbacks
    code pushTo(Queue q) {
        return sub () { q.push(LOOKUPS); };
    }

    #! a closure that calls the code that it is given (host code)
    code callsHost(code c) {
        return hash<auto> sub () { return c(); };
    }

    #! a call reference to a method of the object
    code methodRef() {
        return \\self.method();
    }

    #! a closure of a static method, which has no object
    static code staticClosure() {
        return hash<auto> sub () { return LOOKUPS; };
    }
}

#! a closure of the tenant's own code, which the tenant calls itself
hash<auto> sub own_closure() {
    code c = hash<auto> sub () { return LOOKUPS; };
    return c();
}

sub export_to(Program target) {
    target.importClass(\"TenantClass\");
}
";

#! the code of the host's Program, which the tenant's class is given to
const HostCode = "DECLARATIONS
#! a class of the host that inherits the tenant's class
class HostSub inherits TenantClass {
    #! a closure of a method of the host
    code hostClosure() {
        return hash<auto> sub () { return LOOKUPS; };
    }
}

object sub make() { return new TenantClass(); }
object sub make_sub() { return new HostSub(); }

#! a closure of the host's code
code sub host_closure() {
    return hash<auto> sub () { return LOOKUPS; };
}

#! a call reference to a method of the tenant's class that the host's code creates
code sub host_method_ref(object o) {
    return \\o.method();
}

#! a closure of a static method of the tenant's class
code sub host_static_closure() {
    return TenantClass::staticClosure();
}

#! calls code in the host's Program
auto sub call_in_host(code c) {
    return c();
}
";

#! what the lookups find in the tenant's Program
const InTenant = {
    "function": "A", "object": "A-O", "call-ref": "A", "global": True, "namespace": "A-N/A-NC",
};

#! what the lookups find in the host's Program
const InHost = {
    "function": "B", "object": "B-O", "call-ref": "B", "global": False, "namespace": "B-N/B-NC",
};

int errors = 0;

sub report(string name, auto expected, auto actual) {
    if (expected == actual) {
        printf("%s=ok\n", name);
    } else {
        printf("%s=FAIL: expected %y got %y\n", name, expected, actual);
        ++errors;
    }
}

#! returns the lookups that the code gives, or the error that it raises
auto sub outcome(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc;
    }
}

#! calls the code given: one call site, which the native code of the IR and JIT modes makes directly once the code
#! has been called often
auto sub call_closure(code c) {
    return c();
}

#! calls the code given with one argument at one call site
auto sub call_closure_arg(code c, int a) {
    return c(a);
}

string sub with_lookups(string src, string name) {
    return replace(replace(src, "DECLARATIONS", declarations(name)), "LOOKUPS", Lookups);
}

Program tenant(PO_NEW_STYLE);
tenant.parse(with_lookups(TenantCode, "A"), "tenant");
Program host_pgm(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.callFunction("export_to", host_pgm);
host_pgm.parse(with_lookups(HostCode, "B"), "host-program");
object o = host_pgm.callFunction("make");

# the tenant's methods run in the tenant's Program, and so do its closures and call references
report("byname-method", InTenant, o.method());
report("byname-closure", InTenant, outcome(o.closure()));
report("byname-closure-in-host", InTenant, host_pgm.callFunction("call_in_host", o.closure()));
report("byname-closure-arg", InTenant + {"a": 1}, outcome(auto sub () { return o.closureArg()(1); }));
report("byname-closure-named", InTenant + {"tag": "named"},
    outcome(auto sub () { return o.closureNamed()(tag: "named"); }));
report("byname-nested", InTenant, outcome(o.nested()));
report("byname-nested-returned", InTenant, outcome(o.nestedReturned()()));
report("byname-static-closure", InTenant, outcome(host_pgm.callFunction("host_static_closure")));
report("byname-method-ref", InTenant, outcome(o.methodRef()));
report("byname-method-ref-host", InTenant, outcome(host_pgm.callFunction("host_method_ref", o)));

# also in threads and callbacks that run the code
report("byname-background", InTenant, outcome(o.inBackground()));
{
    Queue q();
    background o.pushTo(q)();
    report("byname-background-callback", InTenant, q.get());
    ThreadPool tp(1);
    tp.submit(o.pushTo(q));
    report("byname-thread-pool", InTenant, q.get());
    tp.stopWait();
    AsyncIoController::addTimerDelay(1ms, o.pushTo(q));
    report("byname-timer", InTenant, q.get());
}

# also once the native code of the IR and JIT modes calls the code directly
{
    code c = o.closure();
    code c1 = o.closureArg();
    auto r;
    auto r1;
    for (int i = 0; i < 3000; ++i) {
        r = call_closure(c);
        r1 = call_closure_arg(c1, i);
    }
    report("byname-hot", InTenant, r);
    report("byname-hot-arg", InTenant + {"a": 2999}, r1);
}

# host code that the tenant's closure calls runs in the host's Program
report("byname-calls-host", InHost, outcome(o.callsHost(host_pgm.callFunction("host_closure"))));

# the closures of the code of the Program that they run in are not affected
report("byname-own-host", InHost, outcome(host_pgm.callFunction("host_closure")));
report("byname-own-tenant", InTenant, tenant.callFunction("own_closure"));
{
    object s = host_pgm.callFunction("make_sub");
    # a closure of a method of the host's subclass runs in the host's Program, one of the inherited method of the
    # tenant's class in the tenant's
    report("byname-subclass-host", InHost, outcome(s.hostClosure()));
    report("byname-subclass-tenant", InTenant, outcome(s.closure()));
}

# once the tenant's Program is deleted, its closures no longer run
{
    code c = o.closure();
    delete tenant;
    auto r = outcome(c);
    report("byname-deleted", "PROGRAM-ERROR", r.typeCode() == NT_STRING ? (r =~ x/^([A-Z-]+):/)[0] : r);
}

# a run without failures ends normally, so that its leak report under valgrind is meaningful
if (errors) {
    exit(1);
}
