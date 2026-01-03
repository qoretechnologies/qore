#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Comprehensive memory test for HTTP/2 support
# Run with: valgrind --leak-check=full --show-leak-kinds=all qore valgrind_test.q
#
# Copyright (C) 2025 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

%new-style
%strict-args
%require-types

%requires ../../../../qlib/Util.qm
%requires ../../../../qlib/Logger.qm
%requires ../../../../qlib/HttpServerUtil.qm
%requires ../../../../qlib/HttpServer.qm

# HTTP/2 test certificate and key (PEM format)
const CertPem = "-----BEGIN CERTIFICATE-----
MIIFNjCCBB6gAwIBAgISA1ELHYM0GFfi2BdvZfzLGJMLMA0GCSqGSIb3DQEBCwUA
MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD
EwJSMzAeFw0yMjA4MDcwOTAwMDdaFw0yMjExMDUwOTAwMDZaMCIxIDAeBgNVBAMT
F2hxLnFvcmV0ZWNobm9sb2dpZXMuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEAlw84m+9dCjeD4s8YYVZ3gbv8YkYQGyyaojgdJuarTyJPYnGzhMf7
PF+Y2398k+8ydAnOTwJXRcaU36f0hHDvTagP1f0EFg3kWlplzsrtrDI/HZBRH0W2
+54YE6FhPEtEkkO1aKy+VXb8QMQKSmsUZS+IxSe+69tWFI60tW92eq53tFAwEWN+
0oBeJBCASRQa7bq1J/BgZhlBSyUQ6Zf+wqUSfmVEf7tsFujyNZ2dfxHtPtLxRspA
7yhgnQQaXHemyEWeZWjIbToazG8SvutmcwIFK/TtE0anWdTR+Hn92mxH9mxnnyns
Ag+RlqxnEpBDIb1ufwCFUhMXBwRiiZIM1QIDAQABo4ICVDCCAlAwDgYDVR0PAQH/
BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAMBgNVHRMBAf8E
AjAAMB0GA1UdDgQWBBSj2nUL9X+zXI7p0fFhg03WZAS0xDAfBgNVHSMEGDAWgBQU
LrMXt1hWy65QCUDmH6+dixTCxjBVBggrBgEFBQcBAQRJMEcwIQYIKwYBBQUHMAGG
FWh0dHA6Ly9yMy5vLmxlbmNyLm9yZzAiBggrBgEFBQcwAoYWaHR0cDovL3IzLmku
bGVuY3Iub3JnLzAiBgNVHREEGzAZghdocS5xb3JldGVjaG5vbG9naWVzLmNvbTBM
BgNVHSAERTBDMAgGBmeBDAECATA3BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIB
FhpodHRwOi8vY3BzLmxldHNlbmNyeXB0Lm9yZzCCAQYGCisGAQQB1nkCBAIEgfcE
gfQA8gB3AN+lXqtogk8fbK3uuF9OPlrqzaISpGpejjsSwCBEXCpzAAABgne/YgYA
AAQDAEgwRgIhAP/ERcskhKhF7M8VIejtrwEtDXJoX1IXec//r64jkOlUAiEAhheZ
VMT5cZ2uoPGoD6+SuQY/CYBuHdXNR/pUC3SGOQEAdwApeb7wnjk5IfBWc59jpXfl
vld9nGAK+PlNXSZcJV3HhAAAAYJ3v2H5AAAEAwBIMEYCIQCmiChg/6dLhE3TfGum
JR7k8s7ibmqw2KVJI3oUR/ogBAIhAIXIIhyqb3W/34ATNT9dIPCGFKpghQw82G8w
ANnChDgYMA0GCSqGSIb3DQEBCwUAA4IBAQAovvC8AiF7+uNLJCEXMe3VeI4Ne+l1
mqCRRujz7ijlr6VgNxZt+i/kx1HJKrSKnQZRQ48xWAipMVYfXxH3u20p4RkkW2nj
jjIVQvQvlFhDjLaJR74PYopp0lPuBW9RKg+C+l3vvjxjkin/MOBX2apGOvC4LJwb
2s6f8cArBRvdhA7nwEmlP3+aqxxkp9STZYpxuKR4F9fRGtg0Y39Db+3XkYp7Y/hV
DBVpHegoty2VFErehkJUmgdNoLdTuC8gHgA3p5bCbApyVGjBuO+QpSWG/3WiBDfm
1IpGT8P2OhFruQHryHEKmY6f5huZV2Y0gFmqBwRiR1ToF5gFveTUJM3d
-----END CERTIFICATE-----";

const KeyPem = "-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCXDzib710KN4Pi
zxhhVneBu/xiRhAbLJqiOB0m5qtPIk9icbOEx/s8X5jbf3yT7zJ0Cc5PAldFxpTf
p/SEcO9NqA/V/QQWDeRaWmXOyu2sMj8dkFEfRbb7nhgToWE8S0SSQ7VorL5VdvxA
xApKaxRlL4jFJ77r21YUjrS1b3Z6rne0UDARY37SgF4kEIBJFBrturUn8GBmGUFL
JRDpl/7CpRJ+ZUR/u2wW6PI1nZ1/Ee0+0vFGykDvKGCdBBpcd6bIRZ5laMhtOhrM
bxK+62ZzAgUr9O0TRqdZ1NH4ef3abEf2bGefKewCD5GWrGcSkEMhvW5/AIVSExcH
BGKJkgzVAgMBAAECggEAPhvyCJtYQ9UjkuPXgF4O8PacBMQN5z5lrgEoa1A4a2cO
AMoDJ7sZ327m6Ij4bdLRichmXTH3NCc8GuFxterBWcqaCD/pqC+6DjRQ27+wDTbz
oHIwCI2feME94QRfeGzyGrlgI1OzRmyPtwljuclhL2Fl+Loo08zxDa7HOjpEGphz
wkD2gQy5S7Mi5ekBZMhy8EklpZdKlOFdvhjo7vQu8vY8xJZ6jN65RV+RBscbmZQl
llK1uW/8UyRIAUK9sM6Ozxbiz7QQELHpSvY3JIiZC5A6rBk2DGqOu8ouzDrQuHsB
LHWjzceu6UcJY0tCeOUkVDnoxbj2UcINEAIABIPN2QKBgQDHHbrwdW8qRecLw8y2
FJDO/6+8VmRKtj8B7CeOYuEZet2UvnPssisCovu0jnMa2ysF/L4YQAWa5XY0bB+E
R/NRzeh4i0jIsW70h2qePTK+XRAGhCCKiCcb7nkED72TTplQ5wgRA/oWPLOrys34
f+YqCI7BVrbWzgDyrhEdRAXSbwKBgQDCNuMlLcGTTi04OIQFDZmzjdvx9KvHvksz
zff9Y0Rb1eUsXF2oG3S55umgeUcG9Nk2KigJzO6WAmTT6rcbn4tsRsnkLdqcEPmg
P1wbBikW3Bz935k7yPcx05bVshLrJsK0Gxa8YYE6hh/PbdiQFtHf2r4uYu1kxC01
518U8oPm+wKBgDfyJIpXlKp+BZMKqsQmNyHSOaBjbb6IQl/Z6KtbIQA1w3h9orjI
vsj43lw3AiRznD0MbKUHqAuDmZjVIG3cgYNkpYLpL8QkBpbyTYS0kUNnho8uJK6H
3uU8NghsG8n99ZoDsAKH6YbB+4Gzc/f0h8kbqnCsWqc0LpQBUJG2gSRFAoGBAI3P
4kBNjuFu3hoFOnEuIyM23HlqPNyXGPZ02TXOfCXKo5Kmx0Ru9+aes80XgUOVGd4x
Hhc56qTijpkm9BlZgEbJ0bWpvczjoELgwPKCpxIoG4tM7+j1r3pUk/jqFGJcZSN5
/DoFwITpVuTxwoZEA2+/m8rnNYy0qoaHsafsBWBtAoGAAPYkINDjhAslu2gQlsZi
7eA4coUg5eNUI+YYy4xonJAU488AsrcAWr5nGEids4ZM5PqKkJezG+6gctaYnjjZ
IQrghzR1AC0Y3XDSuJYGRcZeUF2XZmOjcXNm0oMxeMEeEuuNLOTZ2pp7aUcYIOfV
OKKqgBjNAlYDdPhOy8O8Agk=
-----END PRIVATE KEY-----";

class TestHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        return makeResponse(200, sprintf("method=%s, path=%s", hdr.method, hdr.path));
    }
}

printf("HTTP/2 Memory Test Suite\n");
printf("========================\n\n");

# Setup server
Logger logger("test", LoggerLevel::getLevelInfo());
hash<HttpServerOptionInfo> http_opts = <HttpServerOptionInfo>{
    "logger": logger,
};
HttpServer server(http_opts);

SSLCertificate cert(CertPem);
SSLPrivateKey key(KeyPem);
hash<HttpListenerOptionInfo> lh = <HttpListenerOptionInfo>{
    "service": 0,
    "node": "localhost",
    "cert": cert,
    "key": key,
};
server.setDefaultHandler("test", new TestHandler());
int port = server.addListener(lh).port;
string url = sprintf("https://localhost:%d", port);

# Test 1: Socket ALPN negotiation (repeated)
printf("Test 1: Socket ALPN memory test...\n");
for (int i = 0; i < 100; i++) {
    Socket sock();
    sock.setAlpnProtocols(("h2", "http/1.1"));
    sock.connect(sprintf("localhost:%d", port), 5000);
    sock.acceptAllCertificates(True);
    sock.upgradeClientToSSL(5000);
    *string proto = sock.getAlpnProtocol();
    sock.close();
}
printf("  Socket ALPN test passed\n");

# Test 2: HTTPClient HTTP/2 enable/disable (repeated)
printf("Test 2: HTTPClient HTTP/2 enable/disable memory test...\n");
for (int i = 0; i < 100; i++) {
    HTTPClient client({"url": url});
    client.acceptAllCertificates(True);
    client.setHttp2Enabled(True);
    bool enabled = client.isHttp2Enabled();
    client.setHttp2Enabled(False);
    enabled = client.isHttp2Enabled();
}
printf("  HTTPClient enable/disable test passed\n");

# Test 3: HTTPClient HTTP/2 settings (repeated)
printf("Test 3: HTTPClient HTTP/2 settings memory test...\n");
for (int i = 0; i < 100; i++) {
    HTTPClient client({"url": url});
    client.acceptAllCertificates(True);
    client.setHttp2Enabled(True);
    *hash<auto> settings = client.getHttp2Settings();
    client.setHttp2Settings({
        "max_concurrent_streams": 100 + i,
        "initial_window_size": 65535 + i,
    });
}
printf("  HTTPClient settings test passed\n");

# Test 4: HTTPClient HTTP version string (repeated)
printf("Test 4: HTTPClient version string memory test...\n");
for (int i = 0; i < 100; i++) {
    HTTPClient client({"url": url});
    client.acceptAllCertificates(True);
    string v1 = client.getHttpVersion();
    client.setHttp2Enabled(True);
    string v2 = client.getHttpVersion();
}
printf("  HTTPClient version string test passed\n");

# Test 5: Full connection cycle (repeated)
printf("Test 5: Full connection cycle memory test...\n");
for (int i = 0; i < 50; i++) {
    HTTPClient client({"url": url});
    client.acceptAllCertificates(True);
    client.setHttp2Enabled(True);

    hash<auto> info;
    try {
        hash<auto> resp = client.send(NOTHING, "GET", "/test" + string(i), NOTHING, NOTHING, \info);
    } catch (hash<ExceptionInfo> ex) {
        # Connection errors are OK for this memory test
    }

    client.disconnect();
}
printf("  Full connection cycle test passed\n");

# Cleanup
delete server;

printf("\n========================\n");
printf("All HTTP/2 memory tests passed!\n");
