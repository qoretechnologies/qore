# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The default values of a function, method or closure are evaluated in the context of the code that declared them,
# never in the class context of the caller (qore#7066); see default-arg-context.qtest
#
# The methods of Vault (DefaultArgVault.qm) call code that other code gives them.  The default values of that code
# are code of its own: those of code of a sandboxed Program and of functions of this script and of the module cannot
# read or write the private and private:internal members of a Vault or call its private methods, although the code
# of Vault that calls them can.  The default values of the methods of Vault and of the closures created in them use
# the private declarations of Vault whoever calls them.

%modern
%no-child-restrictions

%requires DefaultArgVault
%requires reflection

#! holds the object whose private declarations the default values of the functions below use
class ScriptHolder {
    public {
        static *object v;
    }
}

#! a function of the script whose default value uses a private member of a Vault
auto sub script_peek(auto a = ScriptHolder::v.secret) {
    return a;
}

#! a function of the script whose default value uses a private:internal member of a Vault
auto sub script_ipeek(auto a = ScriptHolder::v.isecret) {
    return a;
}

#! a function of the script whose default value writes a private member of a Vault
auto sub script_write(auto a = (ScriptHolder::v.secret = "changed")) {
    return a;
}

#! a function of the script whose default value calls a private method of a Vault
auto sub script_call(auto a = ScriptHolder::v.helper()) {
    return a;
}

#! a class of the script whose constructor has a default value that uses a private member of a Vault
class ScriptCtor {
    public {
        auto a;
    }

    constructor(auto a = ScriptHolder::v.secret) {
        self.a = a;
    }
}

#! a class of the script with a private member, whose method creates an object of ScriptKeeperCtor
class ScriptKeeper {
    public {
        static *object k;
    }

    private {
        string secret = "ksecret";
    }

    auto make() {
        return (new ScriptKeeperCtor()).a;
    }
}

#! a class of the script whose constructor has a default value that uses a private member of a ScriptKeeper
class ScriptKeeperCtor {
    public {
        auto a;
    }

    constructor(auto a = ScriptKeeper::k.secret) {
        self.a = a;
    }
}

#! another class of the script, which calls the methods of a Vault
class Other {
    private {
        string secret = "other";
    }

    auto callIt(code c) {
        return c();
    }

    list<auto> own(Vault v) {
        return v.own();
    }

    list<auto> sown() {
        return Vault::sown();
    }
}

#! the operations of the default values of the sandboxed code on the private declarations of a Vault
const Ops = {
    "read": "Holder::v.secret",
    "iread": "Holder::v.isecret",
    "write": "(Holder::v.secret = \"changed\")",
    "call": "Holder::v.helper()",
    "icall": "Holder::v.ihelper()",
};

#! the ways that a Vault calls the code of the sandboxed Program, with the call of each of them
const Kinds = {
    "method": "v.notify(new L_%s())",
    "method by name": "v.callMethod(new L_%s())",
    "function reference": "v.callIt(\\f_%s())",
    "call_function_args": "v.callArgs(\\f_%s())",
    "closure": "v.callIt(cl_%s())",
    "closure with captures": "v.callIt(clc_%s())",
    "closure of a method": "v.callIt((new C_%s()).mk())",
    "static method reference": "v.callIt(\\W_%s::sm())",
    "method reference": "v.callIt(\\(new W_%s()).m())",
    "static caller": "v.callStatic(\\f_%s())",
    "background": "v.callBackground(\\f_%s())",
    "sort callback": "(v.callSort(cls_%s()), Holder::last)[1]",
};

#! returns the source of the sandboxed Program
string sub tenant_source() {
    string src = "%modern\nclass Holder { public { static auto v; static auto last; } }\n"
        "auto sub f_path(auto a = get_script_path()) { return a; }\n";
    foreach hash<auto> op in (Ops.pairIterator()) {
        string n = op.key;
        string e = op.value;
        src += sprintf("class L_%s { auto onEvent(auto a = %s) { return a; } }\n", n, e);
        src += sprintf("auto sub f_%s(auto a = %s) { return a; }\n", n, e);
        src += sprintf("class W_%s { static auto sm(auto a = %s) { return a; } auto m(auto a = %s) { return a; } }\n",
            n, e, e);
        src += sprintf("class C_%s { code mk() { return auto sub (auto a = %s) { return a; }; } }\n", n, e);
        src += sprintf("code sub cl_%s() { return auto sub (auto a = %s) { return a; }; }\n", n, e);
        src += sprintf("code sub clc_%s() { int z = 1; return auto sub (auto a = %s) { return (a, z); }; }\n", n, e);
        src += sprintf("code sub cls_%s() { return auto sub (auto x, auto y, auto a = %s) { Holder::last = a; "
            "return x <=> y; }; }\n", n, e);
        src += sprintf("auto sub run_%s(object v, string kind) {\n    Holder::v = v;\n    switch (kind) {\n", n);
        foreach hash<auto> kind in (Kinds.pairIterator()) {
            src += sprintf("        case %y: return %s;\n", kind.key, sprintf(kind.value, n));
        }
        src += "    }\n}\n";
    }
    src += "auto sub run_path(object v) { return v.callIt(\\f_path()); }\n";
    return src;
}

#! prints the result of a call
sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

Vault v();

# the code of a sandboxed Program called by a Vault
Program p(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION);
p.setScriptPath("tenant-program");
p.setSandboxManager(new SandboxManager());
p.parse(tenant_source(), "tenant");
foreach string op in (keys Ops) {
    foreach string kind in (keys Kinds) {
        show(sprintf("sandbox %s %s", kind, op), auto sub () { return p.callFunction("run_" + op, v, kind); });
    }
}
# the default value of a function of the sandboxed Program runs in its Program
show("sandbox program", auto sub () { return p.callFunction("run_path", v); });

# the functions of the script and of the module called by a Vault
ScriptHolder::v = v;
VaultHolder::v = v;
show("script function reference read", auto sub () { return v.callIt(\script_peek()); });
show("script function reference iread", auto sub () { return v.callIt(\script_ipeek()); });
show("script function reference write", auto sub () { return v.callIt(\script_write()); });
show("script function reference call", auto sub () { return v.callIt(\script_call()); });
show("script closure read", auto sub () { return v.callIt(auto sub (auto a = ScriptHolder::v.secret) { return a; }); });
show("script closure call", auto sub () { return v.callIt(auto sub (auto a = ScriptHolder::v.helper()) { return a; }); });
show("script constructor read", auto sub () { return v.construct(Class::forName("ScriptCtor")).a; });
ScriptKeeper::k = new ScriptKeeper();
show("script constructor from a class read", auto sub () { return ScriptKeeper::k.make(); });
show("module function direct read", auto sub () { return v.peekDirect(); });
show("module function reference read", auto sub () { return v.callIt(\vault_peek()); });
show("module function reference iread", auto sub () { return v.callIt(\vault_ipeek()); });
show("module function reference call", auto sub () { return v.callIt(\vault_call()); });
show("module function by reflection read", auto sub () { return Function::forName("vault_peek").call(); });

# the default values of the methods of Vault and of its closures use its private declarations whoever calls them
show("own top-level", auto sub () { return v.own(); });
show("own other class", auto sub () { return (new Other()).own(v); });
show("own subclass", auto sub () { return (new SubVault()).fromSub(); });
show("own inherited top-level", auto sub () { return (new SubVault()).own(); });
show("own method reference", auto sub () { code c = \v.own(); return c(); });
show("own method reference other class", auto sub () { return (new Other()).callIt(\v.own()); });
show("own by name", auto sub () { return call_object_method(v, "own"); });
show("own by reflection", auto sub () { return cast<NormalMethod>(Class::forName("Vault").getMethod("own")).call(v); });
show("own constructor top-level", auto sub () { return (new VaultCtor()).got; });
show("own constructor other class", auto sub () {
    return (new Other()).callIt(auto sub () { return (new VaultCtor()).got; });
});
show("own constructor subclass", auto sub () { return (new SubVaultCtor()).got; });
show("own constructor by reflection", auto sub () { return Class::forName("VaultCtor").newObject().got; });
show("own static top-level",auto sub () { return Vault::sown(); });
show("own static other class", auto sub () { return (new Other()).sown(); });
show("own static reference", auto sub () { code c = \Vault::sown(); return c(); });
show("own static by reflection", auto sub () { return Class::forName("Vault").getStaticMethod("sown").call(); });
show("own closure top-level", auto sub () { return v.closure()(); });
show("own closure other class", auto sub () { return (new Other()).callIt(v.closure()); });
show("own static closure top-level", auto sub () { return Vault::sclosure()(); });
show("own static closure other class", auto sub () { return (new Other()).callIt(Vault::sclosure()); });

# no default value changed the private members
show("state", auto sub () { return v.state(); });
