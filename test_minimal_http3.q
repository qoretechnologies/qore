#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%requires qlib/Util.qm
%requires qlib/Logger.qm
%requires qlib/HttpServerUtil.qm
%requires qlib/AsyncSocketIo
%requires qlib/HttpServerAsyncIo
%requires qlib/HttpServer.qm
%requires qlib/HttpClientIo

class TestServer {
    private HttpServer srv;
    private int port;

    constructor(int port_arg = 0) {
        port = port_arg;
        srv = new HttpServer({"logger": new Logger("test"), "port": port});
        srv.setHandler("/test", NOTHING, sub (HttpListenerInterface listener, hash<auto> cx) {
            listener.sendResponse({"status_code": 200, "body": "OK"});
        });
    }

    int start() {
        port = srv.startHTTP3();
        printf("Server listening on port %d\n", port);
        return port;
    }

    stop() {
        srv.stop();
    }

    int getPort() {
        return port;
    }
}

sub main() {
    printf("Test: Single HTTP/3 request in IR mode\n");

    # Start server
    TestServer server(0);
    int port = server.start();

    try {
        # Give server time to start
        usleep(100ms);

        # Create client
        printf("Creating client connection...\n");
        HttpClientConnectionManager mgr();
        HttpClientStreamHandle stream = cast<HttpClientStreamHandle>(
            mgr.acquireStream(sprintf("https://localhost:%d", port)));

        printf("Submitting request...\n");
        hash<auto> resp = stream.request("GET", "/test", NOTHING, NOTHING, 5000ms);

        printf("Response: status=%d, body=%y\n", resp.status_code, resp.body);

        if (resp.status_code == 200) {
            printf("✓ Test PASSED\n");
        } else {
            printf("✗ Test FAILED: unexpected status %d\n", resp.status_code);
            exit(1);
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("✗ Test FAILED: %s: %s\n", ex.err, ex.desc);
        exit(1);
    } finally {
        server.stop();
    }
}

main();
