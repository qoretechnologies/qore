# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisKeyDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis key operations data provider
public class RedisKeyDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "keys",
            "desc": "Redis key operations provider; key management (DEL, EXISTS, EXPIRE, TTL, KEYS, SCAN, TYPE, RENAME)",
            "type": "RedisKeyDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisKeyRequestDataType();
        const ResponseType = new RedisKeyResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis key operations provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "exists":
                list<auto> args = req.keys ?? (req.key,);
                int count = redis.commandWithArgs("EXISTS", args);
                return {"count": count};

            case "expire":
                int result = redis.command("EXPIRE", req.key, req.seconds);
                return {"success": result == 1};

            case "expireat":
                int result = redis.command("EXPIREAT", req.key, req.timestamp);
                return {"success": result == 1};

            case "pexpire":
                int result = redis.command("PEXPIRE", req.key, req.milliseconds);
                return {"success": result == 1};

            case "ttl":
                int ttl = redis.command("TTL", req.key);
                return {"ttl": ttl};

            case "pttl":
                int pttl = redis.command("PTTL", req.key);
                return {"ttl": pttl};

            case "persist":
                int result = redis.command("PERSIST", req.key);
                return {"success": result == 1};

            case "keys":
                list<auto> key_list = redis.command("KEYS", req.pattern ?? "*");
                return {"keys": key_list};

            case "scan":
                list<auto> result = redis.commandWithArgs("SCAN", (req.cursor ?? 0,)
                    + (req.pattern ? ("MATCH", req.pattern) : ())
                    + (req.count ? ("COUNT", req.count) : ()));
                return {"cursor": result[0].toInt(), "keys": result[1]};

            case "type":
                string type = redis.command("TYPE", req.key);
                return {"type": type};

            case "rename":
                redis.command("RENAME", req.key, req.newkey);
                return {"success": True};

            case "renamenx":
                int result = redis.command("RENAMENX", req.key, req.newkey);
                return {"success": result == 1};

            default:  # del
                list<auto> args = req.keys ?? (req.key,);
                int deleted = redis.commandWithArgs("DEL", args);
                return {"deleted": deleted};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisKeyRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'del' (default), 'exists', 'expire', 'expireat', 'pexpire', 'ttl', 'pttl', 'persist', 'keys', 'scan', 'type', 'rename', 'renamenx'"},
            "key": {"type": StringOrNothingType, "desc": "The key to operate on"},
            "keys": {"type": SoftListOrNothingType, "desc": "Multiple keys for DEL/EXISTS"},
            "newkey": {"type": StringOrNothingType, "desc": "New key name for RENAME"},
            "seconds": {"type": IntOrNothingType, "desc": "TTL in seconds for EXPIRE"},
            "milliseconds": {"type": IntOrNothingType, "desc": "TTL in milliseconds for PEXPIRE"},
            "timestamp": {"type": IntOrNothingType, "desc": "Unix timestamp for EXPIREAT"},
            "pattern": {"type": StringOrNothingType, "desc": "Pattern for KEYS/SCAN"},
            "cursor": {"type": IntOrNothingType, "desc": "Cursor for SCAN (default: 0)"},
            "count": {"type": IntOrNothingType, "desc": "Count hint for SCAN"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisKeyResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "deleted": {"type": IntOrNothingType, "desc": "Number of keys deleted"},
            "count": {"type": IntOrNothingType, "desc": "Number of existing keys"},
            "success": {"type": SoftBoolType, "desc": "Operation success"},
            "ttl": {"type": IntOrNothingType, "desc": "TTL in seconds/milliseconds (-1 = no expiry, -2 = key doesn't exist)"},
            "keys": {"type": SoftListOrNothingType, "desc": "List of keys matching pattern"},
            "cursor": {"type": IntOrNothingType, "desc": "Next cursor for SCAN (0 = complete)"},
            "type": {"type": StringOrNothingType, "desc": "Key type (string, list, set, zset, hash, stream)"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
