# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The probes of short-string-access.qtest: each passes strings of 6 bytes or fewer, which a value can hold inline
# (with no QoreStringNode), to the logger_bin module, and prints what the module made of them.  A qcc-compiled
# executable holds the strings of a constant hash inline; in a debug build ss() makes them inline in every execution
# mode (dbg_make_short_string()).

%modern
%requires logger_bin

#! the strings of the probes: a qcc-compiled executable holds the strings of a constant hash inline
const Short = {
    "a.log": "a.log",
};

#! returns the string: inline in a debug build, and from the constant hash otherwise
string sub ss(string s) {
%ifdef QoreDebug
    return dbg_make_short_string(s);
%else
    return Short{s};
%endif
}

#! a file appender whose file name is assigned by Qore code, as the rotating appenders of the Logger module do
class ShortNameAppender inherits Qore::Logger::LoggerAppenderFile {
    constructor(string path) : Qore::Logger::LoggerAppenderFile("T", new Qore::Logger::LoggerLayoutPattern("%m%n"),
            path) {
        fileName = ss("a.log");
    }
}

#! the probes in the order that they run
const Probes = (
    # the file name member of a file appender, opened in the current directory (a temporary directory of the test)
    "LoggerAppenderFile::openFile",
);

#! runs a probe and returns its result
string sub run_probe(string name) {
    switch (name) {
        case "LoggerAppenderFile::openFile": {
            ShortNameAppender a("unused.log");
            a.open();
            a.close();
            return sprintf("%s %y", a.getFileName(), is_file("a.log"));
        }
    }
    throw "PROBE-ERROR", sprintf("unknown probe %y", name);
}

foreach string name in (ARGV ?: Probes) {
    try {
        printf("%s: %s\n", name, run_probe(name));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}
