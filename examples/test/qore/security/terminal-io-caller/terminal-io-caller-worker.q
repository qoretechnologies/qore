#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of terminal-io-caller.qtest (qore#7140): library code writes to the terminal for its caller only if
# neither its own options nor the calling code restrict terminal I/O; prints one "<case>=ok" line per check (or
# "<case>=expected <x>, got <y>"), and nothing else: any output that the library code writes for a restricted caller
# makes the output differ

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}"
%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
# loaded by the worker, so that the restricted Programs import them (with the worker's options)
%requires TermCheck
%requires DatasourceProvider
%requires ConnectionProvider
%requires TelnetClient
%requires Logger
%requires DataProvider
%requires TerminalFormat

list<string> results = ();

sub check(string name, auto expected, auto actual) {
    push results, name + "=" + (expected === actual ? "ok" : sprintf("expected %y, got %y", expected, actual));
}

const Restricted = PO_MODERN | PO_NO_TERMINAL_IO;

#! a Program with the given options that imports the modules and runs the probes
Program sub make_program(int po, *SandboxManager sm) {
    Program p(po);
    if (sm) {
        p.setSandboxManager(sm);
    }
    map p.loadModule($1), ("TermCheck", "DatasourceProvider", "ConnectionProvider", "TelnetClient", "Logger",
        "DataProvider", "TerminalFormat");
    p.parse("bool sub direct() { return SandboxManager::isTerminalIoAllowed(); }
bool sub via_module() { return TermCheck::allowed(); }
*string sub ds() { try { return DatasourceProvider::get_ds_string(\"qore-7140-no-such-ds\", True); } "
        "catch (hash<ExceptionInfo> ex) { return ex.err; } }
*hash<auto> sub connections() { return ConnectionProvider::get_connections(True); }
class Term inherits TelnetClient {
    constructor() : TelnetClient(\"localhost:1\") {}
    hash<auto> size() { return getWindowSize(); }
}
hash<auto> sub window_size() { return (new Term()).size(); }
string sub use(string how) { return TermCheck::use(how); }
# objects of the standard modules that write to the terminal, created by this code
bool sub own_writers() {
    Logger logger(\"restricted\", LoggerLevel::getLevelInfo());
    LoggerAppenderStdOut out(\"out\", new LoggerLayoutPattern(\"%m%n\"));
    out.open();
    logger.addAppender(out);
    LoggerAppenderStdErr err(\"err\", new LoggerLayoutPattern(\"%m%n\"));
    err.open();
    logger.addAppender(err);
    logger.addAppender(new StdoutAppender());
    logger.info(\"restricted logger line\");
    ConsoleDebugInfoCollector collector();
    collector.collect(<DataProviderDebugInfo>{\"operation\": \"restricted collector line\", \"start_time\": now_us()});
    TerminalProgress progress(<TerminalProgressOptions>{\"interactive\": False, \"columns\": 80});
    progress.finish(progress.add(\"restricted progress line\", 10), \"done\");
    # what the writers decided by
    return SandboxManager::isTerminalIoAllowed();
}
# a logger that the host created and gave this code
bool sub host_logger(Logger logger) { logger.info(\"host logger line for restricted code\"); return logger.isEnabledFor(LoggerLevel::getLevelInfo()); }
", "probe");
    return p;
}

# the test runs the worker with QORE_DATASOURCE_PROVIDERS and QORE_CONNECTION_PROVIDERS naming a module that cannot be
# loaded, whose errors the provider modules write to stdout in verbose mode (a module reads its environment when it is
# loaded, so they are set before the worker starts)

# the worker itself may write to the terminal
check("host", True, SandboxManager::isTerminalIoAllowed());
check("host-module", True, TermCheck::allowed());

# code restricted from terminal I/O, and library code that serves it, may not
{
    Program p = make_program(Restricted);
    check("restricted-direct", False, p.callFunction("direct"));
    check("restricted-module", False, p.callFunction("via_module"));
    # nothing is written for the providers that cannot be loaded
    check("restricted-ds", "DATASOURCE-ERROR", p.callFunction("ds"));
    check("restricted-connections", {}, p.callFunction("connections"));
    # the window size is not read from the terminal
    check("restricted-window-size", {"rows": 24, "columns": 80}, p.callFunction("window_size"));
    # the builtins that use the terminal refuse the caller also in library code
    foreach string how in (("printf", "print", "stdout", "stderr", "stream", "streamwriter", "window")) {
        check("restricted-builtin-" + how, "ILLEGAL-EXPRESSION", p.callFunction("use", how));
    }
    # GetOpt raises the error that it would write to stderr before it exits
    check("restricted-getopt", "GETOPT-ERROR", p.callFunction("use", "getopt"));
    # the writers of the standard modules that the restricted code creates write nothing
    check("restricted-own-writers", False, p.callFunction("own_writers"));
    # a logger that the host created writes its terminal lines for the host, not while it serves the restricted code
    Logger logger("host", LoggerLevel::getLevelInfo());
    LoggerAppenderStdOut out("out", new LoggerLayoutPattern("%m%n"));
    out.open();
    logger.addAppender(out);
    logger.info("host logger line");
    # the logger is enabled for the line, which is still not written
    check("restricted-host-logger", True, p.callFunction("host_logger", logger));
    # the same with a stream appender writing to a stream to the terminal: the line is left out, no exception
    Logger stream_logger("hoststream", LoggerLevel::getLevelInfo());
    LoggerAppenderStream stream_appender("s", new LoggerLayoutPattern("%m%n"),
        new StreamWriter(new StdoutOutputStream()));
    stream_appender.open();
    stream_logger.addAppender(stream_appender);
    stream_logger.info("host stream logger line");
    check("restricted-host-stream-logger", True, p.callFunction("host_logger", stream_logger));
}

# a sandboxed caller that is not restricted from terminal I/O may
{
    Program p = make_program(PO_MODERN, new SandboxManager());
    check("sandboxed-direct", True, p.callFunction("direct"));
    check("sandboxed-module", True, p.callFunction("via_module"));
}

map printf("%s\n", $1), results;
