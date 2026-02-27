# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProvider debug info collector classes

/** DataProviderDebugCollector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {

#! Debug info operation types
/** @since DataProvider 3.3
*/
public const DPDI_OP_REQUEST = "request";           #!< HTTP/REST request operation
public const DPDI_OP_RESPONSE = "response";         #!< HTTP/REST response operation
public const DPDI_OP_CONNECT = "connect";           #!< Connection established
public const DPDI_OP_DISCONNECT = "disconnect";     #!< Connection closed
public const DPDI_OP_SEND = "send";                 #!< Message sent (WebSocket, etc.)
public const DPDI_OP_RECEIVE = "receive";           #!< Message received (WebSocket, etc.)

#! Data provider debug info structure
/** This hashdecl provides technical debugging information about data provider operations.

    @since DataProvider 3.3
*/
public hashdecl DataProviderDebugInfo {
    #! Operation type (e.g., "request", "response", "connect", "send", "receive")
    string operation;

    #! Timestamp when the operation started
    date start_time;

    #! Timestamp when the operation completed
    *date end_time;

    #! Duration in milliseconds
    *int duration_ms;

    #! Target URL if applicable
    *string url;

    #! HTTP method if applicable (GET, POST, PUT, DELETE, etc.)
    *string method;

    #! HTTP protocol version (e.g., "HTTP/1.1", "HTTP/2")
    *string protocol;

    #! HTTP status code for responses
    *int status_code;

    #! HTTP status message for responses
    *string status_message;

    #! Request headers
    *hash<auto> request_headers;

    #! Response headers
    *hash<auto> response_headers;

    #! Request body (may be truncated for large payloads)
    *data request_body;

    #! Response body (may be truncated for large payloads)
    *data response_body;

    #! Content type of request
    *string request_content_type;

    #! Content type of response
    *string response_content_type;

    #! Request body size in bytes
    *int request_size;

    #! Response body size in bytes
    *int response_size;

    #! WebSocket opcode if applicable
    *int ws_opcode;

    #! WebSocket message type (text, binary, ping, pong, close)
    *string ws_message_type;

    #! True if this is an HTTP/2 connection
    *bool http2;

    #! True if this is an HTTP/3 connection
    *bool http3;

    #! HTTP/2 stream ID if applicable
    *int h2_stream_id;

    #! True if TLS/SSL is used
    *bool tls;

    #! TLS version if applicable
    *string tls_version;

    #! Remote peer information
    *hash<auto> peer_info;

    #! Local socket information
    *hash<auto> socket_info;

    #! Any error information
    *string error;

    #! Additional provider-specific debug info
    *hash<auto> info;
}

#! Abstract base class for debug info collection
/** Data providers call the @ref DataProvider::AbstractDebugInfoCollector::collect method with debug info for each operation.
    Implementations can store, log, or process this information as needed.

    @since DataProvider 3.3
*/
public class AbstractDebugInfoCollector {
    #! Called by data provider with debug info for each operation
    /** @param info the debug information for the operation

        This method is called by data providers when debug collection is enabled.
        Implementations should handle the debug info appropriately (store, log, etc.).
    */
    abstract collect(hash<DataProviderDebugInfo> info);

    #! Called when the data provider is done (disconnect, cleanup, etc.)
    /** Implementations can override this to perform any final processing.
    */
    flush() {
        # default implementation does nothing
    }

    #! Called to clear any collected data
    /** Implementations can override this to clear internal buffers.
    */
    clear() {
        # default implementation does nothing
    }
}

#! Buffered debug info collector - stores info for later retrieval
/** This collector stores debug info in memory for later retrieval and analysis.

    @par Example:
    @code{.py}
BufferedDebugInfoCollector collector();
provider.setDebugCollector(collector);

# Make some requests
auto response = provider.doRequest(request);

# Retrieve collected debug info
list<hash<DataProviderDebugInfo>> info = collector.getAll();
foreach hash<DataProviderDebugInfo> debug in (info) {
    printf("%s %s -> %d (%dms)\n", debug.method, debug.url, debug.status_code, debug.duration_ms);
}
    @endcode

    @since DataProvider 3.3
*/
public class BufferedDebugInfoCollector inherits AbstractDebugInfoCollector {
    private {
        #! Buffer of collected debug info
        list<hash<DataProviderDebugInfo>> buffer();

        #! Mutex for thread safety
        Mutex m();

        #! Maximum buffer size (0 = unlimited)
        int max_size = 0;
    }

    #! Creates the collector with optional maximum buffer size
    /** @param max_size maximum number of entries to keep (0 = unlimited)

        When max_size is reached, oldest entries are removed to make room for new ones.
    */
    constructor(int max_size = 0) {
        self.max_size = max_size;
    }

    #! Collects debug info into the buffer
    collect(hash<DataProviderDebugInfo> info) {
        m.lock();
        on_exit m.unlock();

        if (max_size > 0 && buffer.size() >= max_size) {
            shift buffer;
        }
        buffer += info;
    }

    #! Returns all collected debug info
    /** @return list of all collected debug info entries
    */
    list<hash<DataProviderDebugInfo>> getAll() {
        m.lock();
        on_exit m.unlock();

        return buffer;
    }

    #! Returns the last N collected debug info entries
    /** @param count number of entries to return
        @return list of the last N debug info entries
    */
    list<hash<DataProviderDebugInfo>> getLast(int count = 1) {
        m.lock();
        on_exit m.unlock();

        if (count >= buffer.size()) {
            return buffer;
        }
        return buffer[(buffer.size() - count)..];
    }

    #! Returns the number of collected entries
    /** @return number of entries in the buffer
    */
    int size() {
        m.lock();
        on_exit m.unlock();

        return buffer.size();
    }

    #! Clears the buffer
    clear() {
        m.lock();
        on_exit m.unlock();

        buffer = ();
    }
}

#! Console debug info collector - outputs debug info immediately
/** This collector outputs debug info to the console immediately as operations occur.
    Useful for interactive debugging and CLI tools like qdp.

    @par Example:
    @code{.py}
ConsoleDebugInfoCollector collector(1);  # verbosity level 1
provider.setDebugCollector(collector);

# Make requests - debug info is printed to console
auto response = provider.doRequest(request);
    @endcode

    @since DataProvider 3.3
*/
public class ConsoleDebugInfoCollector inherits AbstractDebugInfoCollector {
    private {
        #! Verbosity level (1 = basic, 2 = headers, 3 = full with bodies)
        int verbosity;

        #! Maximum body size to display
        int max_body_size = 1024;

        #! Output file (default: stdout)
        Qore::File output;

        #! Optional color helper
        *Util::TerminalColor color;
    }

    #! Creates the collector with verbosity level
    /** @param verbosity verbosity level:
        - 1: Basic info (method, URL, status, duration)
        - 2: Include headers
        - 3: Include request/response bodies (truncated)
        @param color optional terminal color helper for colorized output
        @param output output file (default: stdout)
    */
    constructor(int verbosity = 1, *Util::TerminalColor color, *Qore::File output) {
        self.verbosity = verbosity;
        self.color = color;
        self.output = output ?? stdout;
    }

    #! Sets the maximum body size to display
    setMaxBodySize(int size) {
        max_body_size = size;
    }

    #! Collects and immediately outputs debug info
    collect(hash<DataProviderDebugInfo> info) {
        string msg = formatDebugInfo(info);
        output.print(msg);
        output.sync();
    }

    #! Formats debug info for display
    private string formatDebugInfo(hash<DataProviderDebugInfo> info) {
        string result = "";

        # Timestamp
        string timestamp = info.start_time.format("YYYY-MM-DD HH:mm:SS.xx");
        result += color ? color.dim(timestamp) : timestamp;
        result += " ";

        # Operation-specific formatting
        switch (info.operation) {
            case DPDI_OP_REQUEST:
            case DPDI_OP_RESPONSE:
                result += formatHttpInfo(info);
                break;
            case DPDI_OP_CONNECT:
                result += formatConnectInfo(info);
                break;
            case DPDI_OP_DISCONNECT:
                result += formatDisconnectInfo(info);
                break;
            case DPDI_OP_SEND:
            case DPDI_OP_RECEIVE:
                result += formatMessageInfo(info);
                break;
            default:
                result += formatGenericInfo(info);
        }

        return result + "\n";
    }

    #! Formats HTTP request/response info
    private string formatHttpInfo(hash<DataProviderDebugInfo> info) {
        string result = "";

        # Direction indicator
        string direction = info.operation == DPDI_OP_REQUEST ? "→" : "←";
        result += color ? color.cyan(direction) : direction;
        result += " ";

        # Method and URL
        if (info.method) {
            result += color ? color.bold(info.method) : info.method;
            result += " ";
        }
        if (info.url) {
            result += color ? color.key(info.url) : info.url;
            result += " ";
        }

        # Status code for responses
        if (info.status_code) {
            string status = sprintf("%d", info.status_code);
            if (info.status_message) {
                status += " " + info.status_message;
            }
            if (color) {
                result += info.status_code >= 400 ? color.error(status) : color.success(status);
            } else {
                result += status;
            }
            result += " ";
        }

        # Protocol info
        if (info.protocol) {
            result += color ? color.dim("(" + info.protocol + ")") : "(" + info.protocol + ")";
            result += " ";
        }

        # Duration
        if (info.duration_ms) {
            string duration = sprintf("%dms", info.duration_ms);
            result += color ? color.dim(duration) : duration;
        }

        # Headers at verbosity 2+
        if (verbosity >= 2) {
            if (info.request_headers) {
                result += "\n  " + (color ? color.dim("Request Headers:") : "Request Headers:");
                result += formatHeaders(info.request_headers, "    ");
            }
            if (info.response_headers) {
                result += "\n  " + (color ? color.dim("Response Headers:") : "Response Headers:");
                result += formatHeaders(info.response_headers, "    ");
            }
        }

        # Bodies at verbosity 3+
        if (verbosity >= 3) {
            if (info.request_body) {
                result += "\n  " + (color ? color.dim("Request Body:") : "Request Body:");
                result += "\n" + formatBody(info.request_body, info.request_content_type);
            }
            if (info.response_body) {
                result += "\n  " + (color ? color.dim("Response Body:") : "Response Body:");
                result += "\n" + formatBody(info.response_body, info.response_content_type);
            }
        }

        # Error info
        if (info.error) {
            result += "\n  " + (color ? color.error("Error: " + info.error) : "Error: " + info.error);
        }

        return result;
    }

    #! Formats connection info
    private string formatConnectInfo(hash<DataProviderDebugInfo> info) {
        string result = color ? color.success("CONNECTED") : "CONNECTED";

        if (info.url) {
            result += " to " + (color ? color.key(info.url) : info.url);
        }

        if (info.protocol) {
            result += " (" + info.protocol + ")";
        }

        if (info.http2) {
            result += " [HTTP/2]";
        }

        if (info.tls) {
            result += " [TLS";
            if (info.tls_version) {
                result += " " + info.tls_version;
            }
            result += "]";
        }

        if (info.duration_ms) {
            result += " " + (color ? color.dim(sprintf("%dms", info.duration_ms)) : sprintf("%dms", info.duration_ms));
        }

        return result;
    }

    #! Formats disconnection info
    private string formatDisconnectInfo(hash<DataProviderDebugInfo> info) {
        string result = color ? color.warning("DISCONNECTED") : "DISCONNECTED";

        if (info.url) {
            result += " from " + info.url;
        }

        if (info.error) {
            result += " - " + (color ? color.error(info.error) : info.error);
        }

        return result;
    }

    #! Formats WebSocket/message info
    private string formatMessageInfo(hash<DataProviderDebugInfo> info) {
        string direction = info.operation == DPDI_OP_SEND ? "→" : "←";
        string result = color ? color.cyan(direction) : direction;
        result += " ";

        string op = info.operation == DPDI_OP_SEND ? "SEND" : "RECV";
        result += color ? color.bold(op) : op;
        result += " ";

        if (info.ws_message_type) {
            result += "[" + info.ws_message_type + "] ";
        }

        if (info.request_size || info.response_size) {
            int size = info.request_size ?? info.response_size;
            result += sprintf("%d bytes ", size);
        }

        if (info.duration_ms) {
            result += color ? color.dim(sprintf("%dms", info.duration_ms)) : sprintf("%dms", info.duration_ms);
        }

        # Show body at verbosity 2+
        if (verbosity >= 2) {
            data body = info.request_body ?? info.response_body;
            if (body) {
                result += "\n" + formatBody(body, info.request_content_type ?? info.response_content_type);
            }
        }

        return result;
    }

    #! Formats generic operation info
    private string formatGenericInfo(hash<DataProviderDebugInfo> info) {
        string result = info.operation.upr();

        if (info.url) {
            result += " " + info.url;
        }

        if (info.duration_ms) {
            result += " " + sprintf("%dms", info.duration_ms);
        }

        return result;
    }

    #! Formats headers for display
    private string formatHeaders(hash<auto> headers, string indent = "") {
        string result = "";
        foreach hash<auto> h in (headers.pairIterator()) {
            result += sprintf("\n%s%s: %s", indent, h.key, h.value);
        }
        return result;
    }

    #! Formats body for display
    private string formatBody(data body, *string content_type) {
        string result;

        if (body.typeCode() == NT_BINARY) {
            if (body.size() > max_body_size) {
                result = sprintf("    [binary: %d bytes, showing first %d]\n", body.size(), max_body_size);
                result += "    " + body.substr(0, max_body_size).toHex();
            } else {
                result = sprintf("    [binary: %d bytes]\n", body.size());
                result += "    " + body.toHex();
            }
        } else {
            string str = body.toString();
            if (str.length() > max_body_size) {
                result = "    " + str.substr(0, max_body_size) + "...";
                result += sprintf("\n    [truncated: %d total bytes]", str.length());
            } else {
                # Indent each line
                str =~ s/\n/\n    /g;
                result = "    " + str;
            }
        }

        return result;
    }
}

} # namespace DataProvider
