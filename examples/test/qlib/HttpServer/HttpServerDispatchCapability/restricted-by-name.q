#!/usr/bin/env qore
# code that may not use uncontrolled APIs (PO_NO_UNCONTROLLED_APIS) and has loaded the HttpServerUtil module cannot
# reach the system-policy call that the module creates by name (see HttpServerDispatchCapability::initResolve()) by
# any route of its own: the call of SandboxManager::createSystemPolicyCall() by name, and of the module's
# private:internal method that makes it, are refused at run time, and their references at parse time; trusted code
# still makes the call by name (qore#6843); run by ../HttpServerDispatchCapability.qtest with "restricted" or "trusted"
# as its argument; prints one line per route: the code of the exception that refused it, or "not refused"

%modern
%no-child-restrictions

bool restricted = ARGV[0] == "restricted";
int opts = PO_NEW_STYLE | (restricted ? PO_NO_UNCONTROLLED_APIS : 0);

Program p(opts);
p.parse("%requires HttpServerUtil
%requires reflection

# the result of a route: the exception code that refused it, or what it returned; a call that it returned is called
string sub result(code route) {
    try {
        auto v = route();
        if (v.typeCode() == NT_CLOSURE || v.typeCode() == NT_CALLREF) {
            return sprintf('not refused: usable call returning %y', v());
        }
        return sprintf('not refused: %s', v.type());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<auto> sub run() {
    code c = string sub () { return 'system policy'; };
    return {
        'call_static_method createSystemPolicyCall': result(sub () {
            return call_static_method('SandboxManager', 'createSystemPolicyCall', c);
        }),
        'call_static_method Qore::SandboxManager::createSystemPolicyCall': result(sub () {
            return call_static_method('Qore::SandboxManager', 'createSystemPolicyCall', c);
        }),
        'call_static_method_args createSystemPolicyCall': result(sub () {
            return call_static_method_args('SandboxManager', 'createSystemPolicyCall', c);
        }),
        'get_call_reference createSystemPolicyCall': result(sub () {
            return call_function_args(get_call_reference('SandboxManager::createSystemPolicyCall'), c);
        }),
        'call_function createSystemPolicyCall': result(sub () {
            return call_function('SandboxManager::createSystemPolicyCall', c);
        }),
        'call_function_args createSystemPolicyCall': result(sub () {
            return call_function_args('SandboxManager::createSystemPolicyCall', c);
        }),
        'reflection createSystemPolicyCall': result(sub () {
            return Class::forName('SandboxManager').getStaticMethod('createSystemPolicyCall').call(c);
        }),
        'call_static_method initResolve': result(sub () {
            return call_static_method('HttpServerDispatchCapability', 'initResolve');
        }),
        'call_static_method ::initResolve': result(sub () {
            return call_static_method('::HttpServerDispatchCapability', 'initResolve');
        }),
        'call_static_method_args initResolve': result(sub () {
            return call_static_method_args('HttpServerDispatchCapability', 'initResolve');
        }),
        'get_call_reference initResolve': result(sub () {
            return call_function_args(get_call_reference('HttpServerDispatchCapability::initResolve'));
        }),
        'call_function_args initResolve': result(sub () {
            return call_function_args('HttpServerDispatchCapability::initResolve');
        }),
        'reflection initResolve': result(sub () {
            return Class::forName('HttpServerDispatchCapability').getStaticMethod('initResolve').call();
        }),
    };
}
", "routes");
foreach hash<auto> i in (p.callFunction("run").pairIterator()) {
    printf("%s: %s\n", i.key, i.value);
}

# the references in the code itself are refused when it is parsed
foreach hash<auto> i in ({
        "direct createSystemPolicyCall": "code sub f() { return SandboxManager::createSystemPolicyCall(sub () {}); }",
        "reference createSystemPolicyCall": "code sub f() { return \\SandboxManager::createSystemPolicyCall(); }",
        "reference initResolve": "code sub f() { return \\HttpServerDispatchCapability::initResolve(); }",
        "direct initResolve": "auto sub f() { return HttpServerDispatchCapability::initResolve(); }",
    }.pairIterator()) {
    Program pp(opts);
    try {
        pp.parse("%requires HttpServerUtil\n" + i.value, "parse");
        printf("%s: not refused at parse time\n", i.key);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", i.key, ex.err);
    }
}
