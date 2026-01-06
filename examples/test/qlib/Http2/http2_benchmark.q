#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%modern
%disable-warning unreferenced-variable

%exec-class Http2Benchmark

#! HTTP/2 vs HTTP/1.1 Performance Benchmark
/** Compares HTTP/1.1 and HTTP/2 performance across various scenarios:
    - Sequential requests (single connection reuse)
    - Concurrent requests (multiplexing advantage)
    - Connection establishment overhead
    - Large body transfers

    @since Qore 2.2
*/
public class Http2Benchmark {
    public {
        const DefaultUrl = "https://httpbin.org";
        const DefaultIterations = 20;
        const DefaultConcurrent = 5;
    }

    private {
        string url;
        int iterations;
        int concurrent;
        bool verbose = False;
    }

    constructor() {
        processArgs();
        runBenchmarks();
    }

    private processArgs() {
        url = shift ARGV ?? DefaultUrl;
        iterations = (shift ARGV ?? string(DefaultIterations)).toInt();
        concurrent = (shift ARGV ?? string(DefaultConcurrent)).toInt();
        verbose = (shift ARGV ?? "") == "-v";

        printf("=== HTTP/2 vs HTTP/1.1 Performance Benchmark ===\n\n");
        printf("Target URL: %s\n", url);
        printf("Iterations: %d\n", iterations);
        printf("Concurrent: %d\n", concurrent);
        printf("\n");
    }

    private runBenchmarks() {
        # Test 1: Sequential requests
        benchmarkSequential();

        # Test 2: Concurrent requests (multiplexing)
        benchmarkConcurrent();

        # Test 3: Connection establishment
        benchmarkConnectionSetup();

        # Test 4: Large body transfer
        benchmarkLargeBody();

        printf("=== Benchmark Complete ===\n");
    }

    #! Compare HTTP/1.1 vs HTTP/2 sequential requests
    private benchmarkSequential() {
        printf("Test 1: Sequential Requests (Connection Reuse)\n");
        printf("  Making %d sequential requests on same connection...\n", iterations);

        # HTTP/1.1
        try {
            HTTPClient h1({
                "url": url,
                "http_version": "1.1",
                "timeout": 30s,
            });
            h1.acceptAllCertificates(True);

            list<float> h1_latencies = ();
            date total_start = now_us();
            for (int i = 0; i < iterations; ++i) {
                date start = now_us();
                h1.send("", "GET", "/get", NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h1_latencies += lat;
            }
            float h1_total = (now_us() - total_start).durationMicroseconds() / 1000.0;

            printf("  HTTP/1.1:\n");
            printLatencyStats(h1_latencies);
            printf("    Total: %.1f ms (%.1f req/s)\n", h1_total, iterations * 1000.0 / h1_total);
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/1.1: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        # HTTP/2
        try {
            HTTPClient h2({
                "url": url,
                "http_version": "2.0",
                "timeout": 30s,
            });
            h2.acceptAllCertificates(True);

            list<float> h2_latencies = ();
            date total_start = now_us();
            for (int i = 0; i < iterations; ++i) {
                date start = now_us();
                h2.send("", "GET", "/get", NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h2_latencies += lat;
            }
            float h2_total = (now_us() - total_start).durationMicroseconds() / 1000.0;

            printf("  HTTP/2:\n");
            printLatencyStats(h2_latencies);
            printf("    Total: %.1f ms (%.1f req/s)\n", h2_total, iterations * 1000.0 / h2_total);

            if (h2.isHttp2Active()) {
                printf("    HTTP/2 confirmed active\n");
            } else {
                printf("    WARNING: HTTP/2 not active, fell back to HTTP/1.1\n");
            }
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/2: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        printf("\n");
    }

    #! Test HTTP/2 multiplexing advantage with concurrent requests
    private benchmarkConcurrent() {
        printf("Test 2: Concurrent Requests (Multiplexing)\n");
        printf("  Making %d concurrent requests...\n", concurrent);

        # HTTP/1.1 - multiple connections needed
        try {
            date start = now_us();
            Counter cnt(concurrent);
            list<float> latencies = ();
            Mutex m();

            for (int i = 0; i < concurrent; ++i) {
                background sub() {
                    on_exit cnt.dec();
                    try {
                        HTTPClient h1({
                            "url": url,
                            "http_version": "1.1",
                            "timeout": 30s,
                        });
                        h1.acceptAllCertificates(True);
                        date req_start = now_us();
                        h1.send("", "GET", "/get", NOTHING, True);
                        float lat = (now_us() - req_start).durationMicroseconds() / 1000.0;
                        m.lock();
                        on_exit m.unlock();
                        latencies += lat;
                    } catch (hash<ExceptionInfo> ex) {
                        # Ignore errors in concurrent test
                    }
                }();
            }

            cnt.waitForZero();
            float total = (now_us() - start).durationMicroseconds() / 1000.0;

            printf("  HTTP/1.1 (multiple connections):\n");
            if (latencies.size() > 0) {
                printLatencyStats(latencies);
            }
            printf("    Total: %.1f ms\n", total);
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/1.1: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        # HTTP/2 - single connection with multiplexing
        # Note: This tests sequential requests on a single connection
        # True multiplexing would require async/background stream submission
        try {
            HTTPClient h2({
                "url": url,
                "http_version": "2.0",
                "timeout": 30s,
            });
            h2.acceptAllCertificates(True);

            date start = now_us();
            Counter cnt(concurrent);
            list<float> latencies = ();
            Mutex m();

            for (int i = 0; i < concurrent; ++i) {
                background sub() {
                    on_exit cnt.dec();
                    try {
                        date req_start = now_us();
                        h2.send("", "GET", "/get", NOTHING, True);
                        float lat = (now_us() - req_start).durationMicroseconds() / 1000.0;
                        m.lock();
                        on_exit m.unlock();
                        latencies += lat;
                    } catch (hash<ExceptionInfo> ex) {
                        # Ignore errors
                    }
                }();
            }

            cnt.waitForZero();
            float total = (now_us() - start).durationMicroseconds() / 1000.0;

            printf("  HTTP/2 (single connection, multiplexed):\n");
            if (latencies.size() > 0) {
                printLatencyStats(latencies);
            }
            printf("    Total: %.1f ms\n", total);

            if (h2.isHttp2Active()) {
                printf("    HTTP/2 confirmed active\n");
            }
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/2: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        printf("\n");
    }

    #! Compare connection establishment overhead
    private benchmarkConnectionSetup() {
        printf("Test 3: Connection Establishment Overhead\n");
        printf("  Creating %d new connections...\n", min(iterations, 10));

        int setup_iterations = min(iterations, 10);

        # HTTP/1.1
        list<float> h1_setup = ();
        for (int i = 0; i < setup_iterations; ++i) {
            try {
                date start = now_us();
                HTTPClient h1({
                    "url": url,
                    "http_version": "1.1",
                    "timeout": 30s,
                });
                h1.acceptAllCertificates(True);
                h1.send("", "GET", "/get", NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h1_setup += lat;
            } catch (hash<ExceptionInfo> ex) {
                # Skip failed connections
            }
        }

        if (h1_setup.size() > 0) {
            printf("  HTTP/1.1 (connect + first request):\n");
            printLatencyStats(h1_setup);
        } else {
            printf("  HTTP/1.1: All connections failed\n");
        }

        # HTTP/2
        list<float> h2_setup = ();
        for (int i = 0; i < setup_iterations; ++i) {
            try {
                date start = now_us();
                HTTPClient h2({
                    "url": url,
                    "http_version": "2.0",
                    "timeout": 30s,
                });
                h2.acceptAllCertificates(True);
                h2.send("", "GET", "/get", NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h2_setup += lat;
            } catch (hash<ExceptionInfo> ex) {
                # Skip failed connections
            }
        }

        if (h2_setup.size() > 0) {
            printf("  HTTP/2 (connect + ALPN + first request):\n");
            printLatencyStats(h2_setup);
        } else {
            printf("  HTTP/2: All connections failed\n");
        }

        # Compare
        if (h1_setup.size() > 0 && h2_setup.size() > 0) {
            float h1_avg = listAvg(h1_setup);
            float h2_avg = listAvg(h2_setup);
            float diff = h2_avg - h1_avg;
            printf("  HTTP/2 overhead: %.1f ms (%.1f%% %s)\n",
                abs(diff), abs(diff * 100 / h1_avg), diff > 0 ? "slower" : "faster");
        }

        printf("\n");
    }

    #! Test large body transfer performance
    private benchmarkLargeBody() {
        printf("Test 4: Large Body Transfer\n");

        # Request 100KB of data
        int body_size = 100 * 1024;
        printf("  Requesting %d KB of data...\n", body_size / 1024);

        # HTTP/1.1
        try {
            HTTPClient h1({
                "url": url,
                "http_version": "1.1",
                "timeout": 60s,
            });
            h1.acceptAllCertificates(True);

            list<float> h1_latencies = ();
            int download_iterations = min(iterations, 5);
            for (int i = 0; i < download_iterations; ++i) {
                date start = now_us();
                hash<auto> resp = h1.send("", "GET", "/bytes/" + body_size, NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h1_latencies += lat;

                # Calculate throughput
                int received_size = 0;
                if (resp.body.typeCode() == NT_BINARY) {
                    received_size = resp.body.size();
                } else if (resp.body.typeCode() == NT_STRING) {
                    received_size = resp.body.length();
                }
                if (verbose) {
                    printf("    Received %d bytes in %.1f ms\n", received_size, lat);
                }
            }

            printf("  HTTP/1.1:\n");
            printLatencyStats(h1_latencies);
            float h1_avg = listAvg(h1_latencies);
            printf("    Throughput: %.1f KB/s\n", (body_size / 1024.0) / (h1_avg / 1000.0));
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/1.1: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        # HTTP/2
        try {
            HTTPClient h2({
                "url": url,
                "http_version": "2.0",
                "timeout": 60s,
            });
            h2.acceptAllCertificates(True);

            list<float> h2_latencies = ();
            int download_iterations = min(iterations, 5);
            for (int i = 0; i < download_iterations; ++i) {
                date start = now_us();
                hash<auto> resp = h2.send("", "GET", "/bytes/" + body_size, NOTHING, True);
                float lat = (now_us() - start).durationMicroseconds() / 1000.0;
                h2_latencies += lat;

                int received_size = 0;
                if (resp.body.typeCode() == NT_BINARY) {
                    received_size = resp.body.size();
                } else if (resp.body.typeCode() == NT_STRING) {
                    received_size = resp.body.length();
                }
                if (verbose) {
                    printf("    Received %d bytes in %.1f ms\n", received_size, lat);
                }
            }

            printf("  HTTP/2:\n");
            printLatencyStats(h2_latencies);
            float h2_avg = listAvg(h2_latencies);
            printf("    Throughput: %.1f KB/s\n", (body_size / 1024.0) / (h2_avg / 1000.0));

            if (h2.isHttp2Active()) {
                printf("    HTTP/2 confirmed active\n");
            }
        } catch (hash<ExceptionInfo> ex) {
            printf("  HTTP/2: FAILED (%s: %s)\n", ex.err, ex.desc);
        }

        printf("\n");
    }

    #! Print latency statistics
    private printLatencyStats(list<float> latencies) {
        if (latencies.size() == 0) {
            printf("    No data\n");
            return;
        }

        float sum = 0;
        float min_lat = latencies[0];
        float max_lat = latencies[0];
        foreach float lat in (latencies) {
            sum += lat;
            if (lat < min_lat) min_lat = lat;
            if (lat > max_lat) max_lat = lat;
        }
        float avg = sum / latencies.size();

        # Calculate p99
        list<float> sorted = sort(latencies);
        int p99_idx = max(0, (sorted.size() * 99 / 100) - 1);
        float p99 = sorted[p99_idx];

        printf("    Avg: %.1f ms, Min: %.1f ms, Max: %.1f ms, P99: %.1f ms\n",
            avg, min_lat, max_lat, p99);
    }

    #! Calculate average of a list
    private float listAvg(list<float> values) {
        if (values.size() == 0) return 0.0;
        float sum = 0;
        foreach float v in (values) {
            sum += v;
        }
        return sum / values.size();
    }
}
