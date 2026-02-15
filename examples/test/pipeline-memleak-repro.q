#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Standalone reproduction for pipeline metrics memory leak investigation
# Simulates FSM pipeline execution with metrics callbacks to identify
# unbounded memory growth.

%modern
%strict-args
%require-types
%enable-all-warnings

%requires DataProvider

%try-module process
%define NoProcess
%endtry

%ifndef NoProcess
%exec-class PipelineMemLeakRepro

#! Simple passthrough processor for testing
class PassthroughProcessor inherits DataProvider::AbstractDataProcessor {
    private bool supportsBulkApiImpl() {
        return False;
    }

    private submitImpl(code<nothing(auto)> enqueue, auto _data) {
        enqueue(_data);
    }
}

#! Slow processor that simulates real-world processing latency
class SlowProcessor inherits DataProvider::AbstractDataProcessor {
    public {
        int delay_us;
    }

    constructor(int delay_us = 100) {
        self.delay_us = delay_us;
    }

    private bool supportsBulkApiImpl() {
        return False;
    }

    private submitImpl(code<nothing(auto)> enqueue, auto _data) {
        if (delay_us > 0) {
            usleep(delay_us);
        }
        enqueue(_data);
    }
}

class PipelineMemLeakRepro {
    private {
        int initial_rss;
    }

    constructor() {
        printf("Pipeline Memory Leak Reproduction Test\n");
        printf("=======================================\n");
        printf("Qore %s\n", Qore::VersionString);
        printMem("start ");

        initial_rss = getMem().rss;

        # Test 1: Baseline without metrics
        testBaseline(100000);

        # Test 2: Fast callback - many callbacks, but they complete quickly
        testFastCallback(100000, 100);

        # Test 3: Slow callback - simulates Qorus event posting with 50ms delay
        testSlowCallback(50000, 100, 50);

        # Test 4: Repeated pipeline creation/destruction
        testRepeatedPipelines(100, 1000);

        # Test 5: Stress test - large volume, slow callbacks, multiple elements
        testStress(200000, 100);

        printf("\n=== Final ===\n");
        printMem("end   ");
        hash<auto> mem = getMem();
        printf("  RSS growth from start: %s\n", fmtMem(mem.rss - initial_rss));
    }

    private hash<auto> getMem() {
        return Process::getMemorySummaryInfo();
    }

    private string fmtMem(int bytes) {
        if (bytes >= 1024 * 1024 * 1024) {
            return sprintf("%.2f GiB", bytes / (1024.0 * 1024.0 * 1024.0));
        }
        if (bytes >= 1024 * 1024) {
            return sprintf("%.2f MiB", bytes / (1024.0 * 1024.0));
        }
        return sprintf("%.2f KiB", bytes / 1024.0);
    }

    private printMem(string label) {
        hash<auto> mem = getMem();
        printf("  [%s] RSS: %s  VSZ: %s  threads: %d\n", label, fmtMem(mem.rss), fmtMem(mem.vsz), num_threads());
    }

    # =========================================================================
    # Test 1: Baseline - pipeline without metrics (should have no leak)
    # =========================================================================
    private testBaseline(int num_records) {
        printf("\n=== Test 1: Baseline - no metrics callback (%d records) ===\n", num_records);
        printMem("before");

        {
            hash<PipelineOptionInfo> opts = <PipelineOptionInfo>{
                "name": "baseline-pipeline",
            };

            DataProviderPipeline pipeline(opts);
            pipeline.append(new PassthroughProcessor());

            for (int i = 0; i < num_records; ++i) {
                pipeline.submit({"id": i, "data": "record-" + i.toString()});
            }
            pipeline.waitDone();
        }  # pipeline goes out of scope here

        printMem("after ");
    }

    # =========================================================================
    # Test 2: Pipeline with fast metrics callback (frequent thread spawning)
    # =========================================================================
    private testFastCallback(int num_records, int record_interval) {
        printf("\n=== Test 2: Fast callback (records: %d, interval: %d) ===\n", num_records, record_interval);
        printMem("before");

        int callback_count = 0;

        {
            code metrics_callback = sub (hash<PipelineMetricsSnapshot> m) {
                ++callback_count;
            };

            hash<PipelineOptionInfo> opts = <PipelineOptionInfo>{
                "name": "fast-callback-pipeline",
                "metrics_callback": metrics_callback,
                "metrics_interval_ms": 10,
                "metrics_record_interval": record_interval,
            };

            DataProviderPipeline pipeline(opts);
            pipeline.append(new PassthroughProcessor());

            for (int i = 0; i < num_records; ++i) {
                pipeline.submit({"id": i, "data": "record-" + i.toString()});
            }
            pipeline.waitDone();

            printf("  callbacks invoked: %d\n", callback_count);
            printMem("done  ");
        }

        # Give background threads time to complete
        usleep(500000);
        printMem("after ");
    }

    # =========================================================================
    # Test 3: Pipeline with SLOW metrics callback (simulates event posting)
    # This is the critical test - slow callbacks cause thread accumulation
    # =========================================================================
    private testSlowCallback(int num_records, int record_interval, int callback_delay_ms) {
        printf("\n=== Test 3: Slow callback (records: %d, interval: %d, callback_delay: %dms) ===\n",
            num_records, record_interval, callback_delay_ms);
        printMem("before");

        int callback_count = 0;
        int max_threads = num_threads();

        {
            code metrics_callback = sub (hash<PipelineMetricsSnapshot> m) {
                ++callback_count;
                # Simulate slow event posting (serialization + WebSocket delivery)
                usleep(callback_delay_ms * 1000);
                int t = num_threads();
                if (t > max_threads) {
                    max_threads = t;
                }
            };

            hash<PipelineOptionInfo> opts = <PipelineOptionInfo>{
                "name": "slow-callback-pipeline",
                "metrics_callback": metrics_callback,
                "metrics_interval_ms": 10,
                "metrics_record_interval": record_interval,
            };

            DataProviderPipeline pipeline(opts);
            pipeline.append(new SlowProcessor(100));  # 100us per record

            for (int i = 0; i < num_records; ++i) {
                pipeline.submit({"id": i, "data": "record-" + i.toString()});
                if (i > 0 && (i % 10000) == 0) {
                    printMem(sprintf("@ %d records", i));
                }
            }
            pipeline.waitDone();

            printf("  callbacks invoked: %d, max concurrent threads: %d\n", callback_count, max_threads);
            printMem("done  ");
        }

        # Wait for background callback threads to drain
        usleep(callback_delay_ms * 2 * 1000);
        printMem("after ");
    }

    # =========================================================================
    # Test 4: Repeated pipeline creation/destruction (leak accumulation test)
    # =========================================================================
    private testRepeatedPipelines(int iterations, int records_per_pipeline) {
        printf("\n=== Test 4: Repeated pipelines (iterations: %d, records: %d each) ===\n",
            iterations, records_per_pipeline);
        printMem("before");

        for (int iter = 0; iter < iterations; ++iter) {
            {
                int callback_count = 0;
                code metrics_callback = sub (hash<PipelineMetricsSnapshot> m) {
                    ++callback_count;
                };

                hash<PipelineOptionInfo> opts = <PipelineOptionInfo>{
                    "name": sprintf("repeated-pipeline-%d", iter),
                    "metrics_callback": metrics_callback,
                    "metrics_interval_ms": 10,
                    "metrics_record_interval": 50,
                };

                DataProviderPipeline pipeline(opts);
                pipeline.append(new PassthroughProcessor());

                for (int i = 0; i < records_per_pipeline; ++i) {
                    pipeline.submit({"id": i, "data": "record-" + i.toString()});
                }
                pipeline.waitDone();
            }

            # Allow background threads to finish
            usleep(100000);

            if ((iter + 1) % 10 == 0) {
                printMem(sprintf("iter %d", iter + 1));
            }
        }

        usleep(500000);
        printMem("final ");
    }

    # =========================================================================
    # Test 5: Large data volume with slow callback (stress test)
    # =========================================================================
    private testStress(int num_records, int callback_delay_ms) {
        printf("\n=== Test 5: Stress test (records: %d, callback_delay: %dms) ===\n",
            num_records, callback_delay_ms);
        printMem("before");

        int callback_count = 0;
        int max_threads = num_threads();

        {
            code metrics_callback = sub (hash<PipelineMetricsSnapshot> m) {
                ++callback_count;
                usleep(callback_delay_ms * 1000);
                int t = num_threads();
                if (t > max_threads) {
                    max_threads = t;
                }
            };

            hash<PipelineOptionInfo> opts = <PipelineOptionInfo>{
                "name": "stress-pipeline",
                "metrics_callback": metrics_callback,
                "metrics_interval_ms": 50,
                "metrics_record_interval": 100,
            };

            DataProviderPipeline pipeline(opts);
            # Multi-element pipeline
            pipeline.append(new PassthroughProcessor());
            pipeline.append(new PassthroughProcessor());
            pipeline.append(new PassthroughProcessor());

            for (int i = 0; i < num_records; ++i) {
                pipeline.submit({"id": i, "data": "record-" + i.toString()});
                if (i > 0 && (i % 50000) == 0) {
                    printMem(sprintf("@ %d records", i));
                }
            }
            pipeline.waitDone();

            printf("  callbacks invoked: %d, max concurrent threads: %d\n", callback_count, max_threads);
            printMem("done  ");
        }

        # Wait for background callback threads to drain
        sleep(callback_delay_ms > 0 ? (callback_delay_ms * 2 / 1000 + 1) : 1);
        printMem("after ");
    }
}
%endif
