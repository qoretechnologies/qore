# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisServerDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedisDataProvider module
public namespace RedisDataProvider {
#! The Redis server operations data provider
public class RedisServerDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "server",
            "desc": "Redis server operations provider; server management (INFO, DBSIZE, FLUSHDB, FLUSHALL, PING, ECHO, TIME)",
            "type": "RedisServerDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisServerRequestDataType();
        const ResponseType = new RedisServerResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis server operations provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "dbsize":
                int size = redis.command("DBSIZE");
                return {"size": size};

            case "flushdb":
                redis.command("FLUSHDB");
                return {"success": True};

            case "flushall":
                redis.command("FLUSHALL");
                return {"success": True};

            case "ping":
                string result = redis.command("PING");
                return {"response": result};

            case "echo":
                string result = redis.command("ECHO", req.message);
                return {"response": result};

            case "time":
                list<auto> result = redis.command("TIME");
                return {"seconds": result[0].toInt(), "microseconds": result[1].toInt()};

            case "config_get":
                list<auto> result = redis.command("CONFIG", "GET", req.parameter ?? "*");
                hash<auto> config = {};
                for (int i = 0; i < result.size(); i += 2) {
                    config{result[i]} = result[i + 1];
                }
                return {"config": config};

            case "client_list":
                string result = redis.command("CLIENT", "LIST");
                return {"clients": result};

            default:  # info
                string result;
                if (req.section) {
                    result = redis.command("INFO", req.section);
                } else {
                    result = redis.command("INFO");
                }
                return {"info": result};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisServerRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {
                "display_name": "Operation",
                "short_desc": "Server operation type",
                "desc": "Operation: 'info' (default), 'dbsize', 'flushdb', 'flushall', 'ping', 'echo', 'time', 'config_get', 'client_list'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "info", "desc": "Get server information"},
                    <AllowedValueInfo>{"value": "dbsize", "desc": "Get database size"},
                    <AllowedValueInfo>{"value": "flushdb", "desc": "Flush current database"},
                    <AllowedValueInfo>{"value": "flushall", "desc": "Flush all databases"},
                    <AllowedValueInfo>{"value": "ping", "desc": "Ping server"},
                    <AllowedValueInfo>{"value": "echo", "desc": "Echo message"},
                    <AllowedValueInfo>{"value": "time", "desc": "Get server time"},
                    <AllowedValueInfo>{"value": "config_get", "desc": "Get configuration"},
                    <AllowedValueInfo>{"value": "client_list", "desc": "List clients"},
                ),
            },
            "section": {
                "display_name": "Section",
                "short_desc": "INFO section",
                "desc": "Section for INFO (server, clients, memory, persistence, stats, replication, cpu, cluster, keyspace)",
                "type": StringOrNothingType,
                "example_value": "server",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "Message for ECHO",
                "desc": "Message for ECHO",
                "type": StringOrNothingType,
                "example_value": "hello",
            },
            "parameter": {
                "display_name": "Parameter",
                "short_desc": "Config parameter pattern",
                "desc": "Parameter pattern for CONFIG GET",
                "type": StringOrNothingType,
                "example_value": "maxmemory*",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisServerResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "info": {
                "display_name": "Info",
                "short_desc": "Server info string",
                "desc": "Server info string",
                "type": StringOrNothingType,
            },
            "size": {
                "display_name": "Size",
                "short_desc": "Database size",
                "desc": "Database size (key count)",
                "type": IntOrNothingType,
                "example_value": 1000,
            },
            "success": {
                "display_name": "Success?",
                "short_desc": "Operation success",
                "desc": "Operation success",
                "type": SoftBoolType,
                "example_value": True,
            },
            "response": {
                "display_name": "Response",
                "short_desc": "PING/ECHO response",
                "desc": "Response for PING/ECHO",
                "type": StringOrNothingType,
                "example_value": "PONG",
            },
            "seconds": {
                "display_name": "Seconds",
                "short_desc": "Unix timestamp seconds",
                "desc": "Unix timestamp seconds",
                "type": IntOrNothingType,
            },
            "microseconds": {
                "display_name": "Microseconds",
                "short_desc": "Microseconds",
                "desc": "Microseconds",
                "type": IntOrNothingType,
            },
            "config": {
                "display_name": "Config",
                "short_desc": "Configuration parameters",
                "desc": "Configuration parameters",
                "type": AutoHashOrNothingType,
            },
            "clients": {
                "display_name": "Clients",
                "short_desc": "Client list",
                "desc": "Client list",
                "type": StringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
