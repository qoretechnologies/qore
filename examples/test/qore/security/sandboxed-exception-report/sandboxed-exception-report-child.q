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
# the rendezvous of the initialization of ReportLoadFail.qm (qore#7232)
%requires ReportGate
%requires json
%requires yaml

Logger sub stdout_logger() {
    Logger logger("report", LoggerLevel::getLevelInfo());
    LoggerAppenderStdOut appender("out", new LoggerLayoutPattern("LOG: %m%n"));
    appender.open();
    logger.addAppender(appender);
    return logger;
}

#! the host's input stream whose read() raises
class HostBadInputStream inherits InputStream {
    *binary read(int limit) {
        throw "HOST-ERR", "host line";
    }
    int peek() {
        return -1;
    }
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
        case "pipeline": {
            # a pipeline of the sandboxed code fails in a C++ step on the async I/O thread; the host gets the result
            # of its future in a thread of its own (qore#7232)
            Socket listener();
            listener.bindINET("127.0.0.1", "0", True);
            listener.listen();
            Socket client();
            client.connect("127.0.0.1:" + listener.getSocketInfo().port);
            Socket server = listener.accept();
            # the promise and its future are the host's: no object of the sandboxed code is on the host's stack when
            # its result is got, so only the origin that the rejection recorded marks the report
            Promise pr();
            Future f = pr.getFuture();
            p.callFunction("pipeline_error", server, pr);
            Counter cnt(1);
            background sub () {
                on_exit cnt.dec();
                f.get();
            }();
            cnt.waitForZero();
            break;
        }
        case "json-stream":
        case "yaml-read":
        case "yaml-write": {
            # the host parses or writes through a stream of the sandboxed code, which raises (qore#7232)
            InputStream input = p.callFunction("bad_input");
            OutputStream output = p.callFunction("bad_output");
            Counter cnt(1);
            background sub () {
                on_exit cnt.dec();
                switch (mode) {
                    case "json-stream": {
                        JsonSaxParser j();
                        j.parseStream(input, sub (hash<auto> event) {});
                        break;
                    }
                    case "yaml-read": {
                        YamlSaxParser y();
                        y.parseStream(input, sub (hash<auto> event) {});
                        break;
                    }
                    case "yaml-write": {
                        YamlStreamWriter w(output);
                        w.startDocument();
                        w.writeScalar("value");
                        w.endDocument();
                        break;
                    }
                }
            }();
            cnt.waitForZero();
            break;
        }
        case "module-load": {
            # the sandboxed code loads a module by path, whose initialization fails with text of its choosing while a
            # host thread waits for the load of the same module (qore#7232)
            Counter done(2);
            background sub () {
                on_exit done.dec();
                try {
                    p.callFunction("load_path", get_script_dir() + "/ReportLoadFail.qm");
                } catch (hash<ExceptionInfo> ex) {
                    # the sandboxed code's own load: its failure is not reported here
                }
            }();
            ReportGate::Gate::started.waitForZero();
            # the host's thread waits for the load, and gets its failure
            background sub () {
                on_exit done.dec();
                load_module("ReportLoadFail");
            }();
            if (!dbg_module_load_waiters("ReportLoadFail", 1, 60000)) {
                throw "TEST-ERROR", "the host's thread did not wait for the load";
            }
            ReportGate::Gate::go.dec();
            done.waitForZero();
            break;
        }
        case "host-pipeline": {
            # the host's own pipeline fails in a C++ step on the I/O thread: its error is not marked (qore#7232)
            Socket listener();
            listener.bindINET("127.0.0.1", "0", True);
            listener.listen();
            Socket client();
            client.connect("127.0.0.1:" + listener.getSocketInfo().port);
            Socket server = listener.accept();
            PollPipeline inner();
            inner.addRequireContextKey("no-such-key", "HOST-ERR", "host line");
            PollPipeline outer(server);
            outer.addDelegate(inner);
            Promise pr();
            outer.setResultPromise(pr);
            AsyncIoController::submit(<SocketPollOperationInfo>{"sock": server, "spop": outer, "to": -1s,
                "owner": "host-pipeline"});
            Future f = pr.getFuture();
            background f.get();
            break;
        }
        case "host-json-stream": {
            # the host parses a stream of its own that raises: not marked (qore#7232)
            background sub () {
                JsonSaxParser j();
                j.parseStream(new HostBadInputStream(), sub (hash<auto> event) {});
            }();
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
