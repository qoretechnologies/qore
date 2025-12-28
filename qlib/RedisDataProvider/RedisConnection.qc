# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! class for Redis connections; returns @ref RedisClient objects
/** supports the following runtime options in getImpl():
    - \c "db": overrides the database number at runtime

    @par URL Format
    The URL format for Redis connections is:
    - \c redis://[username:password@]host[:port][/db]
    - \c rediss://[username:password@]host[:port][/db] (for TLS connections)

    @par Examples
    - \c redis://localhost - connect to localhost on default port 6379
    - \c redis://localhost:6380 - connect to localhost on port 6380
    - \c redis://secret@localhost/1 - authenticate with password, select db 1
    - \c redis://user:secret@localhost/0 - ACL auth with username (Redis 6.0+)
    - \c rediss://localhost - TLS connection to localhost
*/
public class RedisConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Redis Connection",
            "short_desc": "A connection to a Redis server",
            "desc": "A connection to a Redis server using the `redis` or `rediss` (TLS) protocols",
            "cls": Class::forName("RedisDataProvider::RedisConnection"),
            "schemes": {
                "redis": True,
                "rediss": True,
            },
            "base_scheme_map": {
                "rediss": "redis",
            },
            "options": {
                "connect_timeout": <ConnectionOptionInfo>{
                    "display_name": "Connect Timeout",
                    "short_desc": "The connection timeout in seconds",
                    "type": "int",
                    "desc": "The connection timeout in seconds",
                    "default_value": 30,
                },
                "db": <ConnectionOptionInfo>{
                    "display_name": "Database",
                    "short_desc": "The Redis database number to select",
                    "type": "int",
                    "desc": "The Redis database number to select (0-15 by default, configurable per server)",
                    "default_value": 0,
                },
                "password": <ConnectionOptionInfo>{
                    "display_name": "Password",
                    "short_desc": "Sets the password for authentication",
                    "type": "string",
                    "desc": "The password for authentication; only used if no password is set in the URL",
                    "sensitive": True,
                },
                "timeout": <ConnectionOptionInfo>{
                    "display_name": "I/O Timeout",
                    "short_desc": "The I/O timeout in seconds",
                    "type": "int",
                    "desc": "The I/O timeout for read and write operations in seconds",
                    "default_value": 30,
                },
                "username": <ConnectionOptionInfo>{
                    "display_name": "Username",
                    "short_desc": "Sets the username for ACL authentication",
                    "type": "string",
                    "desc": "The username for ACL authentication (Redis 6.0+); only used if no username is set in "
                        "the URL",
                },
            },
        };
    }

    #! creates the RedisConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options

        See @ref AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {}, hash<auto> options = {},
            *Logger::LoggerInterface logger)
            : AbstractConnectionWithInfo(name, description, url, attributes, options, logger) {
    }

    #! creates the RedisConnection object
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored? Default: True
        - enabled (optional bool): is the connection enabled? Default: True
        - locked (optional bool): is the connection locked? Default: False
        - debug_data (optional bool): debug data? Default: False
        - tags (optional hash): tags for the connection; no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! returns \c "redis"
    string getType() {
        return "redis";
    }

    #! returns runtime options
    /** return a hash with the following keys reflecting support for the corresponding runtime options in getImpl():
        - \c "db": overrides the database number at runtime
    */
    *hash<auto> getRuntimeOptions() {
        return {
            "db": True,
        };
    }

    #! returns a RedisDataProvider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return a RedisDataProvider object for this connection

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        @debug(logDebug("RedisConnection::getDataProvider(): creating data provider"));
        return new RedisDataProvider({"redis": get()} + constructor_options);
    }

    #! returns @ref True, as the connection returns a data provider with the @ref getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the @ref getDataProvider() method

        @see @ref getDataProvider()
    */
    bool hasDataProvider() {
        return True;
    }

    #! Sets child data provider capabilities
    private setChildCapabilities() {
        children_can_support_apis = True;
    }

    #! Returns a @ref RedisClient object
    /** @param connect if @ref True "True" then a connection is made to the Redis server
        @param rtopts supports the following runtime options:
        - \c "db": overrides the database number at runtime

        @return a RedisClient object
    */
    private RedisClient getImpl(bool connect = True, *hash<auto> rtopts) {
        hash<auto> client_opts = getClientOptions(rtopts);
        @debug(logDebug("RedisConnection::getImpl(): creating client with opts: %y", client_opts - "password"));
        RedisClient client(client_opts);
        if (connect) {
            client.connect();
            @debug(logDebug("RedisConnection::getImpl(): client connected"));
        }
        return client;
    }

    #! Returns the options for creating a RedisClient
    private hash<auto> getClientOptions(*hash<auto> rtopts) {
        hash<auto> client_opts = <auto>{
            "host": urlh.host ?? "localhost",
        };

        if (urlh.port) {
            client_opts.port = urlh.port;
        }

        # Handle username - URL takes precedence, then options
        if (urlh.username) {
            client_opts.username = urlh.username;
        } else if (opts.username) {
            client_opts.username = opts.username;
        }

        # Handle password - URL takes precedence, then options
        if (urlh.password) {
            client_opts.password = urlh.password;
        } else if (opts.password) {
            client_opts.password = opts.password;
        }

        # Handle database - runtime options take precedence, then URL path, then options
        if (exists rtopts.db) {
            client_opts.db = rtopts.db;
        } else if (urlh.path && urlh.path != "/") {
            string path = urlh.path;
            if (path[0] == "/") {
                path = path.substr(1);
            }
            if (path.val()) {
                client_opts.db = path.toInt();
            }
        } else if (exists opts.db) {
            client_opts.db = opts.db;
        }

        # TLS based on protocol
        if (urlh.protocol == "rediss") {
            client_opts.tls = True;
        }

        # Timeouts from options
        if (opts.connect_timeout) {
            client_opts.connect_timeout = seconds(opts.connect_timeout);
        }
        if (opts.timeout) {
            client_opts.timeout = seconds(opts.timeout);
        }

        return client_opts;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}
