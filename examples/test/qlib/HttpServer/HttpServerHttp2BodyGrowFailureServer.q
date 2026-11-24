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

    The HTTP server of HttpServerHttp2BodyGrowFailure.qtest: it writes the ports of its HTTP/1.1 and HTTP/2 (TLS)
    listeners and the size of its address space to its standard output, answers a request with the size of the body
    that it was given, returns the error of a request that it logged for "GET /error", and ends when it gets
    "GET /quit".  Its argument "measure" makes it write the same line and end.
*/

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires Logger
%requires HttpServerUtil
%requires HttpServer

#! the test certificate and its key
const CertPem = "-----BEGIN CERTIFICATE-----
MIIDCTCCAfGgAwIBAgIUahuuRGDlubXWYmiPhjVeT66vtNYwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI2MDIyMjE5MjIwMVoXDTM2MDIy
MDE5MjIwMVowFDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAjZNfLFUyyo02MjZjouaR11FybkZNwDWBG/bhkpTVyj/x
QKlwKf20LbGSCmMIYHaFnODMg6u6E6sfRJVk+ofwVHBN/Ug+mvoL1zlNJanMdXLH
7oUsUILY6bVUDPm1ACZmFJsAc9jfHfkluJ6vfYG+DOJbyB0nsMfO9/fvrwndQPUQ
k9eMq2VfDohni4ijk5BWD/P9UmrciZUNOaNISay469Le5ZrtfGt9An1+Ye3TckJC
tTF4q7+V+GC88w5QtLd1AMQGf/1ax5cwqQv9SSFvhnwj9o3V3Wkz1dq4XA/9rSA1
ecPHh/tJeGuvbD11Hyr9LTYjQ+DIel0DV0ZDHdc5vwIDAQABo1MwUTAdBgNVHQ4E
FgQUYBBsVylVinbbAzTbUt0jRM6mJvwwHwYDVR0jBBgwFoAUYBBsVylVinbbAzTb
Ut0jRM6mJvwwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAfIAE
EPvLO+gOnm366xJabLr2Sw3hfmvCq/zSTlPcrB9jdBlOLpMmKcdGiOOCNmzg9+BX
YNX+I6+MNOGp0j3afWlVwEjiMD66ydSMPEQw0FTzvpIjFN3fHo/MJzFbingRLrk8
LCo94GmjMBYG+50M8MCKE6EzqfmoYT9fiMJcyJwwdptOWf0URNYKKrG1wRlX4+o8
uNjQ3VwynI95fAw9WcR/ZtP0pkmCor/hKaYgVRsSGjD78Fu0Uuhm/pCAOuObpEHj
5eoTlaWC3pZzGA67piqlEDOWiRi9Mo2CzG0t5fmQ83KB51ImauKdYB21whxjkZ+5
BjwMC4hqSD+bzO2B+A==
-----END CERTIFICATE-----";

const KeyPem = "-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCNk18sVTLKjTYy
NmOi5pHXUXJuRk3ANYEb9uGSlNXKP/FAqXAp/bQtsZIKYwhgdoWc4MyDq7oTqx9E
lWT6h/BUcE39SD6a+gvXOU0lqcx1csfuhSxQgtjptVQM+bUAJmYUmwBz2N8d+SW4
nq99gb4M4lvIHSewx8739++vCd1A9RCT14yrZV8OiGeLiKOTkFYP8/1SatyJlQ05
o0hJrLjr0t7lmu18a30CfX5h7dNyQkK1MXirv5X4YLzzDlC0t3UAxAZ//VrHlzCp
C/1JIW+GfCP2jdXdaTPV2rhcD/2tIDV5w8eH+0l4a69sPXUfKv0tNiND4Mh6XQNX
RkMd1zm/AgMBAAECggEAATo1WYTgPK75DiS4PBR5UDS7DryvoN/LNOsGLUdQC9b+
aEaacSrAvKKs9vJr2KuTtfahn0SbsjRKFZtL9aYRuHxZbhLfGWiF4MGflcOBQRq+
eVFl6clwnaS+S/cq5rZD1/WODUhkisslqhI/cubvNwe2rvyduE510LgxYkLuIZ7a
+SCQz1XyPyhb8eVrWIZiprufjqXvDy01MC74l3ZYqaq1hr1oKi0ZDHI95Os7gtwQ
4HQ3qdJsmYfbaf1223REp8gybnshsxkyjhKRU3nG7ZvgrRRMFkJvfHZ8VcS3cvsK
ccKBJg7376+VDtc+RHUcBMyVZ881y4Rq2t6ssYVJVQKBgQDFq+N7ZGKOtQChSIXn
ju/gP5WzSKJFaZhdkSggHi1K+PkuCZ1SXzKD3CPibreSUx9hzoZAAmTjzOfwhVZ7
5HDqhrKfkWNeThAdXlglhNX5LVKhhmBsNGawANO8Pmq+pueWhK/76XgiGcOknyt6
NH1ZE/QZpFeZB2binEhf6leYGwKBgQC3WgF2eaj2c4pugq13oZ9BHms5nsbYQgj8
1H2C1wsgJNiz4tq6xFdvjiaw+r1OrCFrl9nz7npFc7uohtrRxXr7sq5nwMPO3QAP
VxzyZ/TFP/FI70Avz6x+xX7HIlptpQ1mWacsdrvtoIG1c8veEJTyA4JgjoMhdDer
JuPze+ZHLQKBgAzuLIhCzcul/W+Pberyx7A0mJdMtgiyWpwsRaKsNqxD0xD24gqk
XYqQmOxT78Arnc7pEpEXVwArxDRQlJKwI87DluWnPgfKNXWmWT/DC7gsBc65Go+Z
ceorc65JqvPjuRx4e4aQBCK+6rRXESICUvzDuGEBBhXljQ5fyMA1dRtnAoGAQSFH
w74Ttvf0BK2G0SgoWglkLWOpXsiWSLlynDenTBPKppu+9vkn1gi41cU72eZrOL0I
Pw5HgMRLtOGyeTGdLiuaqGBhLepzSv+22KT03dyI6U+TjWhH7gBZuIMLfEdVKzZN
nA3RU+fgK4SmtotNXWrBtWrqScfuvm1iK+unMUECgYBrwc1rU/GI9tH4MWvBX99n
pmFneeIUuXJajOsh7jUyTxcrHl+4kkgHDExuFMNPHfrIG8xCj43fjzxD/spz3xRS
+Ki3i3lYOFNhTUCREt5Xx23hkksK6j7SYlfY0IV2UruWt/UubpsfEqxC1Uk07NTo
l50Xb2GWQesIt9k5jF5IOw==
-----END PRIVATE KEY-----";

#! keeps the error of a request that the server logs
class RequestErrorCapture inherits LoggerAppenderWithLayout {
    private {
        #! the error of the last request that failed, or "none"
        string error = "none";
        Mutex lck();
    }

    constructor() : LoggerAppenderWithLayout("capture", new LoggerLayoutPattern("%m")) {
        open();
    }

    processEventImpl(int type, auto params) {
        if (type == EVENT_LOG) {
            # the error of a request that the server logs
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

#! the handler of the requests; the server collects a request body before the handler is called
class BodyGrowFailureHandler inherits AbstractHttpRequestHandler {
    public {
        #! decremented by "GET /quit"
        Counter quit(1);
    }

    private {
        RequestErrorCapture capture;
    }

    constructor(RequestErrorCapture capture) {
        self.capture = capture;
    }

    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        switch (hdr.path) {
            case "error":
                return makeResponse(200, capture.takeError(), {"Content-Type": "text/plain"});

            case "quit":
                quit.dec();
                break;
        }
        return makeResponse(200, sprintf("served %s %d", hdr.path, body.size()), {"Content-Type": "text/plain"});
    }
}

#! returns the size of the address space of the process in kilobytes
int sub vm_size() {
    return (ReadOnlyFile::readTextFile("/proc/self/status") =~ x/VmSize:\s+([0-9]+)/)[0].toInt();
}

RequestErrorCapture capture();
BodyGrowFailureHandler handler(capture);
Logger logger("test", LoggerLevel::getLevelError());
logger.addAppender(capture);
HttpServer server(<HttpServerOptionInfo>{
    "logger": logger,
});
server.setDefaultHandler("grow", handler);
int port = server.addListener(<HttpListenerOptionInfo>{"service": 0, "node": "127.0.0.1"}).port;
int tls_port = server.addListener(<HttpListenerOptionInfo>{
    "service": 0,
    "node": "127.0.0.1",
    "cert": new SSLCertificate(CertPem),
    "key": new SSLPrivateKey(KeyPem),
    "enable_h3": False,
}).port;
server.waitForAsyncIo();
printf("port %d tls %d vm %d\n", port, tls_port, vm_size());
flush();
if (ARGV[0] != "measure") {
    handler.quit.waitForZero();
}
