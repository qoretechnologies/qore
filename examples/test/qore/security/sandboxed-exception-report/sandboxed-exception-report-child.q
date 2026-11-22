#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the child of sandboxed-exception-report.qtest (qore#7139): runs the case given as its argument; prints the ID of the
# sandboxed Program ("pgm=<id>") and "done" on stdout, and the reports on stdout (the logger) or stderr

%modern
%no-child-restrictions
%prepend-module-path "${SCRIPT_DIR}"
%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires Logger
# loaded by the child, so that the sandboxed Program imports it
%requires ReportCheck

Logger sub stdout_logger() {
    Logger logger("report", LoggerLevel::getLevelInfo());
    LoggerAppenderStdOut appender("out", new LoggerLayoutPattern("LOG: %m%n"));
    appender.open();
    logger.addAppender(appender);
    return logger;
}

sub main() {
    string mode = ARGV[0];
    AsyncIoController::setMaxCallbackWorkers(1);
    Queue q();
    Program p(PO_MODERN);
    p.setSandboxManager(new SandboxManager());
    p.loadModule("ReportCheck");
    p.parse(ReadOnlyFile::readTextFile(get_script_dir() + "/tenant.q"), "tenant");
    printf("pgm=%d\n", p.getProgramId());
    switch (mode) {
        case "timer-logger":
            set_async_io_logger(stdout_logger());
            p.callFunction("timer", q);
            q.get(60s);
            break;
        case "timer-stderr":
            p.callFunction("timer", q);
            q.get(60s);
            break;
        case "task":
            p.callFunction("task", q);
            q.get(60s);
            break;
        case "tld-task":
            p.callFunction("tld_task", q);
            q.get(60s);
            break;
        case "pool":
            p.callFunction("pool", q);
            break;
        case "background":
            # the process ends after the thread
            p.callFunction("bg");
            break;
        case "chars":
            p.callFunction("bg_chars");
            break;
        case "host-runs-tenant-closure": {
            # the host runs the tenant's closure in a thread of its own, with no sandboxed caller on its stack
            code c = p.callFunction("closure");
            # the tenant's Program stays until the thread has ended
            Counter cnt(1);
            background sub () {
                on_exit cnt.dec();
                c();
            }();
            cnt.waitForZero();
            break;
        }
        case "short":
            p.callFunction("bg_short");
            break;
        case "queue-error": {
            # the host writes to a queue whose error the sandboxed code set, in a thread of its own
            Queue eq();
            p.callFunction("set_queue_error", eq);
            background sub () { eq.push(1); }();
            break;
        }
        case "module":
            p.callFunction("bg_module");
            break;
        case "encodings":
            p.callFunction("bg_encodings");
            break;
        case "values":
            p.callFunction("bg_values");
            break;
        case "chained":
            p.callFunction("bg_chained");
            break;
        case "mark":
            p.callFunction("bg_mark");
            break;
        case "promise-error":
        case "async-error": {
            # the host gets the result of the future in a thread of its own and does not catch the error
            Future f = p.callFunction(mode == "promise-error" ? "promise_error" : "async_error");
            # the tenant's Program stays until the thread has ended, as the future's code runs in it
            Counter cnt(1);
            background sub () {
                on_exit cnt.dec();
                f.get();
            }();
            cnt.waitForZero();
            break;
        }
        case "queue-read": {
            # the host reads from a queue whose error the sandboxed code set, in a thread of its own
            Queue eq();
            p.callFunction("set_queue_error", eq);
            background eq.get();
            break;
        }
        case "parallel": {
            # the host runs the sandboxed code's closure with parallel_map() in a thread of its own; the tenant's Program
            # stays until the thread has ended
            code c = p.callFunction("map_closure");
            Counter cnt(1);
            background sub () {
                on_exit cnt.dec();
                # two elements and two threads: the closure runs in worker threads, whose exception is raised again
                parallel_map(c, (1, 2), 2);
            }();
            cnt.waitForZero();
            break;
        }
        # the host's own code: reported as before, without a mark
        case "host-timer-logger":
            set_async_io_logger(stdout_logger());
            AsyncIoController::addTimerDelay(0s, sub () {
                AsyncIoController::addTimerDelay(0s, sub () { q.push(1); });
                throw "HOST-ERR", "host line";
            });
            q.get(60s);
            break;
        case "host-timer-stderr":
            AsyncIoController::addTimerDelay(0s, sub () {
                AsyncIoController::addTimerDelay(0s, sub () { q.push(1); });
                throw "HOST-ERR", "host line";
            });
            q.get(60s);
            break;
        case "host-task":
            AsyncIoController::submitTask(sub () {
                AsyncIoController::submitTask(sub () { q.push(1); });
                throw "HOST-ERR", "host line";
            });
            q.get(60s);
            break;
        case "host-background":
            background sub () { throw "HOST-ERR", "host line"; }();
            break;
    }
    print("done\n");
}

main();
