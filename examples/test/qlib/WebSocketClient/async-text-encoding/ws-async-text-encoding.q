#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# helper of WebSocketAsyncTextEncoding.qtest: run with a default encoding other than UTF-8 (--charset=ISO-8859-1); it
# receives the text messages of a WebSocket server with WebSocketClient, and prints the encoding and the UTF-8 bytes of
# each one, one per line

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires Util
%requires Logger
%requires HttpServerUtil
%requires HttpServer
%requires WebSocketUtil
%requires WebSocketHandler
%requires WebSocketClient
%requires AsyncSocketIo

# the messages in UTF-8, given as bytes: the source is read in the default encoding of the process
const Messages = (
    binary_to_string(<6772c3b6c39f6520e282ac>, "UTF-8"),
    "ASCII",
    binary_to_string(<e697a5e69cace8aa9e>, "UTF-8"),
);

class SendWsConnection inherits WebSocketConnection {
    constructor(WebSocketHandler handler) : WebSocketConnection(handler) {
    }

    gotMessage(string msg) {
        # the client asks for the messages; each is sent in UTF-8, whatever the default encoding of this process
        foreach string m in (Messages) {
            send(m);
        }
    }
}

class SendWsHandler inherits WebSocketHandler {
    WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new SendWsConnection(self);
    }
}

HttpServer server(<HttpServerOptionInfo>{"logger": new Logger("ws-async-text-encoding", LoggerLevel::getLevelInfo())});
SendWsHandler handler();
server.setHandler("ws", "", NOTHING, handler);
server.setDefaultHandler("ws", handler);
int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
on_exit {
    server.stop();
    AsyncSocketIo::AsyncSocketIoController ctrl = AsyncSocketIo::getGlobalAsyncIoController();
    ctrl.stopClear();
}

Queue q();
WebSocketClient wsc(sub (auto msg) { q.push(msg); }, {"url": "ws://127.0.0.1:" + port});
wsc.connect();
on_exit wsc.disconnect();
wsc.send("go");
for (int i = 0; i < Messages.size(); ++i) {
    auto msg = q.get(30s);
    printf("%s %s %s\n", msg.type(), msg.typeCode() == NT_STRING ? msg.encoding() : "-",
        msg.typeCode() == NT_STRING ? binary(convert_encoding(msg, "UTF-8")).toHex() : binary(msg).toHex());
}
printf("default %s\n", get_default_encoding());
