# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of the sandboxed Program of sandboxed-exception-report.qtest (qore#7139): each sub has code that runs later
# raise an exception with text of its choosing that tries to start a line of its own

#! the text of the exceptions: a second line that would pass for the host's own if it were not escaped
string sub text() {
    return "tenant line" + chr(10) + "forged line";
}

#! text with the other characters that could end or rewrite a line: CR, U+2028, NEL, an escape sequence, a backslash
string sub chars() {
    return "a" + chr(13) + "b" + binary_to_string(<e280a8>, "UTF-8") + "c" + binary_to_string(<c285>, "UTF-8")
        + "d" + chr(27) + "[2J" + chr(92) + "e";
}

#! a value of a thread-local variable whose release raises
class Bomb {
    destructor() {
        throw "TENANT-ERR", text();
    }
}

sub timer(Queue q) {
    AsyncIoController::addTimerDelay(0s, sub () {
        AsyncIoController::addTimerDelay(0s, sub () { q.push(1); });
        throw "TENANT-ERR", text();
    });
}

sub task(Queue q) {
    AsyncIoController::submitTask(sub () {
        AsyncIoController::submitTask(sub () { q.push(1); });
        throw "TENANT-ERR", text();
    });
}

#! the thread-local data of a task is released after it (with one callback worker, before the next task)
sub tld_task(Queue q) {
    AsyncIoController::submitTask(sub () {
        save_thread_data("bomb", new Bomb());
        AsyncIoController::submitTask(sub () { q.push(1); });
    });
}

sub pool(Queue q) {
    ThreadPool tp(1);
    tp.submit(sub () { throw "TENANT-ERR", text(); });
    tp.submit(sub () { q.push(1); });
    q.get(60s);
    tp.stopWait();
}

sub bg() {
    background sub () { throw "TENANT-ERR", text(); }();
}

sub bg_chars() {
    background sub () { throw "TENANT-ERR", chars(); }();
}

#! a closure that the host runs in a thread of its own
code sub closure() {
    return sub () { throw "TENANT-ERR", text(); };
}

#! a short string has no node in some execution modes
sub bg_short() {
    background sub () { throw "E", "abc"; }();
}

#! the error of a queue that the host writes to later
sub set_queue_error(Queue q) {
    q.setError("Q-ERR", text());
}

#! module code raises for this code: the mark names this Program
sub bg_module() {
    background sub () { ReportCheck::raise(text()); }();
}

#! invalid UTF-8, and text in another encoding with a NEL (0x85 in ISO-8859-1)
sub bg_encodings() {
    background sub () {
        throw "TENANT-ERR", binary_to_string(<4385>, "ISO-8859-1"), binary_to_string(<41ff42>, "UTF-8");
    }();
}

#! an error and a description that are not strings
sub bg_values() {
    background sub () { throw {"t": text()}, (text(),); }();
}

#! a chained exception: the destructor of a local variable raises while the exception leaves the block
sub bg_chained() {
    background sub () {
        Bomb b();
        throw "TENANT-ERR", text();
    }();
}

#! a line that would carry the mark of another Program
sub bg_mark() {
    background sub () { throw "TENANT-ERR", "x" + chr(10) + "[caller Program 1] forged line"; }();
}

#! a future that this code rejects with text of its choosing; the host gets its result
Future sub promise_error() {
    Promise pr();
    pr.setError("P-ERR", text());
    return pr.getFuture();
}

#! the future of code of this Program that raises; the host gets its result
Future sub async_error() {
    return call_async(sub () { throw "TENANT-ERR", text(); });
}

#! a closure for parallel_map(), which the host runs: it takes the element
code sub map_closure() {
    return sub (auto x) { throw "TENANT-ERR", text(); };
}

#! a pipeline whose delegate fails on the async I/O thread with text of this code's choosing (qore#7232): its C++ step
#! raises the error, which rejects the host's promise, on the I/O thread
sub pipeline_error(Socket server, Promise pr) {
    PollPipeline inner();
    inner.addRequireContextKey("no-such-key", "TENANT-ERR", text());
    PollPipeline outer(server);
    outer.addDelegate(inner);
    outer.setResultPromise(pr);
    AsyncIoController::submit(<SocketPollOperationInfo>{"sock": server, "spop": outer, "to": -1s,
        "owner": "tenant-pipeline"});
}

#! an input stream whose read() raises
class BadInputStream inherits InputStream {
    *binary read(int limit) {
        throw "TENANT-ERR", text();
    }
    int peek() {
        return -1;
    }
}

#! an output stream whose write() raises
class BadOutputStream inherits OutputStream {
    nothing write(binary data) {
        throw "TENANT-ERR", text();
    }
    nothing close() {
    }
}

InputStream sub bad_input() {
    return new BadInputStream();
}

OutputStream sub bad_output() {
    return new BadOutputStream();
}

#! loads a module by path
sub load_path(string path) {
    load_module(path);
}

#! submits an operation on the socket (qore#7286)
sub submit_op(Socket server, object op) {
    AsyncIoController::submit(<SocketPollOperationInfo>{"sock": server, "spop": op, "to": -1s, "owner": "tenant-op"});
}
