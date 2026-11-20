# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of a sandboxed service restricted from thread control that loads WebSocketHandler first; parsed by the
# child of WebSocketHandlerRestrictedTimers.qtest

%requires WebSocketHandler
%requires WebSocketHandlerTimerFixture
%requires WebSocketHandlerUndeclaredTimerFixture

#! a handler that closes an idle stream after a second
class TestHandler inherits WebSocketHandler {
    constructor() : WebSocketHandler(NOTHING, {"idle_timeout": 1}) {
    }
}

#! returns the result of the code, or the code of the exception that it raises
string sub result(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! starts an HTTP/2 WebSocket stream of the handler; returns "ended" when the idle timer of the stream has ended it
string sub stream_idle_timeout() {
    TestHandler h();
    Queue ended();
    hash<auto> cx = {
        "id": 1,
        "header-info": {
            "http2": True,
            "is_websocket_connect": True,
            "stream_id": 1,
            "stream_data_queue": new Queue(),
            "register_stream_listener": sub (object listener) {},
            # the server's end of the stream, which the handler runs when the stream ends
            "ws_end": sub () { ended.push("ended"); },
            "ws_submit": sub (code<nothing()> end) { end(); },
            "ws_accept_end": sub () {},
        },
        "peer-info": {"hostname_desc": "test", "port": 1},
    };
    hash<auto> hdr = {"method": "CONNECT", ":protocol": "websocket"};
    # the handler arms the heartbeat and idle timers of the stream and returns at once
    if (!h.startImpl("1", cx, hdr, new Socket())) {
        return "not externally managed";
    }
    return ended.get(60000);
}

list<string> sub test() {
    return (
        "WebSocketHandler stream idle timer: " + result(\stream_idle_timeout()),
        "declared own timer fired: " + result(auto sub () {
            WebSocketHandlerTimerFixture::ModuleTimer t(0s);
            return t.waitFired(60s);
        }),
        "declared own timer canceled: " + result(auto sub () {
            WebSocketHandlerTimerFixture::ModuleTimer t(1h);
            return t.cancel();
        }),
        "declared caller closure: " + result(auto sub () {
            int id = WebSocketHandlerTimerFixture::arm_with(1h, sub () {});
            # a timer that was armed is not left pending
            WebSocketHandlerTimerFixture::cancel_timer(id);
            return id;
        }),
        "undeclared own timer: " + result(\WebSocketHandlerUndeclaredTimerFixture::own_timer()),
    );
}
