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
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'info' (default), 'dbsize', 'flushdb', 'flushall', 'ping', 'echo', 'time', 'config_get', 'client_list'"},
            "section": {"type": StringOrNothingType, "desc": "Section for INFO (server, clients, memory, persistence, stats, replication, cpu, cluster, keyspace)"},
            "message": {"type": StringOrNothingType, "desc": "Message for ECHO"},
            "parameter": {"type": StringOrNothingType, "desc": "Parameter pattern for CONFIG GET"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisServerResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "info": {"type": StringOrNothingType, "desc": "Server info string"},
            "size": {"type": IntOrNothingType, "desc": "Database size (key count)"},
            "success": {"type": SoftBoolType, "desc": "Operation success"},
            "response": {"type": StringOrNothingType, "desc": "Response for PING/ECHO"},
            "seconds": {"type": IntOrNothingType, "desc": "Unix timestamp seconds"},
            "microseconds": {"type": IntOrNothingType, "desc": "Microseconds"},
            "config": {"type": AutoHashOrNothingType, "desc": "Configuration parameters"},
            "clients": {"type": StringOrNothingType, "desc": "Client list"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
