#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of inherited-implementation-domain.qtest (qore#7543): a builtin method of a class in a functional domain
# that implements, in a Qore class that inherits both, an abstract method of a parent class that has no functional
# domain, is refused to code restricted from the domain as any other builtin method of the class, whatever the form
# of the call; prints one "<case>=ok" line per check, or "<case>=<the differences>"

%modern
%enable-all-warnings
%no-child-restrictions
%requires FsUtil
%requires HttpClientIo

#! a class without a functional domain that declares a method that a builtin class of the FILESYSTEM domain has
class Lines {
    abstract *string readLine(bool include_eol = True, *string eol);
}

#! the abstract method is implemented by the builtin method of ReadOnlyFile, which is in the FILESYSTEM domain
class ScriptFile inherits Lines, ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }
}

#! a class without a functional domain that declares a method that a builtin class without a domain has
class Reader {
    abstract *binary readBinary(int limit = -1);
}

#! the abstract method is implemented by the builtin method of StreamReader, which has no functional domain
class ScriptStream inherits Reader, StreamReader {
    constructor() : StreamReader(new BinaryInputStream(<6162>)) {
    }
}

#! the abstract method is implemented by a method in Qore
class ScriptLines inherits Lines {
    *string readLine(bool include_eol = True, *string eol) {
        return "line";
    }
}

#! a class that declares an overload of its own of a method that it inherits from a builtin class in the FILESYSTEM
#! domain: a call of the inherited builtin overload resolves through the method of this class
class OwnOverload inherits ReadOnlyFile {
    constructor(string path) : ReadOnlyFile(path) {
    }

    *string readLine(int n) {
        return "own";
    }
}

#! the methods called: their arguments, and the abstract parent class that declares them
const Methods = {
    "getState": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "getGoal": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "goalReached": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "continuePoll": {"type": "AbstractPollOperation", "args": "", "values": ()},
    "readLine": {"type": "Lines", "args": "False", "values": (False,)},
    "readBinary": {"type": "Reader", "args": "1", "values": (1,)},
    # the overload of OwnOverload, a class in the FILESYSTEM domain, which code restricted from it cannot name
    "readLineOwn": {"name": "readLine", "type": "object", "args": "1", "values": (1,)},
};

#! the forms of a call: on an object reference, through a variable of the abstract parent class, with
#! call_object_method_args(), through a call reference, and through a call reference that the host calls
const Forms = ("object", "typed", "dynamic", "call-ref", "host-call-ref");

const Refused = "INVALID-FUNCTION-ACCESS";

list<string> results = ();

#! the refusals of a call: by the parse options of the calling code, and by the sandbox that governs it
const ParseRefused = "parse-refused";
const SandboxRefused = "sandbox-refused";

#! the result of \a code's go() with \a args in a Program with \a po, governed by \a sm if given: "called",
#! ParseRefused, SandboxRefused or the error
string sub run_owned(string code, int po, *SandboxManager sm, *list<auto> args) {
    try {
        Program p(PO_MODERN | po);
        if (sm) {
            p.setSandboxManager(sm);
        }
        p.parse(code, "owned-call");
        return p.callFunctionArgs("go", args) === True ? "called" : "unexpected result";
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == Refused) {
            if (ex.desc =~ /^parse options do not allow access to builtin \w+ '(\S*::)?(fs|mixed)?[tT]ouch\(\)'/) {
                return ParseRefused;
            }
            if (ex.desc =~ /^the sandbox that governs the calling code does not allow access to uncontrolled API/) {
                return SandboxRefused;
            }
        }
        return ex.err + ": " + ex.desc;
    }
}

#! the forms of the call of a method of an object of a class owned by a Program: on the object, with
#! call_object_method(), through a call reference that the calling code creates and through one that the owner created
const OwnedForms = {
    "object": "auto sub go(object o, code c) { return o.%s(); }",
    "dynamic": "auto sub go(object o, code c) { return call_object_method(o, \"%s\"); }",
    "call-ref": "auto sub go(object o, code c) { code r = \\o.%s(); return r(); }",
    "owner-call-ref": "auto sub go(object o, code c) { return c(); }",
};

#! checks that each form of the call of method \a m on \a obj (\a ref: the owner's call reference to it) by code in a
#! Program with \a po, governed by the sandbox that \a get_sm returns if given, gives \a expected
sub check_owned(string name, string m, object obj, code ref, int po, *code get_sm, string expected) {
    list<string> diffs = ();
    foreach hash<auto> i in (OwnedForms.pairIterator()) {
        string rv = run_owned(sprintf(i.value, m), po, get_sm ? get_sm() : NOTHING, (obj, ref));
        if (rv != expected) {
            push diffs, sprintf("%s: %s", i.key, rv);
        }
    }
    push results, name + "=" + (diffs ? diffs.join("; ") : "ok");
}

#! the code of the Program that makes the calls
string sub make_code() {
    string code;
    foreach hash<auto> i in (Methods.pairIterator()) {
        string f = i.key;
        string m = i.value.name ?? f;
        string a = i.value.args;
        code += sprintf("auto sub object_%s(object o) { return o.%s(%s); }\n", f, m, a);
        code += sprintf("auto sub typed_%s(%s o) { return o.%s(%s); }\n", f, i.value.type, m, a);
        code += sprintf("auto sub dynamic_%s(object o) { return call_object_method_args(o, \"%s\", (%s)); }\n", f,
            m, a);
        code += sprintf("auto sub call_ref_%s(object o) { code c = \\o.%s(); return c(%s); }\n", f, m, a);
        code += sprintf("code sub make_ref_%s(object o) { return \\o.%s(); }\n", f, m);
    }
    return code;
}

#! the result of a call of \a m on \a obj in the form \a form in \a p: the error, or "called"
string sub call(Program p, string form, string m, object obj) {
    try {
        if (form == "host-call-ref") {
            code c = p.callFunction("make_ref_" + m, obj);
            call_function_args(c, Methods{m}.values);
        } else {
            p.callFunction(replace(form, "-", "_") + "_" + m, obj);
        }
        return "called";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! checks that each of \a forms of a call of each of \a methods on \a obj in a Program with \a po gives \a expected
sub check(string name, int po, object obj, list<string> methods, string expected, list<string> forms = Forms) {
    Program p(po);
    p.importClass("Lines");
    p.importClass("Reader");
    p.parse(make_code(), "calls");
    list<string> diffs = ();
    foreach string m in (methods) {
        foreach string form in (forms) {
            string rv = call(p, form, m, obj);
            if (rv != expected) {
                push diffs, sprintf("%s %s(): %s", form, m, rv);
            }
        }
    }
    push results, name + "=" + (diffs ? diffs.join("; ") : "ok");
}

# a listener that the poll operation targets: no connection may be made to it by the restricted code
Socket listener();
listener.bind("127.0.0.1:0");
listener.listen();
int port = listener.getSocketInfo().port;

TmpDir tmp("qore-inherited-implementation-domain-");
string path = tmp.path + DirSep + "lines.txt";
{
    File f();
    f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
    f.write("line\n");
}

list<string> poll_methods = ("getState", "getGoal", "goalReached", "continuePoll");

{
    HttpClientIo::Http1ClientConnection conn(parse_url("http://127.0.0.1:" + port));
    # the methods of the poll operation of the connection that the Qore class HttpClientIo::Http1ClientPollOperation
    # inherits from Qore::Http1ClientPollOperationBase, which implement the abstract methods of
    # Qore::AbstractPollOperation that it also inherits from HttpClientIo::HttpClientPollOperation
    check("network-poll-operation", PO_MODERN | PO_NO_NETWORK, conn.getPollOperation(), poll_methods, Refused);
    # no connection was made
    push results, "network-no-connection=" + (listener.accept(0) ? "connected" : "ok");
    # code that may use the network can call them
    check("network-poll-operation-allowed", PO_MODERN, conn.getPollOperation(), ("getState", "getGoal",
        "goalReached"), "called");
    # a method of the module that calls a builtin method of the poll operation for restricted code still does
    {
        Program p(PO_MODERN | PO_NO_NETWORK);
        p.parse("auto sub f(object c) { return c.getActiveStreamCount(); }", "module-call");
        string rv;
        try {
            rv = sprintf("%y", p.callFunction("f", conn));
        } catch (hash<ExceptionInfo> ex) {
            rv = ex.err;
        }
        push results, "network-module-method=" + (rv == "0" ? "ok" : rv);
    }
}

# the same with a class of this Program and a builtin class in the FILESYSTEM domain
check("filesystem-file", PO_MODERN | PO_NO_FILESYSTEM, new ScriptFile(path), ("readLine",), Refused);
check("filesystem-file-allowed", PO_MODERN, new ScriptFile(path), ("readLine",), "called");
# the builtin overload of a method of which the class declares an overload of its own; the class's own overload stays
# callable
list<string> untyped_forms = select Forms, $1 != "typed";
check("filesystem-own-overload", PO_MODERN | PO_NO_FILESYSTEM, new OwnOverload(path), ("readLine",), Refused,
    untyped_forms);
check("filesystem-own-overload-own", PO_MODERN | PO_NO_FILESYSTEM, new OwnOverload(path), ("readLineOwn",),
    "called");

# an implementation without a functional domain stays callable by restricted code
check("no-domain-builtin", PO_MODERN | PO_NO_FILESYSTEM | PO_NO_NETWORK, new ScriptStream(), ("readBinary",),
    "called");
check("no-domain-qore", PO_MODERN | PO_NO_FILESYSTEM | PO_NO_NETWORK, new ScriptLines(), ("readLine",), "called");

# an object of a builtin class that no Program owns, called directly, is refused as before qore#7543
check("static-builtin-object", PO_MODERN | PO_NO_FILESYSTEM, new ReadOnlyFile(path), ("readLine",), Refused,
    untyped_forms);

# a builtin class owned by a Program (created for it at run time, as the classes of language bridge modules are) runs
# that Program's foreign code: the uncontrolled API of an object of the class that the Program gives to code
# restricted from it is a capability of that Program, as an object of one of its Qore classes is (qore#5521), unless a
# sandbox that refuses uncontrolled APIs governs that code; the other domains of its methods are checked against the
# calling code, and code restricted from the domain cannot create such an object for a Program with more rights
{
    # writes a module that creates such a class for its own Program
    code write_maker = sub (string name) {
        File f();
        f.open2(tmp.path + DirSep + name + ".qm", O_CREAT | O_WRONLY | O_TRUNC);
        f.write(sprintf("%%new-style
%%requires restrictedbuiltinprobe
module %s {
    version = \"1.0\";
    desc = \"creates objects of a builtin class owned by the module's Program\";
    author = \"Qore Technologies, s.r.o.\";
    url = \"https://qore.org\";
}
public namespace %s {
    public object sub make() {
        return RestrictedBuiltinProbe::make();
    }
}
", name, name));
    };
    # loaded first by restricted code, which the module's Program inherits the restrictions of
    write_maker("RestrictedProbeMaker");
    # loaded first by trusted code (the owner): a module is a capability
    write_maker("TrustedProbeMaker");
    string restricted_maker = tmp.path + DirSep + "RestrictedProbeMaker.qm";
    string trusted_maker = tmp.path + DirSep + "TrustedProbeMaker.qm";

    # the Program that owns the class; it calls the code that restricted code gives it
    Program owner(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS);
    owner.parse(sprintf("%%requires restrictedbuiltinprobe
%%requires %s
object sub make() { return RestrictedBuiltinProbe::make(); }
auto sub call_it(code c) { return c(); }
auto sub call_touch(object o) { return o.touch(); }
code sub make_touch_ref(object o) { return \\o.touch(); }
code sub make_fs_touch_ref(object o) { return \\o.fsTouch(); }
code sub make_mixed_touch_ref(object o) { return \\o.mixedTouch(); }
# a class of this Program whose abstract method is implemented by the builtin method of a class that no Program owns
class Toucher {
    abstract bool touch();
}
class ScriptProbe inherits Toucher, RestrictedBuiltinProbe::StaticProbe {
}
object sub make_script_probe() { return new ScriptProbe(); }
object sub make_static_probe() { return new RestrictedBuiltinProbe::StaticProbe(); }
", trusted_maker), "owner");
    object probe = owner.callFunction("make");

    code deny = SandboxManager sub () { return new SandboxManager(); };
    # a sandbox that allows uncontrolled APIs
    code allow = SandboxManager sub () {
        SandboxManager sm();
        sm.setUncontrolledApiPolicy(True);
        return sm;
    };

    code touch_ref = owner.callFunction("make_touch_ref", probe);
    check_owned("owned-builtin-capability", "touch", probe, touch_ref, PO_NO_UNCONTROLLED_APIS, NOTHING, "called");
    # the sandbox's uncontrolled API policy governs the code that makes the call, whichever Program owns the class
    check_owned("owned-builtin-sandboxed", "touch", probe, touch_ref, 0, deny, SandboxRefused);
    check_owned("owned-builtin-sandboxed-restricted", "touch", probe, touch_ref, PO_NO_UNCONTROLLED_APIS, deny,
        SandboxRefused);
    check_owned("owned-builtin-sandbox-allows", "touch", probe, touch_ref, PO_NO_UNCONTROLLED_APIS, allow, "called");
    # another domain of a method of the class is checked against the calling code; the owner's call reference keeps
    # the owner's rights
    code fs_touch_ref = owner.callFunction("make_fs_touch_ref", probe);
    hash<string, string> fs_expected = {"object": ParseRefused, "dynamic": ParseRefused, "call-ref": ParseRefused,
        "owner-call-ref": "called"};
    {
        list<string> diffs = ();
        foreach hash<auto> i in (OwnedForms.pairIterator()) {
            string rv = run_owned(sprintf(i.value, "fsTouch"), PO_NO_FILESYSTEM, NOTHING, (probe, fs_touch_ref));
            if (rv != fs_expected{i.key}) {
                push diffs, sprintf("%s: %s", i.key, rv);
            }
        }
        push results, "owned-builtin-other-domain=" + (diffs ? diffs.join("; ") : "ok");
    }
    check_owned("owned-builtin-other-domain-allowed", "fsTouch", probe, fs_touch_ref, PO_NO_UNCONTROLLED_APIS,
        NOTHING, "called");
    # a method in both domains: its uncontrolled API is the owner's, its other domain is checked against the caller
    code mixed_touch_ref = owner.callFunction("make_mixed_touch_ref", probe);
    {
        list<string> diffs = ();
        foreach hash<auto> i in (OwnedForms.pairIterator()) {
            string rv = run_owned(sprintf(i.value, "mixedTouch"), PO_NO_FILESYSTEM, NOTHING,
                (probe, mixed_touch_ref));
            if (rv != fs_expected{i.key}) {
                push diffs, sprintf("%s: %s", i.key, rv);
            }
        }
        push results, "owned-builtin-mixed-domain=" + (diffs ? diffs.join("; ") : "ok");
    }
    check_owned("owned-builtin-mixed-domain-uncontrolled", "mixedTouch", probe, mixed_touch_ref,
        PO_NO_UNCONTROLLED_APIS, NOTHING, "called");

    # the uncontrolled API of a builtin class that no Program owns (a class of a binary module) is checked against the
    # calling code, also when the call reaches it through a class of the owner (qore#7543); the owner's call reference
    # keeps the owner's rights
    {
        hash<string, string> unowned_expected = {"object": ParseRefused, "dynamic": ParseRefused,
            "call-ref": ParseRefused, "owner-call-ref": "called"};
        foreach hash<auto> c in ({"unowned-builtin-through-owner-class": "make_script_probe",
                "unowned-builtin-object": "make_static_probe"}.pairIterator()) {
            object obj = owner.callFunction(c.value);
            code ref = owner.callFunction("make_touch_ref", obj);
            list<string> diffs = ();
            foreach hash<auto> i in (OwnedForms.pairIterator()) {
                string rv = run_owned(sprintf(i.value, "touch"), PO_NO_UNCONTROLLED_APIS, NOTHING, (obj, ref));
                if (rv != unowned_expected{i.key}) {
                    push diffs, sprintf("%s: %s", i.key, rv);
                }
            }
            push results, c.key + "=" + (diffs ? diffs.join("; ") : "ok");
        }
    }

    # the objects of a module that trusted code loaded first are the module's capabilities; a sandbox still refuses
    # them
    string trusted_code = sprintf("%%requires %s
auto sub go(object owner) { object o = TrustedProbeMaker::make(); return o.touch(); }", trusted_maker);
    {
        string rv = run_owned(trusted_code, PO_NO_UNCONTROLLED_APIS, NOTHING, (owner,));
        push results, "owned-builtin-trusted-module=" + (rv == "called" ? "ok" : rv);
        rv = run_owned(trusted_code, 0, deny(), (owner,));
        push results, "owned-builtin-trusted-module-sandboxed=" + (rv == SandboxRefused ? "ok" : rv);
    }

    # code restricted from the domain cannot create such an object for a Program with more rights than its own: the
    # class that it creates, that a module that it loads first creates, that a Program that it creates creates, and
    # that the owner creates when it calls a call reference or a closure of the restricted code are its own or of a
    # Program with its restrictions, and refused to it; the owner calling the method of such an object is refused too
    hash<string, string> own = {
        "owned-builtin-own-class": "%requires restrictedbuiltinprobe
auto sub go(object owner) { return RestrictedBuiltinProbe::make().touch(); }",
        "owned-builtin-own-module": sprintf("%%requires %s
auto sub go(object owner) { object o = RestrictedProbeMaker::make(); return o.touch(); }", restricted_maker),
        "owned-builtin-own-program": "auto sub go(object owner) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS);
    p.parse(\"%requires restrictedbuiltinprobe\nobject sub make() { return RestrictedBuiltinProbe::make(); }\", \"p\");
    object o = p.callFunction(\"make\");
    return o.touch();
}",
        "owned-builtin-owner-calls-ref": "%requires restrictedbuiltinprobe
auto sub go(object owner) {
    object o = owner.callFunction(\"call_it\", \\RestrictedBuiltinProbe::make());
    return o.touch();
}",
        "owned-builtin-owner-calls-closure": "%requires restrictedbuiltinprobe
auto sub go(object owner) {
    object o = owner.callFunction(\"call_it\", object sub () { return RestrictedBuiltinProbe::make(); });
    return o.touch();
}",
        "owned-builtin-owner-calls-own-object": "%requires restrictedbuiltinprobe
auto sub go(object owner) {
    return owner.callFunction(\"call_touch\", RestrictedBuiltinProbe::make());
}",
    };
    foreach hash<auto> i in (own.pairIterator()) {
        string rv = run_owned(i.value, PO_NO_UNCONTROLLED_APIS, NOTHING, (owner,));
        push results, i.key + "=" + (rv == ParseRefused ? "ok" : rv);
    }
}

listener.close();

print(results.join("\n") + "\n");
