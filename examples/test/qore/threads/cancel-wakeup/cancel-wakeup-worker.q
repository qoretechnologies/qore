#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# cancels threads blocked in each kind of wait and reports how long each wait took to end after the request
# usage: cancel-wakeup-worker.q <tmpdir> <file with a TLS certificate and private key>
# output: one line per case: "<case>|<exception>|<longest latency in microseconds>|<detail>"

%modern
%no-child-restrictions

# the number of times each case is run; the longest latency is reported
const Trials = 3;

string tmpdir = ARGV[0];
string tls_pem = ReadOnlyFile::readTextFile(ARGV[1]);
int fifo_seq = 0;

# creates a FIFO that a child process writes to once it is running: opening it for reading blocks until there is a
# writer, so reading it tells the test that the child has started
string sub make_fifo() {
    string path = sprintf("%s/fifo-%d", tmpdir, ++fifo_seq);
    if (mkfifo(path)) {
        throw "FIFO-ERROR", sprintf("mkfifo(%y): %s", path, strerror());
    }
    return path;
}

# waits until the child process started by a case has written to its FIFO
sub wait_fifo(string path) {
    ReadOnlyFile f(path);
    f.readBinary(1);
}

# runs a blocking call in a new thread, waits until the call is about to block (or has blocked), requests
# cancellation, and returns the exception that ended the call and the time from the request to the end of the call
hash<auto> sub cancel_one(code blocking, code wait_blocked, code cancel) {
    Queue started();
    Queue done();
    int tid = background sub () {
        hash<auto> rv;
        try {
            started.push(True);
            rv.detail = blocking();
            rv.err = "NONE";
        } catch (hash<ExceptionInfo> ex) {
            rv.err = ex.err;
            rv.detail = ex.arg;
        }
        rv.end = clock_get_monotonic_us();
        done.push(rv);
    }();
    started.get();
    wait_blocked();
    int start = clock_get_monotonic_us();
    cancel(tid);
    hash<auto> rv = done.get();
    rv.us = rv.end - start;
    return rv;
}

sub cancel_tid(int tid) {
    cancel_thread(tid, "cancel-wakeup test");
}

sub nothing_to_wait_for() {
}

# runs a case Trials times and prints its line
sub run_case(string name, code blocking, code wait_blocked = \nothing_to_wait_for(), code cancel = \cancel_tid()) {
    *string err;
    int max_us = 0;
    string detail = "";
    for (int i = 0; i < Trials; ++i) {
        hash<auto> rv = cancel_one(blocking, wait_blocked, cancel);
        if (exists err && rv.err != err) {
            err = sprintf("%s/%s", err, rv.err);
        } else {
            err = rv.err;
        }
        if (rv.us > max_us) {
            max_us = rv.us;
        }
        if (exists rv.detail) {
            detail = sprintf("%y", rv.detail);
        }
    }
    printf("%s|%s|%d|%s\n", name, err, max_us, detail);
    flush();
}

# a connected socket pair; nothing is ever sent from the server side
hash<auto> sub socket_pair(bool tls) {
    Socket listener();
    listener.bind("127.0.0.1:0", True);
    listener.listen();
    on_exit listener.close();
    if (tls) {
        listener.setCertificate(tls_pem);
        listener.setPrivateKey(tls_pem);
    }
    int port = listener.getSocketInfo().port;
    Queue accepted();
    background sub () {
        try {
            accepted.push(tls ? listener.acceptSSL(15s) : listener.accept(15s));
        } catch (hash<ExceptionInfo> ex) {
            accepted.push(ex);
        }
    }();
    Socket client();
    string target = sprintf("127.0.0.1:%d", port);
    if (tls) {
        client.connectSSL(target, 15s);
    } else {
        client.connect(target, 15s);
    }
    auto a = accepted.get(15s);
    if (!(a instanceof Socket)) {
        throw a.err, a.desc;
    }
    return {"client": client, "server": a};
}

# a pipe read: nothing is ever written to the pipe
run_case("pipe read", auto sub () {
    hash<PipeInfo> p = File::getPipe();
    return p.read.readBinary(1);
});

# a backquote read: the child writes nothing to its output before it is killed
{
    string fifo = make_fifo();
    run_case("backquote read", auto sub () {
        return backquote(sprintf("printf x > '%s'; exec sleep 60", fifo));
    }, sub () { wait_fifo(fifo); });
}

# a process wait: the child never exits by itself
{
    string fifo = make_fifo();
    run_case("system wait", auto sub () {
        return system(sprintf("printf x > '%s'; exec sleep 60", fifo));
    }, sub () { wait_fifo(fifo); });
}

# a StreamPipe read: nothing is ever written to the pipe
run_case("stream pipe read", auto sub () {
    StreamPipe sp();
    return sp.getInputStream().read(1);
});

# a socket read and a TLS socket read: the peer never sends anything; the connections are made before the case runs
{
    list<hash<auto>> pairs = map socket_pair(False), xrange(Trials);
    int idx = 0;
    run_case("socket read", auto sub () {
        return pairs[idx++].client.recv(1);
    });
    list<hash<auto>> tls_pairs = map socket_pair(True), xrange(Trials);
    idx = 0;
    run_case("tls socket read", auto sub () {
        return tls_pairs[idx++].client.recv(1);
    });
    map ($1.client.close(), $1.server.close()), pairs + tls_pairs;
}

# sleep() and usleep()
run_case("sleep", auto sub () {
    return sleep(60);
});
run_case("usleep", auto sub () {
    return usleep(60s);
});

# lock waits: the lock is held by this thread for the whole case
{
    Mutex m();
    m.lock();
    run_case("mutex lock", auto sub () { m.lock(); });
    run_case("mutex lock timeout", auto sub () { return m.lock(60s); });
    run_case("auto lock", auto sub () { AutoLock al(m); });
    m.unlock();

    RWLock rw();
    rw.writeLock();
    run_case("read lock", auto sub () { rw.readLock(); });
    run_case("read lock timeout", auto sub () { return rw.readLock(60s); });
    rw.writeUnlock();
    rw.readLock();
    run_case("write lock", auto sub () { rw.writeLock(); });
    run_case("write lock timeout", auto sub () { return rw.writeLock(60s); });
    rw.readUnlock();

    Gate g();
    g.enter();
    run_case("gate enter", auto sub () { g.enter(); });
    run_case("gate enter timeout", auto sub () { return g.enter(60s); });
    g.exit();
}

# a condition wait: the lock is held again when the cancellation is raised
{
    Mutex m();
    Condition c();
    run_case("condition wait", auto sub () {
        m.lock();
        on_exit m.unlock();
        try {
            c.wait(m);
        } catch (hash<ExceptionInfo> ex) {
            # the wait ends with the lock held
            if (!m.lockOwner()) {
                throw "LOCK-NOT-HELD", "the lock is not held after the cancelled wait";
            }
            rethrow;
        }
    });
}

# a counter, queue and semaphore wait
{
    Counter cnt(1);
    run_case("counter wait", auto sub () { cnt.waitForZero(); });
    Queue q();
    run_case("queue get", auto sub () { return q.get(); });
    Semaphore sem(0);
    run_case("auto semaphore", auto sub () { AutoSemaphore as(sem); });
}

# a channel select wait, with and without a timeout
{
    Channel ch();
    run_case("channel select", auto sub () {
        return channel_select(({"channel": ch, "op": "recv"},));
    });
    run_case("channel select timeout", auto sub () {
        return channel_select(({"channel": ch, "op": "recv"},), 60s);
    });
}

# a Datasource transaction lock wait: this thread holds the lock for the whole case
{
    *Datasource ds;
    try {
        ds = new Datasource("dbitest:u/p@db");
        ds.setAutoCommit(False);
        ds.open();
        ds.beginTransaction();
    } catch (hash<ExceptionInfo> ex) {
        printf("datasource lock|SKIP|0|%s\n", ex.err);
        remove ds;
    }
    if (ds) {
        run_case("datasource lock", auto sub () { return ds.exec("select 1"); });
        ds.rollback();
    }
}

# a program interrupt during a backquote read in a sandboxed Program
{
    string fifo = make_fifo();
    SandboxManager sm();
    Program p(PO_NEW_STYLE);
    p.setSandboxManager(sm);
    p.parse("string sub run(string cmd) { return backquote(cmd); }", "cancel-wakeup-interrupt");
    run_case("interrupted backquote read", auto sub () {
        # the previous trial's interrupt has ended its call; this trial starts without it
        sm.clearInterrupt();
        return p.callFunction("run", sprintf("printf x > '%s'; exec sleep 60", fifo));
    }, sub () { wait_fifo(fifo); }, sub (int tid) {
        sm.requestInterrupt();
    });
}

# a cancellation requested during a deferral is delivered only after the deferred wait has completed normally
{
    string fifo = make_fifo();
    run_case("deferred backquote read", auto sub () {
        string out = defer_thread_cancel(string sub () {
            return backquote(sprintf("printf x > '%s'; sleep 0.2; printf done", fifo));
        });
        # the next cancellation point raises the request; the output of the deferred call is reported with it
        try {
            usleep(1);
        } catch (hash<ExceptionInfo> ex) {
            throw ex.err, ex.desc, out;
        }
        return out;
    }, sub () { wait_fifo(fifo); });
}

# after a cancelled wait and clear_thread_cancel(), the next wait is not woken by the earlier request
{
    Queue ready();
    run_case("wait after cleared cancel", auto sub () {
        hash<PipeInfo> p = File::getPipe();
        try {
            # the request is made once the thread is in this block, so that this read is what it cancels
            ready.push(True);
            p.read.readBinary(1);
        } catch (hash<ExceptionInfo> ex) {
            clear_thread_cancel();
        }
        int start = clock_get_monotonic_us();
        # a read with a timeout must time out after the whole timeout
        try {
            p.read.readBinary(1, 200);
        } catch (hash<ExceptionInfo> ex) {
            int us = clock_get_monotonic_us() - start;
            throw "AFTER-CLEAR", ex.err, us >= 200000;
        }
    }, sub () { ready.get(); });
}
