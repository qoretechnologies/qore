# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Private access is given to the class itself, never to another class with the same name and signature (qore#7036);
# see class-identity-private-access.qtest
#
# Each class below has a look-alike: a class with the same name and the same declarations (members, methods and their
# signatures).  The look-alikes are declared in another namespace of this Program, in a child Program and in a
# sandboxed child Program; none of them may read or write the private and private:internal members of an object of
# the real class, call its private and private:internal methods or see its non-public members in an enumeration of
# the object.  The real class, its subclasses and its copies imported or injected into a child Program keep their
# access.

%modern
%no-child-restrictions

%requires HttpServer
%requires HttpServerUtil

#! a plain user class
class Shape {
    private {
        string secret = "secret";
    }

    private:internal {
        string isecret = "isecret";
    }

    private string helper() {
        return "helper";
    }

    private:internal string ihelper() {
        return "ihelper";
    }

    private static string shelper() {
        return "shelper";
    }

    private {
        static string svar = "svar";
        const SCONST = "sconst";
    }

    #! the operation \a what of the code of the class on the object \a o
    auto get(string what, object o) {
        switch (what) {
            case "read": return o.secret;
            case "write": o.secret = "changed"; return o.secret;
            case "call": return o.helper();
            case "iread": return o.isecret;
            case "iwrite": o.isecret = "ichanged"; return o.isecret;
            case "icall": return o.ihelper();
            case "dynamic": return o.("sec" + "ret");
            case "idynamic": return o.("isec" + "ret");
            case "keys": return keys o;
            case "first": return o.firstKey();
            case "last": return o.lastKey();
            case "slice": return o{"secret", "isecret"};
            case "iterator": {
                list<string> l = ();
                ObjectIterator i(o);
                while (i.next()) {
                    l += i.getKey();
                }
                return l;
            }
            case "method": return call_object_method(o, "helper");
            case "imethod": return call_object_method(o, "ihelper");
            case "callable": return o.hasCallableMethod("ihelper");
            case "reference": {
                code c = \o.helper();
                return c();
            }
        }
        throw "UNKNOWN-OPERATION", what;
    }
}

#! a subclass: the private declarations of the class are its own, the private:internal declarations are not
class SubShape inherits Shape {
    auto subGet(string what, object o) {
        switch (what) {
            case "read": return o.secret;
            case "write": o.secret = "changed"; return o.secret;
            case "call": return o.helper();
            case "iread": return o.isecret;
            case "icall": return o.ihelper();
        }
        throw "UNKNOWN-OPERATION", what;
    }
}

namespace Fake {
#! the look-alike of ::Shape in another namespace of this Program
class Shape {
    private {
        string secret = "secret";
    }

    private:internal {
        string isecret = "isecret";
    }

    private string helper() {
        return "helper";
    }

    private:internal string ihelper() {
        return "ihelper";
    }

    private static string shelper() {
        return "shelper";
    }

    private {
        static string svar = "svar";
        const SCONST = "sconst";
    }

    #! the operation \a what of the code of the class on the object \a o
    auto get(string what, object o) {
        switch (what) {
            case "read": return o.secret;
            case "write": o.secret = "changed"; return o.secret;
            case "call": return o.helper();
            case "iread": return o.isecret;
            case "iwrite": o.isecret = "ichanged"; return o.isecret;
            case "icall": return o.ihelper();
            case "dynamic": return o.("sec" + "ret");
            case "idynamic": return o.("isec" + "ret");
            case "keys": return keys o;
            case "first": return o.firstKey();
            case "last": return o.lastKey();
            case "slice": return o{"secret", "isecret"};
            case "iterator": {
                list<string> l = ();
                ObjectIterator i(o);
                while (i.next()) {
                    l += i.getKey();
                }
                return l;
            }
            case "method": return call_object_method(o, "helper");
            case "imethod": return call_object_method(o, "ihelper");
            case "callable": return o.hasCallableMethod("ihelper");
            case "reference": {
                code c = \o.helper();
                return c();
            }
        }
        throw "UNKNOWN-OPERATION", what;
    }
}

#! the look-alike of HttpServer::CallbackInputStream (HttpServerUtil), a module class that inherits a builtin class
class CallbackInputStream inherits InputStream {
    private {
        code callback;
        *object owner;
        *binary buf;
        bool eof = False;
    }

    constructor(code callback, *object owner) {
        self.callback = callback;
        self.owner = owner;
    }

    *binary read(int limit) {
        switch (limit) {
            case 0: return binary(sprintf("%y", ::target.eof));
            case 1: ::target.eof = True; return binary(sprintf("%y", ::target.eof));
            case 2: return binary(sprintf("%y", keys ::target));
        }
    }

    int peek() {
        return -1;
    }
}

#! the look-alike of HttpServer::HttpListenerHandlerView (HttpServer), a module class with a private:internal
#! member
class HttpListenerHandlerView inherits HttpServer::HttpListenerInterface {
    private:internal {
        *HttpListener listener;
    }

    constructor(HttpListener listener) {
        self.listener = listener;
    }

    log(string fmt, ...) {
    }

    logError(string fmt, ...) {
    }

    logResponse(hash<auto> cx, hash<auto> rv) {
    }

    removeExternalDedicatedSocket(softstring id) {
        ::target.listener = NOTHING;
    }

    removeExternalDedicatedSocket(softstring id, HttpServer::AbstractHttpSocketHandlerInterface handler) {
    }

    int getMaxDecodedBodySize() {
        return 0;
    }

    string getDefaultTextEncoding() {
        auto v = ::target.listener;
        return v ? v.className() : "empty";
    }

    *hash<auto> getDefaultResponseHeaders() {
    }

    addUserThreadContext(hash<auto> uctx) {
    }

    auto removeUserThreadContext(*string k) {
    }

    signalPersistentSessionClose(softstring uh) {
    }

    signalPersistentSessionClose(softstring uh, HttpServer::AbstractHttpRequestHandler handler) {
    }
}
}

#! a subclass of a class of a module (an AOT-compiled module when its qmod is used): the private members of the
#! module class are its own, on its objects and on the other objects of the module class
class SubStream inherits HttpServer::CallbackInputStream {
    constructor() : HttpServer::CallbackInputStream(sub () {}) {
    }

    string eofOf(HttpServer::CallbackInputStream o) {
        return sprintf("%y/%y", eof, o.eof);
    }
}

#! the object that the look-alikes of the module classes try to use
our auto target;

#! the operations of Shape::get()
const Operations = ("read", "write", "call", "iread", "iwrite", "icall", "dynamic", "idynamic", "keys", "first",
    "last", "slice", "iterator", "method", "imethod", "callable", "reference");

#! the operations of SubShape::subGet()
const SubOperations = ("read", "write", "call", "iread", "icall");

#! the look-alikes in a child Program: in its root namespace, with the path of the real classes
const LookAlikeSource = "%modern
%requires HttpServer
%requires HttpServerUtil
%requires reflection

our auto target;

class Shape {
    private {
        string secret = \"secret\";
    }

    private:internal {
        string isecret = \"isecret\";
    }

    private string helper() {
        return \"helper\";
    }

    private:internal string ihelper() {
        return \"ihelper\";
    }

    private static string shelper() {
        return \"shelper\";
    }

    private {
        static string svar = \"svar\";
        const SCONST = \"sconst\";
    }

    auto get(string what, object o) {
        switch (what) {
            case \"read\": return o.secret;
            case \"write\": o.secret = \"changed\"; return o.secret;
            case \"call\": return o.helper();
            case \"iread\": return o.isecret;
            case \"iwrite\": o.isecret = \"ichanged\"; return o.isecret;
            case \"icall\": return o.ihelper();
            case \"dynamic\": return o.(\"sec\" + \"ret\");
            case \"idynamic\": return o.(\"isec\" + \"ret\");
            case \"keys\": return keys o;
            case \"first\": return o.firstKey();
            case \"last\": return o.lastKey();
            case \"slice\": return o{\"secret\", \"isecret\"};
            case \"iterator\": {
                list<string> l = ();
                ObjectIterator i(o);
                while (i.next()) {
                    l += i.getKey();
                }
                return l;
            }
            case \"method\": return call_object_method(o, \"helper\");
            case \"imethod\": return call_object_method(o, \"ihelper\");
            case \"callable\": return o.hasCallableMethod(\"ihelper\");
            case \"reference\": {
                code c = \\o.helper();
                return c();
            }
        }
        throw \"UNKNOWN-OPERATION\", what;
    }
}

class CallbackInputStream inherits InputStream {
    private {
        code callback;
        *object owner;
        *binary buf;
        bool eof = False;
    }

    constructor(code callback, *object owner) {
        self.callback = callback;
        self.owner = owner;
    }

    *binary read(int limit) {
        switch (limit) {
            case 0: return binary(sprintf(\"%y\", ::target.eof));
            case 1: ::target.eof = True; return binary(sprintf(\"%y\", ::target.eof));
            case 2: return binary(sprintf(\"%y\", keys ::target));
        }
    }

    int peek() {
        return -1;
    }
}

namespace HttpServer {
class HttpListenerHandlerView inherits HttpServer::HttpListenerInterface {
    private:internal {
        *HttpListener listener;
    }

    constructor(HttpListener listener) {
        self.listener = listener;
    }

    log(string fmt, ...) {
    }

    logError(string fmt, ...) {
    }

    logResponse(hash<auto> cx, hash<auto> rv) {
    }

    removeExternalDedicatedSocket(softstring id) {
        ::target.listener = NOTHING;
    }

    removeExternalDedicatedSocket(softstring id, HttpServer::AbstractHttpSocketHandlerInterface handler) {
    }

    int getMaxDecodedBodySize() {
        return 0;
    }

    string getDefaultTextEncoding() {
        auto v = ::target.listener;
        return v ? v.className() : \"empty\";
    }

    *hash<auto> getDefaultResponseHeaders() {
    }

    addUserThreadContext(hash<auto> uctx) {
    }

    auto removeUserThreadContext(*string k) {
    }

    signalPersistentSessionClose(softstring uh) {
    }

    signalPersistentSessionClose(softstring uh, HttpServer::AbstractHttpRequestHandler handler) {
    }
}
}

auto sub shape(string what, object o) {
    return new Shape().get(what, o);
}

#! a method of the look-alike called through reflection on an object of the real class
auto sub reflect(string what, object o) {
    return Class::forName(\"Shape\").getNormalMethod(\"get\").call(o, what, o);
}

auto sub stream(int op, object o) {
    target = o;
    return new CallbackInputStream(sub () {}).read(op);
}

string sub view(bool write, object o, HttpListener l) {
    target = o;
    HttpServer::HttpListenerHandlerView v(l);
    if (write) {
        v.removeExternalDedicatedSocket(\"x\");
        return \"written\";
    }
    return v.getDefaultTextEncoding();
}
";

#! the code of a child Program that uses the copy of the real class imported into it
const ImportSource = "%modern
auto sub shape(string what, object o) {
    return new Shape().get(what, o);
}

object sub make() {
    return new Shape();
}
";

#! the code of a child Program into which SubShape is injected as Shape
const InjectionSource = "%modern
auto sub shape(string what, object o) {
    return new Shape().subGet(what, o);
}

object sub make() {
    return new Shape();
}
";

#! a look-alike of Shape beside the copy of Shape imported into the Program: the private static declarations of Shape
#! are refused at parse time
const StaticSources = {
    "method": "::Shape::shelper()",
    "variable": "::Shape::svar",
    "constant": "::Shape::SCONST",
};

const StaticLookAlike = "%modern
namespace Fake {
class Shape {
    private {
        string secret = \"secret\";
    }

    private:internal {
        string isecret = \"isecret\";
    }

    private string helper() {
        return \"helper\";
    }

    private:internal string ihelper() {
        return \"ihelper\";
    }

    private static string shelper() {
        return \"shelper\";
    }

    private {
        static string svar = \"svar\";
        const SCONST = \"sconst\";
    }

    auto get(string what, object o) {
        return f();
    }
}
}
";

#! the look-alike of a child Program whose method is bound to an object of the real class: its code uses the members
#! and methods of self (a method of a class runs only on an object of its class or of a subclass)
const BindSource = "
auto sub ref_qualified(string what, object o) {
    code c = o.getCallReference(\"Shape::get\");
    return c(what, o);
}

auto sub ref_unqualified(object o) {
    code c = o.getCallReference(\"get\");
    return c(\"call\", o);
}

auto sub method_qualified(string what, object o) {
    return call_object_method(o, \"Shape::get\", what, o);
}

auto sub reflect_bound(string what, object o) {
    return Class::forName(\"Shape\").getNormalMethod(\"get\").call(o, what, o);
}
";

#! the operations of the look-alike's code on self (BindSource)
const BindOperations = ("bare", "barewrite", "bareicall", "bareiwrite");

#! the cases of the look-alike's get() for BindOperations
const BindCases = "case \"bare\": return secret;
            case \"barewrite\": secret = \"changed\"; return secret;
            case \"bareicall\": return ihelper();
            case \"bareiwrite\": isecret = \"ichanged\"; return isecret;
            case \"read\":";

#! a look-alike beside the copy of Shape imported into the Program, whose code calls the method of Shape on self
const QualifiedSource = "%modern
namespace Fake {
class Shape {
    private {
        string secret = \"secret\";
    }

    private:internal {
        string isecret = \"isecret\";
    }

    private string helper() {
        return \"helper\";
    }

    private:internal string ihelper() {
        return \"ihelper\";
    }

    private static string shelper() {
        return \"shelper\";
    }

    private {
        static string svar = \"svar\";
        const SCONST = \"sconst\";
    }

    auto get(string what, object o) {
        return ::Shape::get(what, self);
    }
}
}

auto sub qualified(string what) {
    return new Fake::Shape().get(what, new Fake::Shape());
}
";

#! a handler that keeps the listener object that it is given
class KeepingHandler inherits AbstractHttpRequestHandler {
    public {
        *HttpListenerInterface kept;
    }

    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        kept = listener;
        return makeResponse(200, "kept");
    }
}

#! the result of an operation: its value, or the exception that it raised
string sub result(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! prints the results of the operations of Shape::get() of a context on a new object of ::Shape
sub shape_case(string label, code get) {
    foreach string what in (Operations) {
        Shape o();
        printf("%s %s: %s\n", label, what, result(auto sub () { return get(what, o); }));
    }
}

#! the child Programs: a plain one and a sandboxed one
hash<string, Program> sub look_alike_programs() {
    Program child(PO_NO_CHILD_PO_RESTRICTIONS);
    child.parse(LookAlikeSource, "look-alike");
    Program sandboxed(PO_NO_CHILD_PO_RESTRICTIONS);
    sandboxed.setSandboxManager(new SandboxManager());
    sandboxed.parse(LookAlikeSource, "sandboxed-look-alike");
    return {"program": child, "sandbox": sandboxed};
}

hash<string, Program> programs = look_alike_programs();

# (iii) a plain user class: the real class, a subclass and the imported copy keep their access
shape_case("own", auto sub (string what, object o) { return new Shape().get(what, o); });
foreach string what in (SubOperations) {
    Shape o();
    printf("sub %s: %s\n", what, result(auto sub () { return new SubShape().subGet(what, o); }));
}
{
    Program imp(PO_NO_CHILD_PO_RESTRICTIONS);
    imp.importClass("Shape");
    imp.parse(ImportSource, "import");
    shape_case("import", auto sub (string what, object o) { return imp.callFunction("shape", what, o); });
    # an object of the imported copy for the real class's code: the same private:internal storage
    shape_case("import-object", auto sub (string what, object o) {
        return new Shape().get(what, imp.callFunction("make"));
    });
}
# a class injected in place of Shape (a subclass of it) keeps the access of its class
{
    Program inj(PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_INJECTION);
    inj.importClass("SubShape", "Shape", True);
    inj.parse(InjectionSource, "injection");
    foreach string what in (SubOperations) {
        Shape o();
        printf("injection %s: %s\n", what, result(auto sub () { return inj.callFunction("shape", what, o); }));
    }
    shape_case("injection-object", auto sub (string what, object o) {
        return new Shape().get(what, inj.callFunction("make"));
    });
}
# the look-alikes have no access
shape_case("namespace", auto sub (string what, object o) { return new Fake::Shape().get(what, o); });
foreach hash<auto> i in (programs.pairIterator()) {
    Program p = i.value;
    shape_case(i.key, auto sub (string what, object o) { return p.callFunction("shape", what, o); });
}

# the private static declarations of Shape for a look-alike beside the imported copy of Shape, at parse time
foreach hash<auto> i in (StaticSources.pairIterator()) {
    Program p(PO_NO_CHILD_PO_RESTRICTIONS);
    p.importClass("Shape");
    printf("static %s: %s\n", i.key, result(auto sub () {
        p.parse(replace(StaticLookAlike, "return f();", "return " + i.value + ";")
            + "auto sub f() { return new Fake::Shape().get(\"\", new Fake::Shape()); }", "static");
        return p.callFunction("f");
    }));
}

# a method of a look-alike called through reflection on an object of the real class
foreach hash<auto> i in (programs.pairIterator()) {
    Program p = i.value;
    foreach string what in (("read", "iread", "call")) {
        Shape o();
        printf("reflection %s %s: %s\n", i.key, what, result(auto sub () {
            return p.callFunction("reflect", what, o);
        }));
    }
}

# a method of a look-alike bound to an object of the real class, whose code uses self
{
    string src = replace(LookAlikeSource, "case \"read\":", BindCases) + BindSource;
    Program bound(PO_NO_CHILD_PO_RESTRICTIONS);
    bound.parse(src, "bound");
    Program bound_sandboxed(PO_NO_CHILD_PO_RESTRICTIONS);
    bound_sandboxed.setSandboxManager(new SandboxManager());
    bound_sandboxed.parse(src, "bound-sandboxed");
    foreach hash<auto> i in ({"program": bound, "sandbox": bound_sandboxed}.pairIterator()) {
        Program p = i.value;
        foreach string what in (BindOperations) {
            foreach string f in (("ref_qualified", "method_qualified", "reflect_bound")) {
                Shape o();
                printf("bound %s %s %s: %s\n", i.key, f, what, result(auto sub () {
                    return p.callFunction(f, what, o);
                }));
            }
        }
        # the method of the object's class: its own access
        Shape o();
        printf("own-bound %s ref_unqualified: %s\n", i.key, result(auto sub () {
            return p.callFunction("ref_unqualified", o);
        }));
    }
}

# the method of the real class called on an object of a look-alike (the qualified call of a method of a base class)
{
    Program p(PO_NO_CHILD_PO_RESTRICTIONS);
    p.importClass("Shape");
    printf("bound qualified call: %s\n", result(auto sub () {
        p.parse(QualifiedSource, "qualified");
        return p.callFunction("qualified", "read");
    }));
}

# (ii) a module class that inherits a builtin class: HttpServer::CallbackInputStream
printf("stream subclass read: %s\n", result(auto sub () {
    return new SubStream().eofOf(new HttpServer::CallbackInputStream(sub () {}));
}));
{
    list<string> ops = ("read", "write", "keys");
    foreach int op in (range(3)) {
        HttpServer::CallbackInputStream o(sub () {});
        target = o;
        printf("stream namespace %s: %s\n", ops[op], result(auto sub () {
            return binary_to_string(new Fake::CallbackInputStream(sub () {}).read(op));
        }));
        foreach hash<auto> i in (programs.pairIterator()) {
            Program p = i.value;
            o = new HttpServer::CallbackInputStream(sub () {});
            printf("stream %s %s: %s\n", i.key, ops[op], result(auto sub () {
                return binary_to_string(p.callFunction("stream", op, o));
            }));
        }
    }
}

# (i) the listener object that the handlers of HttpServer get: HttpServer::HttpListenerHandlerView
{
    KeepingHandler handler();
    HttpServer server(<HttpServerOptionInfo>{});
    on_exit delete server;
    server.setDefaultHandler("keep", handler);
    int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
    server.waitForAsyncIo();
    HTTPClient hc({"url": "http://127.0.0.1:" + port, "timeout": 10s});
    hc.get("/");
    object view = handler.kept;
    HttpListener l(new HttpServer(<HttpServerOptionInfo>{}), sub () {}, 0, new Sequence(),
        <HttpListenerOptionInfo>{"name": "look-alike", "no_bind": True});
    target = view;
    # the reads first: a write that is not refused changes the object
    printf("view namespace read: %s\n", result(auto sub () {
        return new Fake::HttpListenerHandlerView(l).getDefaultTextEncoding();
    }));
    foreach hash<auto> i in (programs.pairIterator()) {
        Program p = i.value;
        printf("view %s read: %s\n", i.key, result(auto sub () { return p.callFunction("view", False, view, l); }));
    }
    printf("view namespace write: %s\n", result(auto sub () {
        new Fake::HttpListenerHandlerView(l).removeExternalDedicatedSocket("x");
        return "written";
    }));
    foreach hash<auto> i in (programs.pairIterator()) {
        Program p = i.value;
        printf("view %s write: %s\n", i.key, result(auto sub () { return p.callFunction("view", True, view, l); }));
    }
    # the object is unchanged
    printf("view listener: %s\n", view.getDefaultTextEncoding());
}
