# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WebSocketTransport class definition

/** WebSocketTransport.qc Copyright 2025 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the AsyncApiDataProvider module
public namespace AsyncApiDataProvider {

#! WebSocket transport implementation for AsyncAPI
/** This class provides WebSocket transport for AsyncAPI message operations.

    Uses WebSocketClientWithSerialization for built-in JSON/YAML serialization support.
*/
public class WebSocketTransport inherits AbstractAsyncApiTransport {
    private {
        #! The WebSocket client with serialization support
        WebSocketClient::WebSocketClientWithSerialization wsc;

        #! Connection status
        bool connected = False;
    }

    #! Creates the transport with an existing WebSocket client
    /** @param wsc the WebSocket client to use
    */
    constructor(WebSocketClient::WebSocketClientWithSerialization wsc) {
        self.wsc = wsc;
    }

    #! Creates the transport with connection options
    /** @param opts WebSocket client options including 'url'

        The following options are supported in addition to standard HTTP client options:
        - \c data: serialization format: \c "json" (default), \c "yaml", or \c "none"
        - \c url: the WebSocket URL to connect to
    */
    constructor(hash<auto> opts) {
        # Default to JSON serialization if not specified
        if (!opts.hasKey("data")) {
            opts.data = "json";
        }
        wsc = new WebSocketClientWithSerialization(opts);
    }

    #! Sends a message via WebSocket
    /** @param channel the channel (not used for WebSocket - messages go to the connected endpoint)
        @param msg the message data to send
        @param headers optional headers (not used for WebSocket)

        @note Messages are automatically serialized according to the \c data option (default: JSON).
    */
    send(string channel, auto msg, *hash<auto> headers) {
        if (!connected) {
            throw "TRANSPORT-ERROR", "WebSocket transport is not connected";
        }
        wsc.sendData(msg);
    }

    #! Returns the protocol name
    /** @return "ws" or "wss" depending on the URL
    */
    string getProtocol() {
        string url = wsc.getUrl();
        if (url =~ /^wss:/i) {
            return "wss";
        }
        return "ws";
    }

    #! Returns True if connected
    bool isConnected() {
        return connected;
    }

    #! Connects the WebSocket client
    connect() {
        if (!connected) {
            wsc.connect();
            connected = True;
        }
    }

    #! Disconnects the WebSocket client
    disconnect() {
        if (connected) {
            wsc.disconnect();
            connected = False;
        }
    }

    #! Sets a logger for the transport
    setLogger(*LoggerInterface logger) {
        wsc.setLogger(logger);
    }

    #! Returns the underlying WebSocket client
    /** @return the WebSocket client object
    */
    WebSocketClient::WebSocketClientWithSerialization getWebSocketClient() {
        return wsc;
    }
}

}
