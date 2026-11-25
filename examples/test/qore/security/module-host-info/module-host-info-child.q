# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of the Programs of module-host-info-worker.q (qore#7586): the worker preloads the modules into each Program
# before it restricts it, sets the q_* variables and runs it; the top-level code calls the functions of the preloaded
# modules that read host information and stores what it gets (or the code of the error) in q_r

# set by the worker before it runs the Program
our *string q_hostname;
our *int q_smtp_port;
our *object q_lent;
our *object q_server;
our *object q_layout;
our *object q_logger;
our *object q_logger2;
our *string q_reldir;
our *string q_vtoken;
our hash<auto> q_r;

# subclasses that reach the protected methods of the modules' classes
class Q7586Telnet inherits TelnetClient::TelnetClient {
    constructor() : TelnetClient("localhost:1") {
    }

    hash<auto> env() {
        return getEnvironment();
    }

    *string term() {
        return getTerminalType();
    }

    *string display() {
        return getDisplay();
    }
}

class Q7586Es inherits ElasticSearchLoggerAppender::ElasticSearchLoggerAppender {
    constructor() : ElasticSearchLoggerAppender("es", new Logger::LoggerLayoutPattern(),
            {"url": "http://127.0.0.1:1", "index": "q7586"}) {
    }

    hash<auto> doc(Logger::LoggerEvent ev) {
        return createDocument(ev);
    }
}

class Q7586Os inherits OpenSearchLoggerAppender::OpenSearchLoggerAppender {
    constructor() : OpenSearchLoggerAppender("os", new Logger::LoggerLayoutPattern(),
            {"url": "http://127.0.0.1:1", "index": "q7586"}) {
    }

    hash<auto> doc(Logger::LoggerEvent ev) {
        return createDocument(ev);
    }
}

# an appender of the code's own that takes a layout of the host
class Q7586OwnAppender inherits Logger::LoggerAppenderWithLayout {
    public {
        list<auto> got = ();
    }

    constructor(Logger::LoggerLayout l) : LoggerAppenderWithLayout("own", l) {
        open();
    }

    processEventImpl(int type, auto params) {
        if (type == EVENT_LOG) {
            got += params;
        }
    }
}

class Q7586Mcp inherits McpServerHandler::McpServerHandler {
    constructor() : McpServerHandler(new HttpServer::PermissiveAuthenticator()) {
    }

    string id() {
        return generateTaskId();
    }
}

class Q7586A2a inherits A2aServerHandler::A2aServerHandler {
    constructor() : A2aServerHandler(new Logger::Logger("a2a"), {"name": "q7586"}) {
    }

    string id() {
        return generateTaskId();
    }
}

# Util
try { q_r.tmp = Util::tmp_location(); } catch (hash<ExceptionInfo> ex) { q_r.tmp = ex.err; }
try { q_r.tmpdir = dirname((new FsUtil::TmpDir("q7586-")).path); } catch (hash<ExceptionInfo> ex) { q_r.tmpdir = ex.err; }
try {
    hash<FsUtil::TmpFileHash> f = FsUtil::make_tmp_file("q7586-");
    unlink(f.path);
    q_r.tmpfile = dirname(f.path);
} catch (hash<ExceptionInfo> ex) {
    q_r.tmpfile = ex.err;
}
try { q_r.norm = Util::normalize_dir("a/../b/./c"); } catch (hash<ExceptionInfo> ex) { q_r.norm = ex.err; }
try { q_r.norm_up = Util::normalize_dir_unix("../x/./y//z/.."); } catch (hash<ExceptionInfo> ex) { q_r.norm_up = ex.err; }
try { q_r.norm_corner = map Util::normalize_dir($1), ("", "./", "a/..", "../../a/../b"); } catch (hash<ExceptionInfo> ex) { q_r.norm_corner = ex.err; }
try { q_r.norm_relcwd = (Util::normalize_dir("x", "."), Util::normalize_dir("x", "d"), Util::normalize_dir("../e", ".")); } catch (hash<ExceptionInfo> ex) { q_r.norm_relcwd = ex.err; }
try { q_r.norm_win = Util::normalize_dir_windows("a\\..\\b"); } catch (hash<ExceptionInfo> ex) { q_r.norm_win = ex.err; }
try { q_r.norm_abs = Util::normalize_dir("/q7586/a/../b"); } catch (hash<ExceptionInfo> ex) { q_r.norm_abs = ex.err; }
try { q_r.norm_explicit = Util::normalize_dir("b", "/q7586"); } catch (hash<ExceptionInfo> ex) { q_r.norm_explicit = ex.err; }
try { q_r.rel = Util::get_relative_path("a", "a/b/f"); } catch (hash<ExceptionInfo> ex) { q_r.rel = ex.err; }
try { q_r.rel_abs = Util::get_relative_path("/q7586/a", "/q7586/b/f"); } catch (hash<ExceptionInfo> ex) { q_r.rel_abs = ex.err; }
try { q_r.rel_dot = Util::get_relative_path(".", "../x"); } catch (hash<ExceptionInfo> ex) { q_r.rel_dot = ex.err; }
try { q_r.rel_dotdir = Util::get_relative_path("x/..", "../y"); } catch (hash<ExceptionInfo> ex) { q_r.rel_dotdir = ex.err; }
try { q_r.rel_sib = Util::get_relative_path("a", "x"); } catch (hash<ExceptionInfo> ex) { q_r.rel_sib = ex.err; }
try { q_r.rel_up = Util::get_relative_path("..", "x"); } catch (hash<ExceptionInfo> ex) { q_r.rel_up = ex.err; }
try { q_r.rel_up2 = Util::get_relative_path("../a", "b"); } catch (hash<ExceptionInfo> ex) { q_r.rel_up2 = ex.err; }
try { q_r.rel_mixed = Util::get_relative_path("/q7586", "y"); } catch (hash<ExceptionInfo> ex) { q_r.rel_mixed = ex.err; }
try { q_r.nocolor = Util::TerminalColor::isColorDisabled(); } catch (hash<ExceptionInfo> ex) { q_r.nocolor = ex.err; }

# HttpServerUtil and HttpServer
try { q_r.url_wild = HttpServer::http_get_url_from_bind("0.0.0.0:8080"); } catch (hash<ExceptionInfo> ex) { q_r.url_wild = ex.err; }
try { q_r.url_wild6 = HttpServer::http_get_url_from_bind("[::]:8080"); } catch (hash<ExceptionInfo> ex) { q_r.url_wild6 = ex.err; }
try { q_r.url_host = HttpServer::http_get_url_from_bind("8080", q_hostname); } catch (hash<ExceptionInfo> ex) { q_r.url_host = ex.err; }
try { q_r.errpage = (new HttpServer::HttpServer()).getHtmlErrorMessage(404, "q7586"); } catch (hash<ExceptionInfo> ex) { q_r.errpage = ex.err; }

# Logger (the binary module's layout) and the search appenders
try {
    Logger::LoggerLayoutPattern layout("%h|%P|%E{Q7586_SECRET}");
    q_r.layout = layout.format(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(), "m"));
} catch (hash<ExceptionInfo> ex) {
    q_r.layout = ex.err;
}
try {
    q_r.es_host = (new Q7586Es()).doc(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(),
        "m")).host;
} catch (hash<ExceptionInfo> ex) {
    q_r.es_host = ex.err;
}
try {
    q_r.os_host = (new Q7586Os()).doc(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(),
        "m")).host;
} catch (hash<ExceptionInfo> ex) {
    q_r.os_host = ex.err;
}

# SmtpClient: the worker's server records the EHLO command
try {
    SmtpClient::SmtpClient smtp(sprintf("127.0.0.1:%d", q_smtp_port));
    smtp.connect();
    smtp.disconnect();
    q_r.smtp = "connected";
} catch (hash<ExceptionInfo> ex) {
    q_r.smtp = ex.err + ": " + ex.desc;
}

# TelnetClient
try {
    Q7586Telnet telnet();
    q_r.telnet_env = telnet.env();
    q_r.telnet_term = telnet.term();
    q_r.telnet_display = telnet.display();
} catch (hash<ExceptionInfo> ex) {
    q_r.telnet_env = ex.err;
}

# QUnit
try { q_r.qunit_env = (new QUnit::Test("q7586", "1.0", ())).getEnv("Q7586"); } catch (hash<ExceptionInfo> ex) { q_r.qunit_env = ex.err; }

# EdifactUtil
try { q_r.edifact = (new EdifactUtil::EdifactDirectoryCache()).getCacheRoot(); } catch (hash<ExceptionInfo> ex) { q_r.edifact = ex.err; }
try { q_r.edifact_tilde = (new EdifactUtil::EdifactDirectoryCache("~/q7586")).getCacheRoot(); } catch (hash<ExceptionInfo> ex) { q_r.edifact_tilde = ex.err; }
try { q_r.edifact_explicit = (new EdifactUtil::EdifactDirectoryCache("/q7586/edifact")).getCacheRoot(); } catch (hash<ExceptionInfo> ex) { q_r.edifact_explicit = ex.err; }

# ConnectionProvider and DatasourceProvider
try { q_r.conns = keys ConnectionProvider::get_connections(); } catch (hash<ExceptionInfo> ex) { q_r.conns = ex.err; }
try { q_r.conn = ConnectionProvider::get_connection("q7586").url; } catch (hash<ExceptionInfo> ex) { q_r.conn = ex.err; }
try { q_r.conn_hash = keys ConnectionProvider::get_connection_hash(); } catch (hash<ExceptionInfo> ex) { q_r.conn_hash = ex.err; }
try { q_r.ds_hash = DatasourceProvider::get_ds_hash(); } catch (hash<ExceptionInfo> ex) { q_r.ds_hash = ex.err; }
try { q_r.ds_string = DatasourceProvider::get_ds_string("q7586"); } catch (hash<ExceptionInfo> ex) { q_r.ds_string = ex.err; }

# the task IDs of the MCP and A2A server handlers
try { q_r.mcp_id = (new Q7586Mcp()).id(); } catch (hash<ExceptionInfo> ex) { q_r.mcp_id = ex.err + ": " + ex.desc; }
try { q_r.a2a_id = (new Q7586A2a()).id(); } catch (hash<ExceptionInfo> ex) { q_r.a2a_id = ex.err; }

# QoreRepl
try {
    *hash<auto> h = (new QoreRepl::QoreRepl({"history_file": NOTHING})).eval("/pwd");
    q_r.repl_pwd = h.error ? h.error.err : h.value;
} catch (hash<ExceptionInfo> ex) {
    q_r.repl_pwd = ex.err;
}

# QoreRepl: the reply of /cd (to the current directory, which is not changed)
try {
    *hash<auto> h = (new QoreRepl::QoreRepl({"history_file": NOTHING})).eval("/cd .");
    q_r.repl_cd = h.error ? h.error.err : h.value;
} catch (hash<ExceptionInfo> ex) {
    q_r.repl_cd = ex.err;
}

# objects of the modules' classes that the host created and lent: the error page of a server of the host gives no
# hostname to such code; a layout of the host keeps its fields (decided when it was created)
try { q_r.lent_errpage = q_server.getHtmlErrorMessage(404, "q7586"); } catch (hash<ExceptionInfo> ex) { q_r.lent_errpage = ex.err; }
try {
    q_r.lent_layout = q_layout.format(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(),
        "m"));
} catch (hash<ExceptionInfo> ex) {
    q_r.lent_layout = ex.err;
}

# loggers of the host: the host's appender formats its fields for the line that the code logs; a layout of the host
# whose pattern the code sets formats no host values for it
try { q_logger.info("m"); } catch (hash<ExceptionInfo> ex) { q_r.lent_logger_error = ex.err; }
try {
    Logger::LoggerLayoutPattern l = cast<Logger::LoggerLayoutPattern>(
        cast<Logger::LoggerAppenderWithLayout>(q_logger2.getAppenders()[0]).getLayout());
    l.setPattern("%E{Q7586_SECRET}|%h|%P");
    q_r.lent_layout_set = l.format(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(),
        "m"));
} catch (hash<ExceptionInfo> ex) {
    q_r.lent_layout_set = ex.err;
}

# an appender of the code's own with the layout of a host appender gets no host values from it
try {
    Q7586OwnAppender own(cast<Logger::LoggerAppenderWithLayout>(q_logger.getAppenders()[0]).getLayout());
    own.post(new Logger::LoggerEvent("q7586", "q7586", Logger::LoggerLevel::getLevelInfo(), "m"));
    own.close();
    q_r.own_appender = trim(own.got.join(""));
} catch (hash<ExceptionInfo> ex) {
    q_r.own_appender = ex.err + ": " + ex.desc;
}

# the asynchronous SMTP connection (a poll operation run by the async I/O controller)
try {
    SmtpClient::SmtpConnection conn("q7586", "", sprintf("smtp://127.0.0.1:%d", q_smtp_port));
    AbstractPollOperation op = conn.startPollConnect();
    *hash<SocketPollInfo> pi = op.continuePoll();
    if (pi) {
        hash<SocketPollResultInfo> res = AsyncIoController::exec(operation_info: <SocketPollOperationInfo>{
            "sock": pi.socket,
            "spop": op,
            "owner": "q7586",
            "to": 30s,
        });
        if (res.ex) {
            throw res.ex.err, res.ex.desc;
        }
    }
    q_r.smtp_async = op.goalReached() ? "connected" : op.getState();
} catch (hash<ExceptionInfo> ex) {
    q_r.smtp_async = ex.err + ": " + ex.desc;
}

# QLS
try { q_r.qls_expand = QLS::DocumentLinks::expand("${Q7586_SECRET}/$Q7586_SECRET/${SCRIPT_DIR}", "d"); } catch (hash<ExceptionInfo> ex) { q_r.qls_expand = ex.err; }
try { q_r.qls_moddirs = QLS::DocumentLinks::getEnvModuleDirs(); } catch (hash<ExceptionInfo> ex) { q_r.qls_moddirs = ex.err; }

# DataProvider: the failures of the modules of QORE_DATA_PROVIDERS
try { q_r.dp_failures = map $1.module, DataProvider::DataProvider::loadProvidersFromEnvironmentWithReport(); } catch (hash<ExceptionInfo> ex) { q_r.dp_failures = ex.err; }

# QoreOnnxTools: the Python executable of the environment
try { q_r.onnx_python = QoreOnnxTools::OnnxQuantizer::checkPythonDependencies().python; } catch (hash<ExceptionInfo> ex) { q_r.onnx_python = ex.err; }

# FileLocationHandler: a relative file location, and references resolved against its effective location
try {
    hash<auto> res = FileLocationHandler::FileLocationHandler::getResourceFromLocation("file://" + q_reldir
        + "/schema/a.yaml");
    q_r.file_eff = res.info.effective_location;
    q_r.file_resolved = ReadOnlyFile::readTextFile(FileLocationHandler::AbstractFileLocationHandler::getPathFromFileUri(
        resolve_url(res.info.effective_location, "b.yaml")));
    q_r.file_resolved_up = ReadOnlyFile::readTextFile(
        FileLocationHandler::AbstractFileLocationHandler::getPathFromFileUri(
        resolve_url(res.info.effective_location, "../schema/b.yaml")));
} catch (hash<ExceptionInfo> ex) {
    q_r.file_eff = ex.err + ": " + ex.desc;
}

# URIs of the virtual working directory that the code crafts: a level that getFileUri() does not create is no
# virtual working directory (and costs nothing), one that it does maps to the parents of the working directory
try {
    string t = q_vtoken ?? "none";
    date start = now_us();
    q_r.vcwd_big = FileLocationHandler::AbstractFileLocationHandler::getPathFromFileUri("file:///qore-cwd-" + t
        + "-10000000/x") == "/qore-cwd-" + t + "-10000000/x" && (now_us() - start) < 10s;
    q_r.vcwd_up = FileLocationHandler::AbstractFileLocationHandler::getPathFromFileUri("file:///qore-cwd-" + t
        + "-3/etc/passwd");
} catch (hash<ExceptionInfo> ex) {
    q_r.vcwd_big = ex.err + ": " + ex.desc;
}

# the host's grants: a policy barrier and a system-policy capability (the host's values for every caller)
try { q_r.grant_barrier = AmbientCallers::invoke_barrier(NOTHING, "Util::tmp_location"); } catch (hash<ExceptionInfo> ex) { q_r.grant_barrier = ex.err; }
try { q_r.grant_capability = AmbientCallers::invoke_capability(NOTHING, "Util::tmp_location"); } catch (hash<ExceptionInfo> ex) { q_r.grant_capability = ex.err; }

# an object that the host lent: it lends nothing here
q_r.lent_tmp = q_lent.direct(NOTHING, "Util::tmp_location");
q_r.lent_url = q_lent.direct(NOTHING, "HttpServer::http_get_url_from_bind", "0.0.0.0:8080");
