#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*
    Copyright (C) 2026 Qore Technologies, s.r.o., all rights reserved

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    The HTTP server of HttpServerRecvGrowFailure.qtest: it writes the port of its listener and the size of its address
    space to its standard output, returns the error of a read of a request body whose memory could not grow that it
    logged for "GET /error", and ends when it gets "GET /quit".  Its argument "measure" makes it write the same line
    and end.
*/

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires Logger
%requires HttpServerUtil
%requires HttpServer

#! keeps the error of the read of a request body that the server logs
class BodyErrorCapture inherits LoggerAppenderWithLayout {
    private {
        #! the error of the last read of a request body, or "none"
        string error = "none";
        Mutex lck();
    }

    constructor() : LoggerAppenderWithLayout("capture", new LoggerLayoutPattern("%m")) {
        open();
    }

    processEventImpl(int type, auto params) {
        if (type == EVENT_LOG) {
            # the error of a body read that the server logs
            *string err = (params =~ x/(SOCKET-RECV-ERROR): the memory for /)[0];
            if (err) {
                AutoLock al(lck);
                error = err;
            }
        }
    }

    #! returns the error logged since the last call, or "none"
    string takeError() {
        AutoLock al(lck);
        on_exit error = "none";
        return error;
    }
}

#! the handler of the requests; the server reads a request body before the handler is called
class GrowFailureHandler inherits AbstractHttpRequestHandler {
    public {
        #! decremented by "GET /quit"
        Counter quit(1);
    }

    private {
        BodyErrorCapture capture;
    }

    constructor(BodyErrorCapture capture) {
        self.capture = capture;
    }

    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        switch (hdr.path) {
            case "string":
                # the handler reads the chunked body as a string
                s.readHTTPChunkedBody(30s);
                break;

            case "error":
                return makeResponse(200, capture.takeError(), {"Content-Type": "text/plain"});

            case "quit":
                quit.dec();
                break;
        }
        return makeResponse(200, "served " + hdr.path, {"Content-Type": "text/plain"});
    }
}

#! returns the size of the address space of the process in kilobytes
int sub vm_size() {
    return (ReadOnlyFile::readTextFile("/proc/self/status") =~ x/VmSize:\s+([0-9]+)/)[0].toInt();
}

BodyErrorCapture capture();
GrowFailureHandler handler(capture);
Logger logger("test", LoggerLevel::getLevelError());
logger.addAppender(capture);
HttpServer server(<HttpServerOptionInfo>{
    "logger": logger,
    # no limit, so that the buffer of a body read grows until no more memory can be allocated for it, and not only
    # up to the default limit of a body that the server reads into memory
    "max_request_body_size": 0,
});
server.setDefaultHandler("grow", handler);
int port = server.addListener(<HttpListenerOptionInfo>{"service": 0, "node": "127.0.0.1"}).port;
server.waitForAsyncIo();
printf("port %d vm %d\n", port, vm_size());
flush();
if (ARGV[0] != "measure") {
    handler.quit.waitForZero();
}
delete server;
