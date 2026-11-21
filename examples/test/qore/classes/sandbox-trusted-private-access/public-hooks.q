#!/usr/bin/env qore
# the public methods of library classes that subclasses declared by sandboxed code use in place of the private members
# of the library classes (qore#6843); run by ../sandbox-trusted-private-access.qtest in every execution mode

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires FtpPoller
%requires FsUtil
%requires Util

%include ../FtpServer.qc

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the library modules and the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.setSandboxManager(new SandboxManager());
    p.loadModule("FtpPoller");
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

const PollerHead = "class MyPoller inherits FtpPoller::FtpPoller {
    constructor(Qore::FtpClient f) : FtpPoller::FtpPoller(f, {'path': '/', 'mask': '*'}) {}
    singleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
    postSingleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
";

# the private members themselves: refused at parse time
const ParseCases = (
    "ftppoller-fatal-member": PollerHead + "    bool f() { fatal = True; return fatal; } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

const Tenant = PollerHead + "    list<bool> useFatal() {
        list<bool> rv = (isFatal(),);
        setFatal();
        rv += isFatal();
        setFatal(False);
        rv += isFatal();
        return rv;
    }
}

auto sub run_poller(Qore::FtpClient f) {
    MyPoller p(f);
    return p.useFatal();
}
";

*Program p = sandboxed("tenant", Tenant);
if (!p) {
    exit(1);
}

sub show(string label, code c) {
    try {
        printf("%s: OK %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

# a client of the host connected to a local server: the poller uses it as it is
FtpServer serv(0, 0, "127.0.0.1");
on_exit serv.shutdown();
FtpClient ftp("ftp://user:pass@127.0.0.1:" + serv.getPort());
ftp.connect();
on_exit ftp.disconnect();
show("ftppoller fatal flag", auto sub () { return p.callFunction("run_poller", ftp); });
