# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedClient class definition

/** MemcachedClient.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

#! The main MemcachedDataProvider namespace
public namespace MemcachedDataProvider {
#! Memcached binary protocol client
/** Implements the memcached binary protocol over a TCP socket connection.

    @see https://github.com/memcached/memcached/wiki/BinaryProtocolRevamped
*/
public class MemcachedClient {
    public {
        #! Binary protocol magic bytes
        const MAGIC_REQUEST = 0x80;
        const MAGIC_RESPONSE = 0x81;

        #! Binary protocol opcodes
        const OP_GET = 0x00;
        const OP_SET = 0x01;
        const OP_ADD = 0x02;
        const OP_REPLACE = 0x03;
        const OP_DELETE = 0x04;
        const OP_INCREMENT = 0x05;
        const OP_DECREMENT = 0x06;
        const OP_QUIT = 0x07;
        const OP_FLUSH = 0x08;
        const OP_GETQ = 0x09;
        const OP_NOOP = 0x0a;
        const OP_VERSION = 0x0b;
        const OP_GETK = 0x0c;
        const OP_GETKQ = 0x0d;
        const OP_APPEND = 0x0e;
        const OP_PREPEND = 0x0f;
        const OP_STAT = 0x10;
        const OP_SETQ = 0x11;
        const OP_ADDQ = 0x12;
        const OP_REPLACEQ = 0x13;
        const OP_DELETEQ = 0x14;
        const OP_INCREMENTQ = 0x15;
        const OP_DECREMENTQ = 0x16;
        const OP_QUITQ = 0x17;
        const OP_FLUSHQ = 0x18;
        const OP_APPENDQ = 0x19;
        const OP_PREPENDQ = 0x1a;
        const OP_VERBOSITY = 0x1b;
        const OP_TOUCH = 0x1c;
        const OP_GAT = 0x1d;
        const OP_GATQ = 0x1e;
        const OP_GATK = 0x23;
        const OP_GATKQ = 0x24;

        #! Response status codes
        const STATUS_OK = 0x0000;
        const STATUS_KEY_NOT_FOUND = 0x0001;
        const STATUS_KEY_EXISTS = 0x0002;
        const STATUS_VALUE_TOO_LARGE = 0x0003;
        const STATUS_INVALID_ARGUMENTS = 0x0004;
        const STATUS_ITEM_NOT_STORED = 0x0005;
        const STATUS_INCR_DECR_NON_NUMERIC = 0x0006;
        const STATUS_VBUCKET_ERROR = 0x0007;
        const STATUS_AUTH_ERROR = 0x0008;
        const STATUS_AUTH_CONTINUE = 0x0009;
        const STATUS_UNKNOWN_COMMAND = 0x0081;
        const STATUS_OUT_OF_MEMORY = 0x0082;
        const STATUS_NOT_SUPPORTED = 0x0083;
        const STATUS_INTERNAL_ERROR = 0x0084;
        const STATUS_BUSY = 0x0085;
        const STATUS_TEMP_FAILURE = 0x0086;

        #! Status code to string mapping
        const StatusMap = {
            STATUS_OK: "OK",
            STATUS_KEY_NOT_FOUND: "NOT_FOUND",
            STATUS_KEY_EXISTS: "EXISTS",
            STATUS_VALUE_TOO_LARGE: "VALUE_TOO_LARGE",
            STATUS_INVALID_ARGUMENTS: "INVALID_ARGUMENTS",
            STATUS_ITEM_NOT_STORED: "NOT_STORED",
            STATUS_INCR_DECR_NON_NUMERIC: "NON_NUMERIC",
            STATUS_VBUCKET_ERROR: "VBUCKET_ERROR",
            STATUS_AUTH_ERROR: "AUTH_ERROR",
            STATUS_AUTH_CONTINUE: "AUTH_CONTINUE",
            STATUS_UNKNOWN_COMMAND: "UNKNOWN_COMMAND",
            STATUS_OUT_OF_MEMORY: "OUT_OF_MEMORY",
            STATUS_NOT_SUPPORTED: "NOT_SUPPORTED",
            STATUS_INTERNAL_ERROR: "INTERNAL_ERROR",
            STATUS_BUSY: "BUSY",
            STATUS_TEMP_FAILURE: "TEMP_FAILURE",
        };

        #! Maximum key length
        const MAX_KEY_LENGTH = 250;

        #! Default port
        const DEFAULT_PORT = 11211;
    }

    private {
        #! Socket for memcached communication
        Socket sock();

        #! Mutex for thread safety
        Mutex m();

        #! Connection parameters
        string host;
        int port;
        timeout connect_timeout = 30s;
        timeout io_timeout = 15s;

        #! SSL/TLS mode
        bool ssl = False;

        #! Connected flag
        bool connected = False;

        #! Opaque counter for binary protocol
        int opaque = 0;
    }

    #! Creates the client with the given options
    /** @param opts a hash with the following keys:
        - \c url: (string) URL in the format memcached://host:port or memcacheds://host:port for SSL
        - \c host: (string) hostname or IP address (alternative to url)
        - \c port: (int) port number (default: 11211)
        - \c connect_timeout: (timeout) connection timeout (default: 30s)
        - \c io_timeout: (timeout) I/O timeout (default: 15s)
        - \c ssl: (bool) use SSL/TLS (default: False)
    */
    constructor(hash<auto> opts) {
        if (opts.url) {
            hash<UrlInfo> url_info = parse_url(opts.url);
            host = url_info.host;
            port = url_info.port ?? DEFAULT_PORT;
            ssl = url_info.protocol == "memcacheds";
        } else {
            host = opts.host ?? "localhost";
            port = opts.port ?? DEFAULT_PORT;
            ssl = opts.ssl ?? False;
        }
        if (opts.connect_timeout) {
            connect_timeout = opts.connect_timeout;
        }
        if (opts.io_timeout) {
            io_timeout = opts.io_timeout;
        }
    }

    #! Destructor - closes connection
    destructor() {
        disconnect();
    }

    #! Returns the host
    string getHost() {
        return host;
    }

    #! Returns the port
    int getPort() {
        return port;
    }

    #! Returns a safe URL for display (no credentials)
    string getSafeURL() {
        return sprintf("%s://%s:%d", ssl ? "memcacheds" : "memcached", host, port);
    }

    #! Connects to the memcached server
    connect() {
        AutoLock al(m);
        if (connected) {
            return;
        }
        sock.connect(sprintf("%s:%d", host, port), connect_timeout);
        if (ssl) {
            sock.upgradeClientToSSL();
        }
        sock.setRecvTimeout(io_timeout);
        sock.setSendTimeout(io_timeout);
        connected = True;
    }

    #! Disconnects from the server
    disconnect() {
        AutoLock al(m);
        if (!connected) {
            return;
        }
        try {
            sock.close();
        } catch () {
            # ignore errors on close
        }
        connected = False;
    }

    #! Returns True if connected
    bool isConnected() {
        return connected;
    }

    #! Store a value (set operation)
    /** @param key the key to store
        @param value the value to store
        @param flags optional flags (32-bit unsigned int)
        @param exptime expiration time in seconds (0 = never expire)
        @return a hash with: stored (bool), status (string), cas (int)
    */
    hash<auto> set(string key, data value, int flags = 0, int exptime = 0) {
        return doStorage(OP_SET, key, value, flags, exptime);
    }

    #! Store a value only if key doesn't exist (add operation)
    hash<auto> add(string key, data value, int flags = 0, int exptime = 0) {
        return doStorage(OP_ADD, key, value, flags, exptime);
    }

    #! Store a value only if key exists (replace operation)
    hash<auto> replace(string key, data value, int flags = 0, int exptime = 0) {
        return doStorage(OP_REPLACE, key, value, flags, exptime);
    }

    #! Append data to existing value
    hash<auto> append(string key, data value) {
        return doAppendPrepend(OP_APPEND, key, value);
    }

    #! Prepend data to existing value
    hash<auto> prepend(string key, data value) {
        return doAppendPrepend(OP_PREPEND, key, value);
    }

    #! Compare and swap - store only if CAS token matches
    hash<auto> cas(string key, data value, int cas_token, int flags = 0, int exptime = 0) {
        return doStorage(OP_SET, key, value, flags, exptime, cas_token);
    }

    #! Get a value by key
    /** @param key the key to retrieve
        @return a hash with: found (bool), key (string), value (data), flags (int), cas (int)
    */
    hash<auto> get(string key) {
        return doGet(OP_GET, key);
    }

    #! Get a value with CAS token
    hash<auto> gets(string key) {
        return doGet(OP_GET, key);
    }

    #! Get and touch (update expiration)
    hash<auto> gat(string key, int exptime) {
        return doGat(OP_GAT, key, exptime);
    }

    #! Get and touch with CAS token
    hash<auto> gats(string key, int exptime) {
        return doGat(OP_GAT, key, exptime);
    }

    #! Get multiple keys at once
    /** @param key_list list of keys to retrieve
        @return a hash with: items (list of hash), found (int), missing (int)
    */
    hash<auto> multiGet(list<string> key_list) {
        validateKeyList(key_list);
        AutoLock al(m);
        ensureConnected();

        list<hash<auto>> items = ();
        int found = 0;
        int missing = 0;

        # Send quiet get requests for all but the last key
        for (int i = 0; i < key_list.size() - 1; i++) {
            sendRequest(OP_GETKQ, key_list[i]);
        }
        # Send normal get for the last key to trigger response
        if (key_list.size() > 0) {
            sendRequest(OP_GETK, key_list[key_list.size() - 1]);
        }

        # Read all responses
        while (True) {
            hash<auto> resp = readResponse();
            if (resp.opcode == OP_GETK || resp.opcode == OP_GETKQ) {
                if (resp.status == STATUS_OK) {
                    push items, {
                        "key": resp.key,
                        "value": resp.value,
                        "flags": resp.flags,
                        "cas": resp.cas,
                        "found": True,
                    };
                    found++;
                }
            }
            # Last response received
            if (resp.opcode == OP_GETK) {
                break;
            }
        }
        missing = key_list.size() - found;

        return {
            "items": items,
            "found": found,
            "missing": missing,
        };
    }

    #! Delete a key
    /** @param key the key to delete
        @return a hash with: deleted (bool), status (string)
    */
    hash<auto> del(string key) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        sendRequest(OP_DELETE, key);
        hash<auto> resp = readResponse();

        return {
            "deleted": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    #! Increment a numeric value
    /** @param key the key to increment
        @param delta the amount to increment by
        @param initial initial value if key doesn't exist (optional)
        @param exptime expiration time for new keys (optional)
        @return a hash with: success (bool), value (int), status (string)
    */
    hash<auto> incr(string key, int delta = 1, *int initial, int exptime = 0) {
        return doIncrDecr(OP_INCREMENT, key, delta, initial, exptime);
    }

    #! Decrement a numeric value
    hash<auto> decr(string key, int delta = 1, *int initial, int exptime = 0) {
        return doIncrDecr(OP_DECREMENT, key, delta, initial, exptime);
    }

    #! Touch - update expiration time
    /** @param key the key to touch
        @param exptime new expiration time
        @return a hash with: touched (bool), status (string)
    */
    hash<auto> touch(string key, int exptime) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        # Build extras: 4 bytes exptime
        binary extras = binary();
        extras += encodeInt32(exptime);

        sendRequestWithExtras(OP_TOUCH, key, NOTHING, extras);
        hash<auto> resp = readResponse();

        return {
            "touched": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    #! Flush all cached data
    /** @param delay optional delay in seconds before flushing
        @return a hash with: flushed (bool), status (string)
    */
    hash<auto> flushAll(int delay = 0) {
        AutoLock al(m);
        ensureConnected();

        binary extras = binary();
        if (delay > 0) {
            extras += encodeInt32(delay);
        }

        sendRequestWithExtras(OP_FLUSH, NOTHING, NOTHING, extras);
        hash<auto> resp = readResponse();

        return {
            "flushed": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    #! Get server version
    /** @return a hash with: version (string)
    */
    hash<auto> version() {
        AutoLock al(m);
        ensureConnected();

        sendRequest(OP_VERSION);
        hash<auto> resp = readResponse();

        return {
            "version": resp.status == STATUS_OK ? resp.value.toString() : NOTHING,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    #! Set verbosity level
    /** @param level verbosity level (0-2)
        @return a hash with: success (bool), status (string)
    */
    hash<auto> verbosity(int level) {
        AutoLock al(m);
        ensureConnected();

        binary extras = encodeInt32(level);
        sendRequestWithExtras(OP_VERBOSITY, NOTHING, NOTHING, extras);
        hash<auto> resp = readResponse();

        return {
            "success": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    #! Get server statistics
    /** @param type optional stat type: NOTHING for general, "items", "slabs", "sizes"
        @return a hash with statistics
    */
    hash<auto> stats(*string type) {
        AutoLock al(m);
        ensureConnected();

        sendRequest(OP_STAT, type);

        hash<auto> result = {};
        while (True) {
            hash<auto> resp = readResponse();
            if (resp.status != STATUS_OK) {
                break;
            }
            # Empty key signals end of stats
            if (!resp.key) {
                break;
            }
            result{resp.key} = resp.value.toString();
        }

        return result;
    }

    #! Quit - close connection gracefully
    quit() {
        {
            AutoLock al(m);
            if (!connected) {
                return;
            }
            try {
                sendRequest(OP_QUIT);
            } catch () {
                # ignore errors
            }
        }
        disconnect();
    }

    private validateKey(string key) {
        if (key.empty()) {
            throw "MEMCACHED-KEY-ERROR", "Key cannot be empty";
        }
        if (key.length() > MAX_KEY_LENGTH) {
            throw "MEMCACHED-KEY-ERROR", sprintf("Key length %d exceeds maximum of %d bytes",
                key.length(), MAX_KEY_LENGTH);
        }
        # Check for invalid characters (spaces, newlines, control chars)
        if (key =~ /[\x00-\x20\x7f]/) {
            throw "MEMCACHED-KEY-ERROR", "Key contains invalid characters (spaces or control characters)";
        }
    }

    private validateKeyList(list<string> key_list) {
        if (key_list.empty()) {
            throw "MEMCACHED-KEY-ERROR", "Key list cannot be empty";
        }
        map validateKey($1), key_list;
    }

    private ensureConnected() {
        if (!connected) {
            throw "MEMCACHED-CONNECTION-ERROR", "Not connected to memcached server";
        }
    }

    private hash<auto> doStorage(int opcode, string key, data value, int flags, int exptime, *int cas_token) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        binary val = value.typeCode() == NT_BINARY ? value : binary(value.toString());

        # Build extras: 4 bytes flags + 4 bytes exptime
        binary extras = binary();
        extras += encodeInt32(flags);
        extras += encodeInt32(exptime);

        sendRequestWithExtras(opcode, key, val, extras, cas_token);
        hash<auto> resp = readResponse();

        return {
            "stored": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
            "cas": resp.cas,
        };
    }

    #! Perform append/prepend operation (no extras in binary protocol)
    private hash<auto> doAppendPrepend(int opcode, string key, data value) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        binary val = value.typeCode() == NT_BINARY ? value : binary(value.toString());

        # Append/prepend don't use extras - just key and value
        sendRequestWithExtras(opcode, key, val, NOTHING);
        hash<auto> resp = readResponse();

        return {
            "stored": resp.status == STATUS_OK,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
            "cas": resp.cas,
        };
    }

    private hash<auto> doGet(int opcode, string key) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        sendRequest(opcode, key);
        hash<auto> resp = readResponse();

        if (resp.status == STATUS_OK) {
            return {
                "found": True,
                "key": key,
                "value": resp.value,
                "flags": resp.flags,
                "cas": resp.cas,
            };
        }
        return {
            "found": False,
            "key": key,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    private hash<auto> doGat(int opcode, string key, int exptime) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        # Build extras: 4 bytes exptime
        binary extras = encodeInt32(exptime);

        sendRequestWithExtras(opcode, key, NOTHING, extras);
        hash<auto> resp = readResponse();

        if (resp.status == STATUS_OK) {
            return {
                "found": True,
                "key": key,
                "value": resp.value,
                "flags": resp.flags,
                "cas": resp.cas,
            };
        }
        return {
            "found": False,
            "key": key,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    private hash<auto> doIncrDecr(int opcode, string key, int delta, *int initial, int exptime) {
        validateKey(key);
        AutoLock al(m);
        ensureConnected();

        # Build extras: 8 bytes delta + 8 bytes initial + 4 bytes exptime
        binary extras = binary();
        extras += encodeInt64(delta);
        extras += encodeInt64(initial ?? 0);
        # If initial is not provided, use max exptime to signal "don't create"
        extras += encodeInt32(exists initial ? exptime : 0xffffffff);

        sendRequestWithExtras(opcode, key, NOTHING, extras);
        hash<auto> resp = readResponse();

        if (resp.status == STATUS_OK) {
            # Response value is 8-byte big-endian integer
            int result_value = decodeInt64(resp.value);
            return {
                "success": True,
                "value": result_value,
                "status": "OK",
            };
        }
        return {
            "success": False,
            "status": StatusMap{resp.status} ?? "UNKNOWN",
        };
    }

    private sendRequest(int opcode, *string key, *binary value) {
        sendRequestWithExtras(opcode, key, value, NOTHING);
    }

    private sendRequestWithExtras(int opcode, *string key, *binary value, *binary extras, *int cas_token) {
        int key_len = key ? key.length() : 0;
        int extras_len = extras ? extras.size() : 0;
        int value_len = value ? value.size() : 0;
        int total_body = key_len + extras_len + value_len;

        # Build 24-byte header
        binary header = binary();
        header += chr(MAGIC_REQUEST);      # Magic
        header += chr(opcode);             # Opcode
        header += encodeInt16(key_len);    # Key length
        header += chr(extras_len);         # Extras length
        header += chr(0);                  # Data type (raw bytes)
        header += encodeInt16(0);          # vbucket id / reserved
        header += encodeInt32(total_body); # Total body length
        header += encodeInt32(++opaque);   # Opaque
        header += encodeInt64(cas_token ?? 0); # CAS

        # Build full request
        binary request = header;
        if (extras) {
            request += extras;
        }
        if (key) {
            request += binary(key);
        }
        if (value) {
            request += value;
        }

        sock.send(request);
    }

    private hash<auto> readResponse() {
        # Read 24-byte header
        binary header = sock.recvBinary(24);
        if (header.size() != 24) {
            throw "MEMCACHED-PROTOCOL-ERROR", "Incomplete response header";
        }

        # Binary indexing returns integers directly, no need for ord()
        int magic = header[0];
        if (magic != MAGIC_RESPONSE) {
            throw "MEMCACHED-PROTOCOL-ERROR", sprintf("Invalid response magic: 0x%02x", magic);
        }

        int resp_opcode = header[1];
        int key_len = decodeInt16(header.substr(2, 2));
        int extras_len = header[4];
        # int data_type = header[5];
        int status = decodeInt16(header.substr(6, 2));
        int total_body = decodeInt32(header.substr(8, 4));
        # int resp_opaque = decodeInt32(header.substr(12, 4));
        int cas = decodeInt64(header.substr(16, 8));

        # Read body if present
        *binary extras;
        *string key;
        *binary value;

        if (total_body > 0) {
            binary body = sock.recvBinary(total_body);
            if (body.size() != total_body) {
                throw "MEMCACHED-PROTOCOL-ERROR", "Incomplete response body";
            }

            int offset = 0;
            if (extras_len > 0) {
                extras = body.substr(offset, extras_len);
                offset += extras_len;
            }
            if (key_len > 0) {
                key = body.substr(offset, key_len).toString();
                offset += key_len;
            }
            int value_len = total_body - extras_len - key_len;
            if (value_len > 0) {
                value = body.substr(offset, value_len);
            }
        }

        # Parse flags from extras for GET responses
        int flags = 0;
        if (extras && extras.size() >= 4) {
            flags = decodeInt32(extras.substr(0, 4));
        }

        return {
            "opcode": resp_opcode,
            "status": status,
            "cas": cas,
            "key": key,
            "value": value,
            "flags": flags,
        };
    }

    #! Encode a 16-bit integer in big-endian format
    private binary encodeInt16(int val) {
        return binary(chr((val >> 8) & 0xff) + chr(val & 0xff));
    }

    #! Encode a 32-bit integer in big-endian format
    private binary encodeInt32(int val) {
        return binary(chr((val >> 24) & 0xff) + chr((val >> 16) & 0xff) +
                      chr((val >> 8) & 0xff) + chr(val & 0xff));
    }

    #! Encode a 64-bit integer in big-endian format
    private binary encodeInt64(int val) {
        return binary(chr((val >> 56) & 0xff) + chr((val >> 48) & 0xff) +
                      chr((val >> 40) & 0xff) + chr((val >> 32) & 0xff) +
                      chr((val >> 24) & 0xff) + chr((val >> 16) & 0xff) +
                      chr((val >> 8) & 0xff) + chr(val & 0xff));
    }

    #! Decode a 16-bit integer from big-endian format
    private int decodeInt16(binary data) {
        # Binary indexing returns integers directly
        return (data[0] << 8) | data[1];
    }

    #! Decode a 32-bit integer from big-endian format
    private int decodeInt32(binary data) {
        return (data[0] << 24) | (data[1] << 16) |
               (data[2] << 8) | data[3];
    }

    #! Decode a 64-bit integer from big-endian format
    private int decodeInt64(binary data) {
        return (data[0] << 56) | (data[1] << 48) |
               (data[2] << 40) | (data[3] << 32) |
               (data[4] << 24) | (data[5] << 16) |
               (data[6] << 8) | data[7];
    }
}
}
