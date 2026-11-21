%modern
%requires AsyncSocketIo
%requires HttpServerAsyncIo
# a callable that code restricted from thread control creates is not scheduled onto another thread, whichever code
# passes it on; a callable that the host creates is, also for an object that the host lends to restricted code; see
# thread-escape-callables.qtest, which runs this script in every execution mode and as a qcc executable (qore#7056)

# the host loads the helper module at run time, so that the restricted Programs use the host's copy, also in a qcc
# executable, which would embed a module that it requires by path instead of loading it
const ScriptDir = realpath(get_script_dir());
load_module(ScriptDir + "/ThreadEscapeHelper.qm");

const Restricted = PO_NEW_STYLE | PO_NO_THREAD_CONTROL | PO_NO_NETWORK | PO_NO_EXTERNAL_INFO;

#! parses the code in a new restricted Program, calls its function t() with the arguments, and prints the result
sub probe(string label, string code, *softlist<auto> args, int po = Restricted) {
    string result;
    try {
        Program p(po);
        p.parse("%modern\n%requires AsyncSocketIo\n%requires HttpServerAsyncIo\n%requires "
            + ScriptDir + "/ThreadEscapeHelper.qm\n" + code, label);
        result = p.callFunctionArgs("t", args);
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("%s: %s\n", label, result);
}

#! the result of code that a probe runs on another thread, or the exception that refused it
const RunOrRefuse = 'string sub run(code<nothing(Counter)> start) {
    Counter done(1);
    try {
        start(done);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return done.waitForZero(10s) ? "timeout" : "ran";
}
';

# the AsyncSocketIo wrapper passes a task and a timer callback to the builtin controller
probe("wrapper submitTask", RunOrRefuse + 'string sub t() {
    AsyncSocketIoController c();
    return run(sub (Counter done) { c.submitTask(sub () { done.dec(); }); });
}');
probe("wrapper addTimerDelay", RunOrRefuse + 'string sub t() {
    AsyncSocketIoController c();
    return run(sub (Counter done) { c.addTimerDelay(1ms, sub () { done.dec(); }); });
}');

# a controller that the host creates and lends: a restricted callable is refused, the host's is run
AsyncSocketIoController lent_controller();
# the host's factory creates the task in the host's code: the task is the host's callable
code make_host_task = code sub (Counter done) { return sub () { done.dec(); }; };
probe("lent controller, restricted task", RunOrRefuse + 'string sub t(AsyncSocketIoController c) {
    return run(sub (Counter done) { c.submitTask(sub () { done.dec(); }); });
}', lent_controller);
probe("lent controller, host task", RunOrRefuse + 'string sub t(AsyncSocketIoController c, code make_host_task) {
    return run(sub (Counter done) { c.submitTask(make_host_task(done)); });
}', (lent_controller, make_host_task));

# a ThreadPool that the host lends: code that may not control threads may use the thread classes, and so a pool of its
# own: the pool runs its code (qore#7056)
ThreadPool lent_pool(2);
probe("lent ThreadPool, restricted task", RunOrRefuse + 'string sub t(ThreadPool tp) {
    return run(sub (Counter done) { tp.submit(sub () { done.dec(); }); });
}', lent_pool);

# module code that runs a callable that it is given with the background operator
probe("module background", RunOrRefuse + 'string sub t() {
    return run(sub (Counter done) { ThreadEscapeHelper::runInBackground(sub () { done.dec(); }); });
}');

# the private helpers of the HTTP controller are internal: a subclass cannot call them
probe("subclass runOnThreadOfItsOwn", 'class X inherits HttpServerAsyncIo::HttpAsyncSocketIoController {
    constructor(AsyncSocketIoController c) : HttpAsyncSocketIoController(c) {}
    spawn(code<nothing()> c) { runOnThreadOfItsOwn(c); }
}
string sub t() { return "parsed"; }');
probe("subclass peer lookup", 'class X inherits HttpServerAsyncIo::HttpAsyncSocketIoController {
    constructor(AsyncSocketIoController c) : HttpAsyncSocketIoController(c) {}
    lookup() { pool_opts.peer_hostname_lookup = True; }
}
string sub t() { return "parsed"; }');

# the HTTP controller's handler pool runs the tasks of a queue of its own: a restricted task is refused there
probe("submitToHandlerPool", RunOrRefuse + 'class T inherits HttpServerAsyncIo::HttpAsyncTarget {
    hash<HttpServerAsyncIo::HttpRequestResultInfo> onHttpRequest(hash<HttpServerAsyncIo::HttpConnectionInfo> info) {
        return <HttpServerAsyncIo::HttpRequestResultInfo>{};
    }
    onIdleTimeout(Socket sock, *HttpServer::HttpListenerInterface listener, hash<auto> cx) {}
    onError(Socket sock, hash<ExceptionInfo> ex, *HttpServer::HttpListenerInterface listener, *hash<auto> cx) {}
    onSseDisconnect(Socket sock, *HttpServer::HttpListenerInterface listener, hash<auto> cx) {}
    onWebSocketFrame(Socket sock, hash<HttpServerAsyncIo::WebSocketFrameInfo> frame,
            *HttpServer::HttpListenerInterface listener, hash<auto> cx) {}
    onWebSocketClose(Socket sock, int code, string reason, *HttpServer::HttpListenerInterface listener,
            hash<auto> cx) {}
}
string sub t() {
    AsyncSocketIoController c();
    HttpAsyncSocketIoController hc(c);
    return run(sub (Counter done) { hc.submitToHandlerPool(new T(), sub () { done.dec(); }); });
}');

# a WebSocket operation that the host creates and lends: its done callback runs on the handler pool, so a restricted
# callable is refused; a frame callback is the capability of a lent operation
HttpServerAsyncIo::HttpWebSocketPollOperation lent_ws_op(new Socket());
probe("lent WebSocket op, setDoneCallback", 'string sub t(HttpServerAsyncIo::HttpWebSocketPollOperation op) {
    try {
        op.setDoneCallback(sub () {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "set";
}', lent_ws_op);
probe("lent WebSocket op, setFrameCallback", 'string sub t(HttpServerAsyncIo::HttpWebSocketPollOperation op) {
    try {
        op.setFrameCallback(sub (auto frame) {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "set";
}', lent_ws_op);
# an operation that restricted code creates from a socket that the host lends: its frame callback is refused
probe("own WebSocket op, setFrameCallback", 'string sub t(Socket sock) {
    try {
        HttpServerAsyncIo::HttpWebSocketPollOperation op(sock);
        op.setFrameCallback(sub (auto frame) {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "set";
}', new Socket());
# a delegating operation that restricted code creates with a completion callback of its own
probe("DelegatingPollOperation on_complete", 'string sub t(HttpServerAsyncIo::HttpWebSocketPollOperation op) {
    try {
        DelegatingPollOperation d(op, sub () {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "created";
}', lent_ws_op);

# a cancel callback of a task is run on a thread of the pool too
probe("lent ThreadPool, restricted cancel callback", 'string sub t(ThreadPool tp, code make_host_task) {
    try {
        tp.submit(make_host_task(new Counter(1)), sub () {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "submitted";
}', (lent_pool, make_host_task));

# module code that runs a callable that it is given with call_async() and parallel_map()
probe("module call_async", 'string sub t() {
    try {
        ThreadEscapeHelper::runAsync(sub () {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "ran";
}');
probe("module parallel_map", 'string sub t() {
    try {
        ThreadEscapeHelper::runParallelMap(int sub (int i) { return i; });
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "ran";
}');

# a poll operation with Qore code of restricted code, which module code submits: its code would run on the
# controller's workers
probe("own poll operation submitted", 'class Op inherits AbstractPollOperation {
    string getGoal() { return "done"; }
    string getState() { return "done"; }
    bool goalReached() { return True; }
    *hash<SocketPollInfo> continuePoll() {}
}
string sub t(Socket sock) {
    try {
        ThreadEscapeHelper::submitOperation(sock, new Op());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "submitted";
}', new Socket());

# Qore::check_thread_callable(): code of restricted code is refused, the host's passes
probe("check_thread_callable, restricted code", 'string sub t() {
    try {
        check_thread_callable(sub () {}, "probe");
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "passed";
}');
probe("check_thread_callable, host code", 'string sub t(code make_host_task) {
    try {
        check_thread_callable(make_host_task(new Counter(1)), "probe");
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "passed";
}', make_host_task);

# code that may not use the thread classes: module code that submits its closure to a pool of the module is refused
const NoThreadClasses = Restricted | PO_NO_THREAD_CLASSES;
# (the module waits for the pool: the restricted code cannot use a Counter, a thread class)
probe("module pool, no thread classes", 'string sub t() {
    try {
        ThreadEscapeHelper::runOnPool(sub () {});
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "ran";
}', NOTHING, NoThreadClasses);

# a closure that module code creates for restricted code is the module's capability: the module runs it on a thread
# of its own, which calls the code of its caller there
probe("module closure calling restricted code", RunOrRefuse + 'string sub t() {
    return run(sub (Counter done) { ThreadEscapeHelper::runWrapped(sub () { done.dec(); }); });
}');

lent_pool.stopWait();
