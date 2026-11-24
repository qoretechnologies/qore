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

class AllocFailureChild {
    constructor() {
        switch (ARGV[0]) {
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

    #! a statement whose own allocation fails (no builtin code is called): the AST interpreter fails the statement
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
