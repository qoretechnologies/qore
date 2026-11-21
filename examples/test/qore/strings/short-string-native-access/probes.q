# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The probes of short-string-native-access.qtest: each passes strings of 6 bytes or fewer, which a value can hold
# inline (with no QoreStringNode), to native code that reads string values out of exceptions and hashes, and prints
# what the code made of them.  A qcc-compiled executable holds the strings of a constant hash inline; in a debug
# build ss() makes them inline in every execution mode (dbg_make_short_string()).  With an argument, only the probe of that name
# runs.

%modern

#! the strings of the probes: a qcc-compiled executable holds the strings of a constant hash inline
const Short = {
    "E1": "E1",
    "d1": "d1",
    "E2": "E2",
    "d2": "d2",
    "b": "b",
    "/x": "/x",
    "ab": "ab",
};

#! returns the string: inline in a debug build, and from the constant hash otherwise
string sub ss(string s) {
%ifdef QoreDebug
    return dbg_make_short_string(s);
%else
    return Short{s};
%endif
}

#! raises an exception with an inline error code and description
string sub throw_short() {
    throw ss("E1"), ss("d1");
}

#! the probes in the order that they run
const Probes = (
    # a background thread that ends with an exception with an inline error code: the thread's outcome is taken from
    # the error code (ProgramThreadHub)
    "background",
    # header values in hashes
    "FastResponseSlot::setDefaults",
    "Socket::setHttpFastExtraHeaders",
    # option values in a hash
    "HTTPClient ssl_ca_file",
    "HTTPClient ssl_ca_path",
    # the values of a string thrown by Qore code are its own
    "caught",
%ifdef QoreDebug
    # the debug hooks that take a string (debug builds only)
    "dbg_string_is_view",
    "dbg_string_capacity",
%endif
);

#! runs a probe and returns its result
string sub run_probe(string name) {
    switch (name) {
        case "background": {
            Counter c(1);
            background sub () {
                on_exit c.dec();
                throw ss("E2"), ss("d2");
            }();
            c.waitForZero();
            # the thread's outcome is taken after its code has returned: wait until the thread is gone
            while (num_threads() > 1) {
                usleep(1ms);
            }
            return "ended";
        }
        case "FastResponseSlot::setDefaults": {
            FastResponseSlot slot();
            slot.setDefaults({"X-A": ss("b")});
            return "ok";
        }
        case "Socket::setHttpFastExtraHeaders": {
            Socket s();
            s.setHttpFastExtraHeaders({"X-A": ss("b")});
            return "ok";
        }
        case "HTTPClient ssl_ca_file": {
            HTTPClient hc({"url": "http://localhost:1", "ssl_ca_file": ss("/x")});
            return "ok";
        }
        case "HTTPClient ssl_ca_path": {
            HTTPClient hc({"url": "http://localhost:1", "ssl_ca_path": ss("/x")});
            return "ok";
        }
        case "caught": {
            try {
                throw_short();
            } catch (hash<ExceptionInfo> ex) {
                return ex.err + "/" + ex.desc;
            }
            return "no exception";
        }
%ifdef QoreDebug
        case "dbg_string_is_view":
            return sprintf("%y", dbg_string_is_view(ss("ab")));
        case "dbg_string_capacity":
            return sprintf("%y", dbg_string_capacity(ss("ab")));
%endif
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
