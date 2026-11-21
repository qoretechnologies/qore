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

# the same with an argument that the new thread computes (a background expression that the compiled execution modes
# run with a helper)
probe("module background, computed argument", RunOrRefuse + 'string sub t() {
    return run(sub (Counter done) { ThreadEscapeHelper::runInBackgroundWithArg(sub (int x) { done.dec(); }, 1); });
}');

# the callee is module code that the new thread runs with arguments that it computes, one of which is a closure of the
# restricted code: the module calls it there, as for a closure of the module that calls it (qore#7087); a compiled
# module, whose code does not give the callee, refuses it (see the test)
probe("module background, module function with a closure argument", RunOrRefuse + 'string sub t() {
    return run(sub (Counter done) {
        ThreadEscapeHelper::runModuleFunctionWithArgs(sub (int x) { done.dec(); }, 1);
    });
}');

# a call reference of the restricted code as the callee, with an argument that the new thread computes
probe("module background, call reference with a computed argument", RunOrRefuse + 'Counter done(1);
sub dec(int x) { done.dec(); }
string sub t() {
    return run(sub (Counter d) { ThreadEscapeHelper::runInBackgroundWithArg(\dec(), 1); });
}');

# a method of an object of a class of the restricted code, called by module code on a new thread
const ObjClass = 'class O {
    private { Counter done; }
    constructor(Counter done) { self.done = done; }
    run() { done.dec(); }
    runWith(int x) { done.dec(); }
}
';
probe("module background, method of an own object", RunOrRefuse + ObjClass + 'string sub t() {
    return run(sub (Counter done) { ThreadEscapeHelper::runMethodInBackground(new O(done)); });
}');
probe("module background, method of an own object with a computed argument", RunOrRefuse + ObjClass + 'string sub t() {
    return run(sub (Counter done) { ThreadEscapeHelper::runMethodInBackgroundWithArg(new O(done), 1); });
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

# a WebSocket operation that the host creates and lends: its done callback runs on the handler pool and its frame
# callback on the controller's workers, outside the sandbox of the restricted code, so a restricted callable is
# refused for both
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

# a delegating operation over an operation of restricted code, or with a counter of a class of restricted code: their
# methods run on the controller's workers
const OpClass = 'class Op inherits AbstractPollOperation {
    string getGoal() { return "done"; }
    string getState() { return "done"; }
    bool goalReached() { return True; }
    *hash<SocketPollInfo> continuePoll() {}
}
';
probe("DelegatingPollOperation over an own operation", OpClass + 'string sub t() {
    try {
        DelegatingPollOperation d(new Op());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "created";
}');
probe("DelegatingPollOperation with an own counter", 'class C inherits Counter {
    constructor() : Counter(1) {}
    int dec() { return Counter::dec(); }
}
string sub t(HttpServerAsyncIo::HttpWebSocketPollOperation op) {
    try {
        DelegatingPollOperation d(op, NOTHING, new C());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "created";
}', lent_ws_op);

# AsyncIoController::exec() runs the operation too
probe("own poll operation executed", OpClass + 'string sub t(Socket sock) {
    try {
        ThreadEscapeHelper::execOperation(sock, new Op());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "executed";
}', new Socket());

# the dispatch hooks of a target of restricted code run on the handler pool around any body
probe("submitToHandlerPool, own target", 'class T inherits HttpServerAsyncIo::HttpAsyncTarget {
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
string sub t(code make_host_task) {
    AsyncSocketIoController c();
    HttpAsyncSocketIoController hc(c);
    try {
        hc.submitToHandlerPool(new T(), make_host_task(new Counter(1)));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "submitted";
}', make_host_task);

# a restricted subclass of a module class runs the module's code: the operations that the module creates are the
# module's, also when it creates them in a method of an object of the subclass
probe("restricted subclass, module operation", 'class S inherits ThreadEscapeHelper::OpSubmitter {}
string sub t(Socket sock) {
    return (new S()).submitOwn(sock);
}', new Socket());

lent_pool.stopWait();
