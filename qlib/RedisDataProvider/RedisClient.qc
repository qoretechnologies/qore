# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisClient.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RedisDataProvider module
public namespace RedisDataProvider {
#! Redis RESP protocol client
/** This class implements the Redis Serialization Protocol (RESP) for communication
    with Redis servers.

    @par RESP Protocol Overview
    Redis uses a simple text-based protocol called RESP (REdis Serialization Protocol):
    - Simple Strings: +OK\\r\\n
    - Errors: -ERR message\\r\\n
    - Integers: :1000\\r\\n
    - Bulk Strings: $5\\r\\nhello\\r\\n (or $-1\\r\\n for null)
    - Arrays: *2\\r\\n$3\\r\\nfoo\\r\\n$3\\r\\nbar\\r\\n (or *-1\\r\\n for null)

    @par Example
    @code{.py}
    RedisClient client({"host": "localhost", "port": 6379});
    client.connect();
    client.command("SET", "mykey", "myvalue");
    auto val = client.command("GET", "mykey");
    printf("Value: %y\n", val);  # "myvalue"
    @endcode
*/
public class RedisClient inherits Qore::Serializable, Logger::LoggerWrapper {
    public {
        #! Default Redis port
        const DefaultPort = 6379;

        #! Default connection timeout
        const DefaultConnectTimeout = 30s;

        #! Default operation timeout
        const DefaultTimeout = 30s;

        #! CRLF terminator for RESP protocol
        const CRLF = "\r\n";
    }

    private {
        #! The socket for Redis communication
        Qore::Socket sock();

        #! The Redis server host
        string host = "localhost";

        #! The Redis server port
        int port = DefaultPort;

        #! Optional password for authentication
        *string password;

        #! Optional username for ACL authentication (Redis 6.0+)
        *string username;

        #! Database number to select
        int db = 0;

        #! Connection timeout
        timeout connect_timeout = DefaultConnectTimeout;

        #! Operation timeout
        timeout io_timeout = DefaultTimeout;

        #! Whether to use TLS
        bool tls = False;

        #! TLS/SSL context options
        *hash<auto> ssl_opts;

        #! Whether we are connected
        bool connected = False;
    }

    #! Creates the Redis client with the given options
    /** @param opts a hash with the following keys:
        - \c host: the Redis server hostname (default: "localhost")
        - \c port: the Redis server port (default: 6379)
        - \c password: optional password for authentication
        - \c username: optional username for ACL authentication (Redis 6.0+)
        - \c db: database number to select (default: 0)
        - \c connect_timeout: connection timeout (default: 30s)
        - \c timeout: operation timeout (default: 30s)
        - \c tls: whether to use TLS (default: False)
        - \c ssl_opts: TLS/SSL context options
    */
    constructor(*hash<auto> opts) {
        if (opts.host) {
            host = opts.host;
        }
        if (opts.port) {
            @assert(opts.port > 0 && opts.port <= 65535, sprintf("invalid port number: %d", opts.port));
            port = opts.port;
        }
        password = opts.password;
        username = opts.username;
        if (opts.db) {
            @assert(opts.db >= 0, sprintf("invalid database number: %d", opts.db));
            db = opts.db;
        }
        if (opts.connect_timeout) {
            connect_timeout = opts.connect_timeout;
        }
        if (opts.timeout) {
            io_timeout = opts.timeout;
        }
        if (opts.tls) {
            tls = opts.tls;
        }
        if (opts.ssl_opts) {
            ssl_opts = opts.ssl_opts;
        }
        # Auto-connect unless disabled
        if (!opts.no_connect) {
            connect();
        }
    }

    #! Creates the Redis client from a URL
    /** @param url the Redis URL in the format: redis://[username:password@]host[:port][/db]
                   or rediss://... for TLS connections
        @param do_connect if True (the default), connect immediately
    */
    constructor(string url, bool do_connect = True) {
        parseUrl(url);
        if (do_connect) {
            connect();
        }
    }

    #! Destructor - ensures the connection is closed
    destructor() {
        disconnect();
    }

    #! Parses a Redis URL
    private parseUrl(string url) {
        @assert(url.val(), "URL cannot be empty");
        # Parse URL: redis://[username:password@]host[:port][/db]
        hash<UrlInfo> url_info = parse_url(url);

        if (url_info.protocol) {
            string proto = url_info.protocol.lwr();
            if (proto == "rediss") {
                tls = True;
            } else if (proto != "redis") {
                throw "REDIS-URL-ERROR", sprintf("invalid Redis URL protocol: %y (expected 'redis' or 'rediss')",
                    url_info.protocol);
            }
        }

        if (url_info.host) {
            host = url_info.host;
        }
        if (url_info.port) {
            port = url_info.port;
        }
        if (url_info.username) {
            username = url_info.username;
        }
        if (url_info.password) {
            password = url_info.password;
        }
        if (url_info.path && url_info.path != "/") {
            string path = url_info.path;
            if (path[0] == "/") {
                path = path.substr(1);
            }
            if (path.val()) {
                db = path.toInt();
            }
        }
    }

    #! Connects to the Redis server
    /** Establishes a connection to the Redis server and performs authentication
        and database selection if configured.

        @throw REDIS-CONNECT-ERROR if the connection fails
        @throw REDIS-AUTH-ERROR if authentication fails
    */
    connect() {
        if (connected) {
            return;
        }

        @debug(logDebug("RedisClient: connecting to %s:%d", host, port));

        sock.setSendTimeout(io_timeout);
        sock.setRecvTimeout(io_timeout);

        string target = sprintf("%s:%d", host, port);
        try {
            if (tls) {
                sock.connect(target, connect_timeout);
                sock.upgradeClientToSSL(connect_timeout);
            } else {
                sock.connect(target, connect_timeout);
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "REDIS-CONNECT-ERROR", sprintf("failed to connect to Redis server at %s:%d: %s: %s",
                host, port, ex.err, ex.desc);
        }

        connected = True;
        @debug(logDebug("RedisClient: connected to %s:%d", host, port));

        # Authenticate if password is provided
        if (password) {
            authenticate();
        }

        # Select database if not default
        if (db != 0) {
            selectDb(db);
        }
    }

    #! Disconnects from the Redis server
    disconnect() {
        if (connected) {
            try {
                sock.close();
            } catch () {
                # Ignore errors on close
            }
            connected = False;
            @debug(logDebug("RedisClient: disconnected from %s:%d", host, port));
        }
    }

    #! Returns True if connected to the Redis server
    bool isConnected() {
        return connected;
    }

    #! Authenticates with the Redis server
    private authenticate() {
        try {
            auto result;
            if (username) {
                # Redis 6.0+ ACL authentication
                result = command("AUTH", username, password);
            } else {
                # Legacy password-only authentication
                result = command("AUTH", password);
            }
            @debug(logDebug("RedisClient: authentication successful"));
        } catch (hash<ExceptionInfo> ex) {
            disconnect();
            throw "REDIS-AUTH-ERROR", sprintf("Redis authentication failed: %s", ex.desc);
        }
    }

    #! Selects the given database
    private selectDb(int database) {
        @assert(database >= 0, sprintf("invalid database number: %d", database));
        try {
            command("SELECT", database);
            @debug(logDebug("RedisClient: selected database %d", database));
        } catch (hash<ExceptionInfo> ex) {
            disconnect();
            throw "REDIS-SELECT-ERROR", sprintf("failed to select Redis database %d: %s", database, ex.desc);
        }
    }

    #! Sends a PING command to test the connection
    /** @return "PONG" if the server is reachable
    */
    string ping() {
        return command("PING");
    }

    #! Executes a Redis command
    /** @param cmd the command name (e.g., "SET", "GET", "HGET")
        @param ... additional arguments for the command

        @return the result of the command

        @throw REDIS-ERROR if the command fails
        @throw REDIS-NOT-CONNECTED if not connected to the server
    */
    auto command(string cmd) {
        return commandWithArgs(cmd, argv);
    }

    #! Executes a Redis command with an argument list
    /** @param cmd the command name
        @param args list of arguments for the command

        @return the result of the command

        @throw REDIS-ERROR if the command fails
        @throw REDIS-NOT-CONNECTED if not connected to the server
    */
    auto commandWithArgs(string cmd, softlist<auto> args) {
        @assert(cmd.val(), "command name cannot be empty");
        if (!connected) {
            throw "REDIS-NOT-CONNECTED", "not connected to Redis server; call connect() first";
        }

        # Build command array
        list<auto> cmdlist = (cmd,) + args;

        # Encode and send
        string encoded = encodeRESP(cmdlist);
        @debug(logTrace("RedisClient: sending command: %y", cmdlist));
        sock.send(encoded);

        # Read and decode response
        auto result = decodeRESP();
        @debug(logTrace("RedisClient: received response: %y", result));
        return result;
    }

    #! Sends multiple commands in a pipeline
    /** @param commands a list of command lists, where each command list contains
        the command name followed by its arguments

        @return a list of results, one for each command

        @throw REDIS-ERROR if any command fails
        @throw REDIS-NOT-CONNECTED if not connected to the server

        @par Example
        @code{.py}
        list<auto> results = client.pipeline((
            ("SET", "key1", "value1"),
            ("SET", "key2", "value2"),
            ("GET", "key1"),
            ("GET", "key2"),
        ));
        @endcode
    */
    list<auto> pipeline(list<list<auto>> commands) {
        @assert(commands.val(), "pipeline commands cannot be empty");
        if (!connected) {
            throw "REDIS-NOT-CONNECTED", "not connected to Redis server; call connect() first";
        }

        # Encode all commands
        string encoded = "";
        foreach list<auto> cmdlist in (commands) {
            @assert(cmdlist.val() && cmdlist[0].val(), "each pipeline command must have a command name");
            encoded += encodeRESP(cmdlist);
        }

        @debug(logTrace("RedisClient: pipeline sending %d commands", commands.size()));
        sock.send(encoded);

        # Read all responses
        list<auto> results = ();
        foreach list<auto> cmd in (commands) {
            results += decodeRESP();
        }

        return results;
    }

    #! Encodes a command as RESP protocol
    /** @param cmdlist the command and its arguments as a list

        @return the RESP-encoded command string
    */
    private string encodeRESP(list<auto> cmdlist) {
        # Commands are sent as arrays of bulk strings
        string result = sprintf("*%d%s", cmdlist.size(), CRLF);
        foreach auto arg in (cmdlist) {
            result += encodeBulkString(arg);
        }
        return result;
    }

    #! Encodes a value as a RESP bulk string
    private string encodeBulkString(auto value) {
        if (!exists value) {
            return "$-1" + CRLF;
        }
        string str = string(value);
        return sprintf("$%d%s%s%s", str.size(), CRLF, str, CRLF);
    }

    #! Decodes a RESP response from the socket
    /** @return the decoded value (string, int, list, or NOTHING for null)

        @throw REDIS-ERROR if the server returns an error
        @throw REDIS-PROTOCOL-ERROR if the response is malformed
    */
    private auto decodeRESP() {
        # Read the type byte
        string line = readLine();
        if (!line.val()) {
            throw "REDIS-PROTOCOL-ERROR", "empty response from Redis server";
        }

        string type = line[0];
        string data = line.size() > 1 ? line.substr(1) : "";

        switch (type) {
            case "+":
                # Simple string
                return data;

            case "-":
                # Error
                throw "REDIS-ERROR", data;

            case ":":
                # Integer
                return data.toInt();

            case "$":
                # Bulk string
                return readBulkString(data.toInt());

            case "*":
                # Array
                return readArray(data.toInt());

            default:
                throw "REDIS-PROTOCOL-ERROR", sprintf("unknown RESP type byte: %y", type);
        }
    }

    #! Reads a line from the socket (terminated by CRLF)
    private string readLine() {
        string line = "";
        while (True) {
            string byte = sock.recv(1);
            if (!byte.val()) {
                throw "REDIS-PROTOCOL-ERROR", "unexpected end of data while reading line";
            }
            if (byte == "\r") {
                # Expect \n next
                string next = sock.recv(1);
                if (next != "\n") {
                    throw "REDIS-PROTOCOL-ERROR", "expected LF after CR in RESP protocol";
                }
                break;
            }
            line += byte;
        }
        return line;
    }

    #! Reads a bulk string of the given length
    private *string readBulkString(int length) {
        if (length < 0) {
            # Null bulk string
            return NOTHING;
        }
        if (length == 0) {
            # Empty string - still need to read CRLF
            sock.recv(2);  # Read and discard CRLF
            return "";
        }
        # Read the string data
        string data = sock.recv(length);
        # Read trailing CRLF
        sock.recv(2);
        return data;
    }

    #! Reads an array of the given length
    private *list<auto> readArray(int length) {
        if (length < 0) {
            # Null array
            return NOTHING;
        }
        list<auto> result = ();
        for (int i = 0; i < length; ++i) {
            result += decodeRESP();
        }
        return result;
    }

    #! Returns the host
    string getHost() {
        return host;
    }

    #! Returns the port
    int getPort() {
        return port;
    }

    #! Returns the database number
    int getDb() {
        return db;
    }

    #! Returns whether TLS is enabled
    bool isTls() {
        return tls;
    }

    #! Returns a safe URL for logging (without password)
    string getSafeURL() {
        string proto = tls ? "rediss" : "redis";
        string result = sprintf("%s://", proto);
        if (username) {
            result += username + "@";
        }
        result += sprintf("%s:%d", host, port);
        if (db != 0) {
            result += sprintf("/%d", db);
        }
        return result;
    }

    #! Logs a debug message
    private logDebug(string fmt, ...) {
        logArgs(LoggerLevel::DEBUG, fmt, argv);
    }

    #! Logs a trace message
    private logTrace(string fmt, ...) {
        logArgs(LoggerLevel::TRACE, fmt, argv);
    }
}
}
