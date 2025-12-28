# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedConnection class definition

/** MemcachedConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MemcachedDataProvider {
#! Memcached connection class
/** Provides connection support for memcached servers using the memcached:// and memcacheds:// URL schemes.
*/
public class MemcachedConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection scheme information
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Memcached Connection",
            "short_desc": "Connection to a memcached server",
            "desc": "Connection to a memcached server for caching data using the memcached binary protocol",
            "cls": Class::forName("MemcachedDataProvider::MemcachedConnection"),
            "schemes": {
                "memcached": True,
                "memcacheds": True,
            },
            "base_scheme_map": {
                "tcp": "memcached",
                "tls": "memcacheds",
                "ssl": "memcacheds",
            },
            "options": ConnectionOptions,
        };

        #! Connection options
        const ConnectionOptions = {
            "connect_timeout": <ConnectionOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "Connection timeout in milliseconds",
                "type": "int",
                "desc": "Connection timeout in milliseconds (default: 30000)",
                "default_value": 30000,
            },
            "io_timeout": <ConnectionOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "Read/write timeout in milliseconds",
                "type": "int",
                "desc": "I/O timeout in milliseconds for read/write operations (default: 15000)",
                "default_value": 15000,
            },
        };
    }

    #! Creates the connection object from a URL
    /** @param name the name of the connection
        @param desc a description of the connection
        @param url the URL to connect to
        @param attributes connection attributes
        @param options connection options
    */
    constructor(string name, string desc, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : AbstractConnectionWithInfo(name, desc, url, attributes, options) {
    }

    #! Creates the connection object from a hash
    /** @param config a hash with connection configuration
        @param attr optional connection attributes
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! Returns the connection type
    string getType() {
        return "memcached";
    }

    #! Returns True if the connection supports the data provider API
    bool hasDataProvider() {
        return True;
    }

    #! Returns a data provider for this connection
    /** @param constructor_options optional constructor options for the data provider
        @return a MemcachedDataProvider object
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new MemcachedDataProvider({"memcached": get()} + constructor_options);
    }

    #! Returns the connection object
    /** @param connect if True, connects to the server
        @param rtopts runtime options
        @return a MemcachedClient object
    */
    private MemcachedClient getImpl(bool connect = True, *hash<auto> rtopts) {
        MemcachedClient client(getConnectionOptions() + rtopts);
        if (connect) {
            client.connect();
        }
        return client;
    }

    #! Returns connection options for the client
    private hash<auto> getConnectionOptions() {
        return {
            "url": url,
        } + opts;
    }

    #! Returns the ConnectionSchemeInfo hash
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}
