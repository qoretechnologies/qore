#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of module-host-info.qtest (qore#7586): a host that preloads modules into Programs and then restricts
# them from external information, as an application does for its sandboxed code; the code of each Program
# (module-host-info-child.q, at top level) calls the modules' functions that read host information; prints one
# "<kind>/<mode>/<case>=ok" line per check (or "...=expected <x>, got <y>")
#
# the environment is set by the test: TMPDIR (a directory of the test), NO_COLOR, Q7586_SECRET, QUNIT_Q7586,
# QORE_EDIFACT_DIRCACHE, TERM, DISPLAY, PRINTER, QORE_CONNECTION_PROVIDERS and QORE_DATASOURCE_PROVIDERS (the path of
# Q7586Providers.qm) and Q7586_CHILD (the path of the code of the Programs)

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}/../ambient-host"

%requires Util
%requires FsUtil
%requires HttpServerUtil
%requires HttpServer
%requires Logger
%requires ElasticSearchLoggerAppender
%requires OpenSearchLoggerAppender
%requires SmtpClient
%requires TelnetClient
%requires QUnit
%requires ConnectionProvider
%requires DatasourceProvider
%requires EdifactUtil
%requires McpServerHandler
%requires A2aServerHandler
%requires QoreRepl
%requires QLS
%requires DataProvider
%requires QoreOnnxTools
%requires FileLocationHandler
%requires AmbientCallers

#! the modules that the host preloads into each Program
const Modules = ("Util", "FsUtil", "HttpServerUtil", "HttpServer", "Logger", "ElasticSearchLoggerAppender",
    "OpenSearchLoggerAppender", "SmtpClient", "TelnetClient", "QUnit", "ConnectionProvider", "DatasourceProvider",
    "EdifactUtil", "McpServerHandler", "A2aServerHandler", "QoreRepl", "QLS", "DataProvider", "QoreOnnxTools",
    "FileLocationHandler");

#! the kinds of Programs: whether the code may read external information
const Kinds = {
    "unrestricted": {"restricted": False, "sandboxed": False},
    "restricted": {"restricted": True, "sandboxed": False},
    # restricted from the start: the code requires the modules that the host's process has loaded, which are not
    # parsed again for it
    "restricted-requires": {"restricted": True, "sandboxed": False, "requires": True},
    "sandboxed-restricted": {"restricted": True, "sandboxed": True},
    # a sandbox that allows everything: it governs the code without restricting it from external information
    "sandboxed": {"restricted": False, "sandboxed": True},
};

#! the execution modes of the Programs
const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED};

list<string> results = ();

sub check(string key, auto expected, auto actual) {
    push results, key + "=" + (expected === actual ? "ok" : sprintf("expected %y, got %y", expected, actual));
}

#! records the EHLO commands of the SMTP clients: one connection after another, until a connection sends "STOP"
class SmtpRecorder {
    public {
        Socket server();
        int port;
        Queue lines();
        Counter done(1);
    }

    constructor() {
        server.bind("127.0.0.1:0");
        server.listen();
        port = server.getSocketInfo().port;
        background run();
    }

    private run() {
        on_exit done.dec();
        while (True) {
            # no timeout: the loop ends with the "STOP" connection of stop(), however long the checks take
            Socket s = server.accept();
            s.send("220 q7586 ESMTP\r\n");
            string line = trim(SmtpRecorder::readLine(s));
            if (line == "STOP") {
                s.close();
                return;
            }
            lines.push(line);
            s.send("250 q7586\r\n");
            s.close();
        }
    }

    #! reads a line
    static string readLine(Socket s) {
        string line = "";
        while (line !~ /\n/) {
            line += s.recv(0, 30s);
        }
        return line;
    }

    stop() {
        Socket s();
        s.connect(sprintf("127.0.0.1:%d", port));
        SmtpRecorder::readLine(s);
        s.send("STOP\r\n");
        done.waitForZero();
        s.close();
        server.close();
    }
}

string child_source = ReadOnlyFile::readTextFile(ENV.Q7586_CHILD);
string hostname = gethostname();
string cwd = getcwd();
string pid = getpid().toString();
string tmp_host = tmp_location();
string tmp_free = realpath(PlatformOS == "Windows" ? "c:\\temp" : "/tmp");
string url_wild_host = http_get_url_from_bind("0.0.0.0:8080");
string url_wild6_host = http_get_url_from_bind("[::]:8080");
string ds = "pgsql:host-user/host-secret@hostdb";

#! what code that may read external information gets
hash<auto> may_read = {
    "tmp": tmp_host,
    "tmpdir": tmp_host,
    "tmpfile": tmp_host,
    "norm": normalize_dir("a/../b/./c", cwd),
    "norm_up": normalize_dir_unix("../x/./y//z/..", cwd),
    "norm_corner": (map normalize_dir($1, cwd), ("", "./", "a/..", "../../a/../b")),
    "norm_relcwd": (normalize_dir("x", "."), normalize_dir("x", "d"), normalize_dir("../e", ".")),
    # the current working directory of a UNIX host prefixed as it is
    "norm_win": cwd + "\\b",
    "norm_abs": "/q7586/b",
    "norm_explicit": "/q7586/b",
    "rel": "./b/f",
    "rel_abs": "../b/f",
    "rel_dot": "../x",
    "rel_dotdir": "../y",
    "rel_sib": get_relative_path("a", "x", cwd),
    "rel_up": get_relative_path("..", "x", cwd),
    "rel_up2": get_relative_path("../a", "b", cwd),
    "rel_mixed": get_relative_path("/q7586", "y", cwd),
    "nocolor": True,
    "url_wild": url_wild_host,
    "url_wild6": url_wild6_host,
    "url_host": "http://localhost:8080",
    "errpage": True,
    "layout": sprintf("%s|%s|%s", hostname, pid, ENV.Q7586_SECRET),
    "es_host": {"name": hostname},
    "os_host": {"name": hostname},
    "smtp": "connected",
    "smtp_ehlo": "EHLO " + hostname,
    "telnet_env": {"PRINTER": ENV.PRINTER, "DISPLAY": ENV.DISPLAY},
    "telnet_term": ENV.TERM,
    "telnet_display": ENV.DISPLAY,
    "qunit_env": ENV.QUNIT_Q7586,
    "edifact": ENV.QORE_EDIFACT_DIRCACHE,
    "edifact_tilde": getenv("HOME") + "/q7586",
    "edifact_explicit": "/q7586/edifact",
    "conns": ("q7586",),
    "conn": "q7586://host-user:host-secret@host.example",
    "conn_hash": ("q7586",),
    "ds_hash": {"q7586": ds},
    "ds_string": ds,
    "mcp_pid": True,
    "a2a_pid": True,
    "repl_pwd": cwd,
    "repl_cd": "Changed to: " + cwd,
    "lent_errpage": True,
    "lent_layout": sprintf("%s|%s|%s", hostname, pid, ENV.Q7586_SECRET),
    "lent_layout_set": sprintf("%s|%s|%s", ENV.Q7586_SECRET, hostname, pid),
    # an appender of the host keeps its fields whatever code logs through it
    "lent_logger_line": sprintf("%s|%s|%s", hostname, pid, ENV.Q7586_SECRET),
    "smtp_async": "connected",
    "own_appender": sprintf("%s|%s|%s", hostname, pid, ENV.Q7586_SECRET),
    "vcwd_big": True,
    "vcwd_up": "../../../etc/passwd",
    "smtp_async_ehlo": "EHLO " + hostname,
    "qls_expand": ENV.Q7586_SECRET + "/" + ENV.Q7586_SECRET + "/d",
    "qls_moddirs": QLS::DocumentLinks::getEnvModuleDirs(),
    "dp_failures": ("q7586-no-such-data-provider-module",),
    "onnx_python": ENV.QORE_ONNX_PYTHON,
    "file_eff_virtual": False,
    "file_resolved": "q7586-b",
    "file_resolved_up": "q7586-b",
    "grant_barrier": tmp_host,
    "grant_capability": tmp_host,
    "lent_tmp": tmp_host,
    "lent_url": url_wild_host,
};

#! what code restricted from external information gets: no host information
hash<auto> may_not_read = may_read + {
    "tmp": tmp_free,
    "tmpdir": tmp_free,
    "tmpfile": tmp_free,
    "norm": "./b/c",
    "norm_up": "../x/y",
    "norm_corner": (".", ".", ".", "../../b"),
    "norm_win": ".\\b",
    # a relative working directory given by such code: the result is below it
    "norm_relcwd": ("./x", "./d/x", "../e"),
    "rel_sib": "../x",
    "rel_up": "INVALID-FUNCTION-ACCESS",
    "rel_up2": "INVALID-FUNCTION-ACCESS",
    "rel_mixed": "INVALID-FUNCTION-ACCESS",
    "nocolor": False,
    "url_wild": "http://127.0.0.1:8080",
    "url_wild6": "http://[::1]:8080",
    "url_host": "http://" + hostname + ":8080",
    "errpage": False,
    "layout": "||",
    "es_host": NOTHING,
    "os_host": NOTHING,
    "smtp_ehlo": "EHLO localhost",
    "telnet_env": {"DISPLAY": NOTHING},
    # TelnetClient's default terminal type
    "telnet_term": "xterm",
    "telnet_display": NOTHING,
    "qunit_env": NOTHING,
    "edifact": "EDIFACT-CACHE-ERROR",
    "edifact_tilde": "EDIFACT-CACHE-ERROR",
    "conns": (),
    "conn": "CONNECTION-ERROR",
    "conn_hash": (),
    "ds_hash": NOTHING,
    "ds_string": "DATASOURCE-ERROR",
    "mcp_pid": False,
    "a2a_pid": False,
    "repl_pwd": "REPL-ERROR",
    "repl_cd": "Changed directory",
    "lent_errpage": False,
    # a lent layout lends nothing: the code that calls its format methods gets no host values
    "lent_layout": "||",
    "lent_layout_set": "||",
    "own_appender": "||",
    "smtp_async_ehlo": "EHLO localhost",
    "qls_expand": "//d",
    "qls_moddirs": (),
    "dp_failures": ("QORE_DATA_PROVIDERS[0]",),
    "onnx_python": "python3",
    "file_eff_virtual": True,
    # a lent object lends nothing
    "lent_tmp": tmp_free,
    "lent_url": "http://127.0.0.1:8080",
};

# the values must differ, or the checks would not show what the code gets
foreach string k in (keys may_read) {
    if (may_read{k} === may_not_read{k} && k !~ /^(norm_abs|norm_explicit|rel|rel_abs|rel_dot|rel_dotdir|rel_sib|smtp|smtp_async|edifact_explicit|grant_.*|lent_logger_line|file_resolved.*|vcwd_.*)$/) {
        check("setup/" + k, "distinct values", "the same value");
    }
}

# objects that the host lends: of the AmbientCallers module, an HTTP server, a logger layout and loggers whose
# appenders write to the host's streams
AmbientCallers::LentWorker lent();
HttpServer::HttpServer host_server();
Logger::LoggerLayoutPattern host_layout("%h|%P|%E{Q7586_SECRET}");
StringOutputStream host_log_stream();
Logger::Logger host_logger("q7586-host", Logger::LoggerLevel::getLevelInfo());
{
    Logger::LoggerAppenderStream app("host", new Logger::LoggerLayoutPattern("%h|%P|%E{Q7586_SECRET}%n"),
        new StreamWriter(host_log_stream));
    app.open();
    host_logger.addAppender(app);
}
# a logger whose layout the Programs take and set a pattern of
Logger::Logger host_logger2("q7586-host2", Logger::LoggerLevel::getLevelInfo());
{
    Logger::LoggerAppenderStream app("host2", new Logger::LoggerLayoutPattern("%m%n"),
        new StreamWriter(new StringOutputStream()));
    host_logger2.addAppender(app);
}
# the token of the virtual working directory of the process, as restricted code sees it in its file URIs
*string vtoken;
{
    Program tp(PO_MODERN);
    tp.loadModule("FileLocationHandler");
    tp.setParseOptions(PO_NO_EXTERNAL_INFO);
    tp.parse("string sub uri() { return FileLocationHandler::AbstractFileLocationHandler::getFileUri(\"x\"); }",
        "vtoken");
    vtoken = (tp.callFunction("uri") =~ x/qore-cwd-([a-z0-9]+)-/)[0];
}
string requires = (map sprintf("%%requires %s\n", $1), Modules + ("AmbientCallers",)).join("");

SmtpRecorder smtp();
# the server's thread ends also if a check raises an exception
on_exit smtp.stop();

foreach hash<auto> mode in (Modes.pairIterator()) {
    foreach hash<auto> kind in (Kinds.pairIterator()) {
        string key = kind.key + "/" + mode.key;
        # the host creates the Program, preloads the modules into it and then restricts it, or creates it restricted
        # and the code requires the modules
        Program p(PO_MODERN | (kind.value.requires ? PO_NO_EXTERNAL_INFO : 0));
        p.setExecMode(mode.value);
        if (kind.value.sandboxed) {
            SandboxManager sm();
            sm.setNetworkDefaultPolicy(True);
            sm.setFilesystemDefaultPolicy(True);
            p.setSandboxManager(sm);
        }
        if (kind.value.requires) {
            p.parse(requires + child_source, "module-host-info-child");
        } else {
            map p.loadModule($1), Modules;
            # loaded by the worker (also into a qcc executable of the worker)
            p.loadModule("AmbientCallers");
            if (kind.value.restricted) {
                p.setParseOptions(PO_NO_EXTERNAL_INFO);
            }
            p.parse(child_source, "module-host-info-child");
        }
        p.setGlobalVarValue("q_hostname", hostname);
        p.setGlobalVarValue("q_smtp_port", smtp.port);
        p.setGlobalVarValue("q_lent", lent);
        p.setGlobalVarValue("q_server", host_server);
        p.setGlobalVarValue("q_layout", host_layout);
        p.setGlobalVarValue("q_logger", host_logger);
        p.setGlobalVarValue("q_logger2", host_logger2);
        p.setGlobalVarValue("q_reldir", ENV.Q7586_RELDIR);
        p.setGlobalVarValue("q_vtoken", vtoken);
        p.setGlobalVarValue("q_r", {});
        int log_size = host_log_stream.getData().size();
        p.run();
        hash<auto> r = p.getGlobalVariable("q_r");
        # the line that the host's appender wrote while the code logged through the host's logger
        r.lent_logger_line = trim(host_log_stream.getData().substr(log_size));
        # the effective location of a file resource: whether it names the virtual working directory and no
        # directory of the real one
        if (r.file_eff.typeCode() == NT_STRING && r.file_eff =~ /^file:/) {
            string eff = decode_url(r.file_eff);
            r.file_eff_virtual = eff =~ /\/qore-cwd-[a-z0-9]+-[0-9]+\// && eff.find(cwd) < 0;
            delete r.file_eff;
        }
        # the task IDs: whether they contain the PID (an error as it is)
        r.mcp_pid = r.mcp_id =~ /^task-/ ? r.mcp_id.find("-" + pid + "-") >= 0 : r.mcp_id;
        r.a2a_pid = r.a2a_id =~ /^task-/ ? r.a2a_id.find("-" + pid + "-") >= 0 : r.a2a_id;
        foreach string k in (("errpage", "lent_errpage")) {
            if (r{k}.typeCode() == NT_STRING && r{k} =~ /^<html>/) {
                r{k} = r{k}.find(" on " + hostname + "</address>") >= 0;
            }
        }
        r.smtp_ehlo = r.smtp == "connected" ? smtp.lines.get(30s) : NOTHING;
        r.smtp_async_ehlo = r.smtp_async == "connected" ? smtp.lines.get(30s) : NOTHING;
        hash<auto> expected = kind.value.restricted ? may_not_read : may_read;
        foreach string k in (keys expected) {
            check(key + "/" + k, expected{k}, r{k});
        }
        delete p;
    }
}

map printf("%s\n", $1), results;
