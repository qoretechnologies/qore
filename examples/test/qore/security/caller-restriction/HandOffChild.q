#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of caller-restriction-hand-offs.qtest, each in a process of its own, as the process-wide state that they
# check is created once: ARGV[0] is the probe, ARGV[1] the order of the callers ("r" or "u" first), ARGV[2] a directory
# for the files; prints one line per caller

%modern
%no-child-restrictions
%requires HttpServer
%requires HandOffProbe
%requires ../../../TestCertificates.qm

string probe = ARGV[0];
string dir = ARGV[2];
string cert = dir + "/cert.pem";
{
    File f();
    f.open2(cert, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    f.write(TestCertificates::CertPem + TestCertificates::KeyPem);
}

const Source = "%modern
%requires HandOffProbe
%requires HttpServer
auto sub run(string probe, string cert) {
    switch (probe) {
        case \"global\": return HandOffProbe::global_task();
        case \"module-task\": return HandOffProbe::module_task();
        case \"ws-frame\": return HandOffProbe::ws_frame();
        # each caller creates its own server; the restricted caller adds a listener without a certificate first, and a
        # listener with a certificate file then
        case \"https\": {
            HttpServer server(<HttpServerOptionInfo>{});
            on_exit server.stop();
            try {
                server.addListener(<HttpListenerOptionInfo>{\"node\": \"127.0.0.1\", \"service\": 0});
                server.addListener(<HttpListenerOptionInfo>{\"node\": \"127.0.0.1\", \"service\": 0, \"cert_loc\": cert});
                return \"listening\";
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
        }
    }
}
";

Program r(PO_NEW_STYLE | PO_NO_FILESYSTEM);
r.parse(Source, "restricted");
Program u(PO_NEW_STYLE);
u.parse(Source, "unrestricted");
hash<string, Program> callers = {"r": r, "u": u};
foreach string c in (ARGV[1] == "r" ? ("r", "u") : ("u", "r")) {
    printf("%s: %y\n", c, callers{c}.callFunction("run", probe, cert));
}
