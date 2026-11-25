#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# run by alloc-failure.qtest: makes an allocation of a string's or a list's storage fail in an operation
# (dbg_fail_allocation(), debug builds only; an allocation of at least 64 KiB, which only the operation makes), and
# prints what the code got and that the process goes on; usage: alloc-failure-child.q <scenario>

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"

%requires HttpServerUtil
%requires HttpServer

%exec-class AllocFailureChild

#! makes the next allocation of a string's or a list's storage in this thread fail
sub fail_next_allocation() {
    call_function("dbg_fail_allocation", 1, 65536);
}

sub disarm() {
    call_function("dbg_fail_allocation", 0);
}

#! the request handler of the server scenario: "/fail" fails for lack of memory in builtin code
class FailHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        if (hdr.path == "fail") {
            try {
                fail_next_allocation();
                string str = strmul("x", 100000);
                disarm();
                return makeResponse(200, sprintf("no exception: %d", str.size()));
            } catch (hash<ExceptionInfo> ex) {
                disarm();
                return makeResponse(503, ex.err);
            }
        }
        if (hdr.path == "fail-uncaught") {
            fail_next_allocation();
            on_exit disarm();
            string str = strmul("x", 100000);
            return makeResponse(200, sprintf("no exception: %d", str.size()));
        }
        return makeResponse(200, "pong");
    }
}

#! grows a list with an operation of the function itself (no builtin code), in which the allocation fails
int sub grow_list(int n) {
    list<int> l = ();
    for (int i = 0; i < n; ++i) {
        l += i;
    }
    return l.size();
}

#! grows a string with an operation of the function itself
int sub grow_string(int n) {
    string str = "";
    for (int i = 0; i < n; ++i) {
        str += "x";
    }
    return str.size();
}

#! grows a list that a closure refers to
int sub grow_closure_local(int n) {
    list<int> l = ();
    code size = int sub () { return l.size(); };
    for (int i = 0; i < n; ++i) {
        l += i;
    }
    return size();
}

#! a function between the caller and the function whose allocation fails
int sub grow_nested(int n) {
    int rv = grow_list(n);
    return rv;
}

#! grows a list of floats with the operation that native code makes without a runtime helper that handles a failed
#! allocation (qore_rt_list_push_float_in_place_unchecked())
int sub grow_floats(int n) {
    list<float> l = ();
    for (int i = 0; i < n; ++i) {
        float f = i * 1.5;
        push l, f;
    }
    return l.size();
}

#! calls grow_floats() directly: native code makes the call without a runtime helper
int sub grow_floats_nested(int n) {
    int rv = grow_floats(n);
    return rv;
}

#! grows a list in a method
class Grower {
    int grow(int n) {
        list<int> l = ();
        for (int i = 0; i < n; ++i) {
            l += i;
        }
        return l.size();
    }
}

class AllocFailureChild {
    constructor() {
        switch (ARGV[0]) {
            case "compiled": compiled(); break;
            case "unhandled-direct": unhandled(False); break;
            case "unhandled-nested": unhandled(True); break;
            case "builtin": builtin(); break;
            case "callback": callback(); break;
            case "thread": thread(); break;
            case "server": server(); break;
            case "statement": statement(); break;
        }
    }

    #! builtin code (strmul()) that cannot allocate the string fails with a catchable exception
    static builtin() {
        try {
            fail_next_allocation();
            string s = strmul("x", 100000);
            printf("no exception: %d\n", s.size());
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        disarm();
        # the thread goes on
        printf("again: %d\n", strmul("x", 100000).size());
    }

    #! an operation whose own allocation fails (no builtin code is called) in a try block of the same function: the AST
    #! interpreter fails the statement, the other modes and compiled code the runtime helper of the operation
    static statement() {
        list<int> l = ();
        try {
            fail_next_allocation();
            for (int i = 0; i < 100000; ++i) {
                l += i;
            }
            printf("no exception: %d\n", l.size());
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        disarm();
        # the list that could not grow keeps its elements and grows again
        int n = l.size();
        bool ok = n > 0 && l[n - 1] == n - 1;
        l += n;
        printf("list: %y\n", ok && l.size() == n + 1 && l[n] == n);
        # the same for a string
        string str = "";
        try {
            fail_next_allocation();
            for (int i = 0; i < 100000; ++i) {
                str += "x";
            }
            printf("no exception: %d\n", str.size());
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        disarm();
        n = str.size();
        ok = n > 0 && str == strmul("x", n);
        str += "y";
        printf("string: %y\n", ok && str.size() == n + 1 && str[n] == "y");
        l = ();
        printf("again: %d\n", strmul("x", 100000).size());
    }

    #! an operation of a called function itself (no builtin code) that cannot allocate its memory fails the call; the
    #! caller catches the exception, and the function runs again; the calls are direct calls, not calls of closures
    static compiled() {
        Grower g();
        # the functions are called often enough to be compiled in the tiered mode
        for (int i = 0; i < 50; ++i) {
            grow_list(10);
            grow_string(10);
            grow_closure_local(10);
            grow_nested(10);
            g.grow(10);
        }
        # a local of the caller, which must keep its value
        string caller_local = "caller";
        try {
            fail_next_allocation();
            printf("list: no exception: %d\n", grow_list(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("list: caught %s\n", ex.err);
        }
        disarm();
        printf("list: again: %d\n", grow_list(100000));
        try {
            fail_next_allocation();
            printf("string: no exception: %d\n", grow_string(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("string: caught %s\n", ex.err);
        }
        disarm();
        printf("string: again: %d\n", grow_string(100000));
        try {
            fail_next_allocation();
            printf("closure-local: no exception: %d\n", grow_closure_local(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("closure-local: caught %s\n", ex.err);
        }
        disarm();
        printf("closure-local: again: %d\n", grow_closure_local(100000));
        try {
            fail_next_allocation();
            printf("nested: no exception: %d\n", grow_nested(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("nested: caught %s\n", ex.err);
        }
        disarm();
        printf("nested: again: %d\n", grow_nested(100000));
        try {
            fail_next_allocation();
            printf("method: no exception: %d\n", g.grow(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("method: caught %s\n", ex.err);
        }
        disarm();
        printf("method: again: %d\n", g.grow(100000));
        printf("caller local: %s\n", caller_local);
    }

    #! an allocation of native code that no runtime helper handles: the call fails with the exception if the
    #! function whose allocation failed was the only native frame left, or the process ends with the message of the
    #! terminate handler if native frames that called it directly were left too; never anything else
    static unhandled(bool nested) {
        for (int i = 0; i < 50; ++i) {
            nested ? grow_floats_nested(10) : grow_floats(10);
        }
        try {
            fail_next_allocation();
            printf("no exception: %d\n", nested ? grow_floats_nested(100000) : grow_floats(100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        disarm();
        printf("again: %d\n", nested ? grow_floats_nested(100000) : grow_floats(100000));
    }

    #! builtin code that calls Qore code (call_function()) whose builtin call fails
    static callback() {
        code c = int sub (int n) {
            fail_next_allocation();
            on_exit disarm();
            return strmul("x", n).size();
        };
        try {
            printf("no exception: %d\n", call_function(c, 100000));
        } catch (hash<ExceptionInfo> ex) {
            printf("caught %s\n", ex.err);
        }
        printf("again: %d\n", call_function(c, 10));
    }

    #! a background thread whose builtin call fails ends with the exception; the process goes on
    static thread() {
        Counter cnt(1);
        background sub () {
            on_exit cnt.dec();
            try {
                fail_next_allocation();
                string s = strmul("x", 100000);
                printf("no exception: %d\n", s.size());
            } catch (hash<ExceptionInfo> ex) {
                printf("caught %s\n", ex.err);
            }
            disarm();
        }();
        cnt.waitForZero();
        printf("again: %d\n", strmul("x", 100000).size());
    }

    #! a server keeps serving after a request fails for lack of memory
    static server() {
        HttpServer srv(<HttpServerOptionInfo>{});
        on_exit srv.stop();
        srv.setDefaultHandler("fail", new FailHandler());
        int port = srv.addListener(<HttpListenerOptionInfo>{"service": 0, "node": "127.0.0.1"}).port;
        string url = sprintf("http://127.0.0.1:%d/", port);
        foreach string path in ("ping", "fail", "ping", "fail-uncaught", "ping") {
            hash<auto> info;
            try {
                HTTPClient hc({"url": url});
                hc.get(path, NOTHING, \info);
            } catch (hash<ExceptionInfo> ex) {
            }
            *string body = info."response-body";
            printf("%s: %d %s\n", path, info."response-headers".status_code,
                body =~ /MEMORY-ALLOCATION-ERROR/ ? "MEMORY-ALLOCATION-ERROR" : body);
        }
    }
}
