# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbConnection class definition

/** MongoDbConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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

public namespace MongoDbDataProvider {
#! MongoDB connection class
/** Provides connection support for MongoDB servers using the mongodb:// URL scheme.
*/
public class MongoDbConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection scheme information
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "MongoDB Connection",
            "short_desc": "Connection to a MongoDB server",
            "desc": "Connection to a MongoDB server for document storage and retrieval",
            "cls": Class::forName("MongoDbDataProvider::MongoDbConnection"),
            "schemes": {
                "mongodb": True,
            },
            "base_scheme_map": {
                "tcp": "mongodb",
            },
            "options": ConnectionOptions,
        };

        #! Connection options
        const ConnectionOptions = {
            "connect_timeout": <ConnectionOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "Connection timeout in milliseconds",
                "type": "int",
                "desc": "Connection timeout in milliseconds (default: 10000)",
                "default_value": 10000,
            },
            "server_selection_timeout": <ConnectionOptionInfo>{
                "display_name": "Server Selection Timeout",
                "short_desc": "Server selection timeout in milliseconds",
                "type": "int",
                "desc": "Timeout for selecting a server in milliseconds (default: 30000)",
                "default_value": 30000,
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
        return "mongodb";
    }

    #! Returns True if the connection supports the data provider API
    bool hasDataProvider() {
        return True;
    }

    #! Returns a data provider for this connection
    /** @param constructor_options optional constructor options for the data provider
        @return a MongoDbDataProvider object
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new MongoDbDataProvider({"client": get()} + constructor_options);
    }

    #! Returns the connection object
    /** @param connect if True, connects to the server (pings the server)
        @param rtopts runtime options
        @return a MongoClient object
    */
    private MongoClient getImpl(bool connect = True, *hash<auto> rtopts) {
        MongoClient client(url);
        if (connect) {
            if (!client.ping()) {
                throw "MONGODB-CONNECTION-ERROR", sprintf("failed to ping MongoDB server at %s", url);
            }
        }
        return client;
    }

    #! Returns the ConnectionSchemeInfo hash
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}
