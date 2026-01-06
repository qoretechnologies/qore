#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%modern

%requires HttpServer
%requires Util
%requires Mime

%exec-class HttpServerBenchmark

class SimpleHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        return makeResponse(200, "OK");
    }
}

class HttpServerBenchmark {
    public {
        const Iterations = 100;
        const Connections = 20;
    }

    private {
        HttpServer syncServer;
        HttpServer asyncServer;
        int syncPort;
        int asyncPort;
    }

    constructor() {
        SimpleHandler handler();

        # Create sync mode server
        syncServer = new HttpServer();
        syncServer.setHandler("test", "", MimeTypeHtml, handler);
        syncServer.setDefaultHandler("test", handler);
        syncPort = syncServer.addListener(<HttpListenerOptionInfo>{
            "service": 0,
            "name": "sync-test",
        }).port;

        # Create async mode server
        asyncServer = new HttpServer(<HttpServerOptionInfo>{
            "async_mode": True,
        });
        asyncServer.setHandler("test", "", MimeTypeHtml, handler);
        asyncServer.setDefaultHandler("test", handler);
        asyncPort = asyncServer.addListener(<HttpListenerOptionInfo>{
            "service": 0,
            "name": "async-test",
        }).port;

        # Wait for I/O thread to start
        usleep(100ms);

        printf("Sync server on port %d\n", syncPort);
        printf("Async server on port %d\n", asyncPort);
        printf("\n");
        flush();

        try {
            runBenchmarks();
        } catch (hash<ExceptionInfo> ex) {
            printf("ERROR: %s: %s\n", ex.err, ex.desc);
        }

        syncServer.stop();
        asyncServer.stop();
    }

    private runBenchmarks() {
        # Benchmark 1: Sequential requests on single connection
        printf("=== Sequential Requests (single connection, %d iterations) ===\n", Iterations);

        float syncTime = benchmarkSequential(syncPort);
        printf("Sync mode:  %7.2f ms total, %.3f ms/request\n", syncTime, syncTime / Iterations);

        float asyncTime = benchmarkSequential(asyncPort);
        printf("Async mode: %7.2f ms total, %.3f ms/request\n", asyncTime, asyncTime / Iterations);
        printf("\n");

        # Benchmark 2: Concurrent connections
        printf("=== Concurrent Connections (%d connections, %d requests each) ===\n",
               Connections, Iterations);

        syncTime = benchmarkConcurrent(syncPort);
        printf("Sync mode:  %7.2f ms total, %.3f ms/request\n", syncTime,
               syncTime / (Connections * Iterations));

        asyncTime = benchmarkConcurrent(asyncPort);
        printf("Async mode: %7.2f ms total, %.3f ms/request\n", asyncTime,
               asyncTime / (Connections * Iterations));
        printf("\n");

        # Benchmark 3: Connection establishment overhead
        printf("=== Connection Establishment (%d new connections) ===\n", Iterations);

        syncTime = benchmarkConnectionEstablishment(syncPort);
        printf("Sync mode:  %7.2f ms total, %.3f ms/connection\n", syncTime, syncTime / Iterations);

        asyncTime = benchmarkConnectionEstablishment(asyncPort);
        printf("Async mode: %7.2f ms total, %.3f ms/connection\n", asyncTime, asyncTime / Iterations);
    }

    private float benchmarkSequential(int port) {
        HTTPClient client({"url": sprintf("http://localhost:%d", port), "timeout": 10s});
        client.connect();
        on_exit client.disconnect();

        date start = now_us();
        for (int i = 0; i < Iterations; ++i) {
            client.get("/");
        }
        date end = now_us();

        return (end - start).durationMilliseconds();
    }

    private float benchmarkConcurrent(int port) {
        Counter cnt(Connections);

        date start = now_us();
        for (int i = 0; i < Connections; ++i) {
            background concurrentWorker(port, cnt);
        }
        cnt.waitForZero();
        date end = now_us();

        return (end - start).durationMilliseconds();
    }

    private concurrentWorker(int port, Counter cnt) {
        on_exit cnt.dec();

        HTTPClient client({"url": sprintf("http://localhost:%d", port), "timeout": 5s});
        client.connect();
        on_exit client.disconnect();

        for (int i = 0; i < Iterations; ++i) {
            client.get("/");
        }
    }

    private float benchmarkConnectionEstablishment(int port) {
        date start = now_us();
        for (int i = 0; i < Iterations; ++i) {
            HTTPClient client({"url": sprintf("http://localhost:%d", port), "timeout": 5s});
            client.connect();
            client.get("/");
            client.disconnect();
        }
        date end = now_us();

        return (end - start).durationMilliseconds();
    }

    private log(string fmt) {
        # Silent logging for benchmarks
    }
}
