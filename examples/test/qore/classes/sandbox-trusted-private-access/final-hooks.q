#!/usr/bin/env qore
# the public methods that library classes give their subclasses in place of their private members are final: a
# subclass cannot redeclare them (qore#6843); run by ../sandbox-trusted-private-access.qtest in every execution mode

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires FtpPoller
%requires DataProvider
%requires HttpServerUtil
%requires RestHandler

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! the class, the method's signature, a redeclaration with another return type
const Methods = (
    ("FtpPoller::FtpPoller", "setFatal(bool fatal = True)", "nothing", "bool setFatal(bool fatal = True)"),
    ("FtpPoller::FtpPoller", "isFatal()", "bool", "int isFatal()"),
    ("DataProvider::Observable", "hasObservers()", "bool", "int hasObservers()"),
    ("DataProvider::Observable", "getObserverCount()", "int", "string getObserverCount()"),
    ("HttpServer::AbstractStreamRequest", "getContext()", "hash<auto>", "string getContext()"),
    ("HttpServer::AbstractStreamRequest", "getHeaders()", "hash<auto>", "string getHeaders()"),
    ("HttpServer::AbstractStreamRequest", "getBody()", "auto", "string getBody()"),
    ("RestHandler::AbstractRestStreamRequestHandler", "getContext()", "hash<auto>", "string getContext()"),
    ("RestHandler::AbstractRestStreamRequestHandler", "getArgs()", "*hash<auto>", "string getArgs()"),
);

string sub try_parse(string label, string code) {
    Program p(Opts);
    p.loadModule("FtpPoller");
    p.loadModule("DataProvider");
    p.loadModule("HttpServerUtil");
    p.loadModule("RestHandler");
    try {
        p.parse(code, label);
        return "parse OK";
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

foreach list<string> m in (Methods) {
    string name = (m[1] =~ x/^([a-zA-Z]+)\(/)[0];
    string body = m[2] == "nothing" ? "" : "return " + (m[2] =~ /hash/ ? "{}" : (m[2] =~ /int/ ? "0" : (m[2] == "bool"
        ? "False" : "NOTHING"))) + ";";
    string other_body = m[3] =~ /^bool/ ? "return False;" : (m[3] =~ /^int/ ? "return 0;" : "return '';");
    # the same signature
    printf("%s::%s same signature: %s\n", m[0], name, try_parse("same", sprintf("class X inherits %s { %s %s { %s } }",
        m[0], m[2], m[1], body)));
    # another return type
    printf("%s::%s other return type: %s\n", m[0], name, try_parse("return", sprintf("class X inherits %s { %s { %s } }",
        m[0], m[3], other_body)));
    # a private redeclaration
    printf("%s::%s private: %s\n", m[0], name, try_parse("private", sprintf("class X inherits %s { private %s %s { %s } "
        "}", m[0], m[2], m[1], body)));
    # other parameters: refused, as the name of a final method is reserved (qore#7111)
    printf("%s::%s other parameters: %s\n", m[0], name, try_parse("params", sprintf("class X inherits %s { %s %s(string "
        "x) { %s } }", m[0], m[2], name, body)));
    # a static method of the name: refused, as the name of a final method is reserved (qore#7111)
    printf("%s::%s static: %s\n", m[0], name, try_parse("static", sprintf("class X inherits %s { static %s %s(string "
        "x) { %s } }", m[0], m[2], name, body)));
}

# a call of a final method on an object of a subclass that sandboxed code declares calls the library's method
Program p(Opts);
p.loadModule("DataProvider");
p.parse("class X inherits DataProvider::Observable {
}
auto sub run() {
    X x();
    DataProvider::Observable o = x;
    return (x.getObserverCount(), o.getObserverCount(), x.hasObservers(), o.hasObservers());
}", "variants");
printf("final method called: %y\n", p.callFunction("run"));
