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
            "operation": {
                "display_name": "Operation",
                "short_desc": "Key operation type",
                "desc": "Operation: 'del' (default), 'exists', 'expire', 'expireat', 'pexpire', 'ttl', 'pttl', 'persist', 'keys', 'scan', 'type', 'rename', 'renamenx'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "del", "desc": "Delete key(s)"},
                    <AllowedValueInfo>{"value": "exists", "desc": "Check if key(s) exist"},
                    <AllowedValueInfo>{"value": "expire", "desc": "Set TTL in seconds"},
                    <AllowedValueInfo>{"value": "expireat", "desc": "Set expiry Unix timestamp"},
                    <AllowedValueInfo>{"value": "pexpire", "desc": "Set TTL in milliseconds"},
                    <AllowedValueInfo>{"value": "ttl", "desc": "Get TTL in seconds"},
                    <AllowedValueInfo>{"value": "pttl", "desc": "Get TTL in milliseconds"},
                    <AllowedValueInfo>{"value": "persist", "desc": "Remove expiry"},
                    <AllowedValueInfo>{"value": "keys", "desc": "Find keys by pattern"},
                    <AllowedValueInfo>{"value": "scan", "desc": "Incrementally iterate keys"},
                    <AllowedValueInfo>{"value": "type", "desc": "Get key type"},
                    <AllowedValueInfo>{"value": "rename", "desc": "Rename key"},
                    <AllowedValueInfo>{"value": "renamenx", "desc": "Rename if new key doesn't exist"},
                ),
            },
            "key": {
                "display_name": "Key",
                "short_desc": "Key to operate on",
                "desc": "The key to operate on",
                "type": StringOrNothingType,
                "example_value": "mykey",
            },
            "keys": {
                "display_name": "Keys",
                "short_desc": "Multiple keys",
                "desc": "Multiple keys for DEL/EXISTS",
                "type": SoftListOrNothingType,
                "example_value": ("key1", "key2"),
            },
            "newkey": {
                "display_name": "New Key",
                "short_desc": "New key name for RENAME",
                "desc": "New key name for RENAME",
                "type": StringOrNothingType,
                "example_value": "newkey",
            },
            "seconds": {
                "display_name": "Seconds",
                "short_desc": "TTL in seconds",
                "desc": "TTL in seconds for EXPIRE",
                "type": IntOrNothingType,
                "example_value": 3600,
            },
            "milliseconds": {
                "display_name": "Milliseconds",
                "short_desc": "TTL in milliseconds",
                "desc": "TTL in milliseconds for PEXPIRE",
                "type": IntOrNothingType,
                "example_value": 60000,
            },
            "timestamp": {
                "display_name": "Timestamp",
                "short_desc": "Unix timestamp",
                "desc": "Unix timestamp for EXPIREAT",
                "type": IntOrNothingType,
            },
            "pattern": {
                "display_name": "Pattern",
                "short_desc": "Key pattern",
                "desc": "Pattern for KEYS/SCAN",
                "type": StringOrNothingType,
                "example_value": "user:*",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "SCAN cursor",
                "desc": "Cursor for SCAN (default: 0)",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "count": {
                "display_name": "Count",
                "short_desc": "SCAN count hint",
                "desc": "Count hint for SCAN",
                "type": IntOrNothingType,
                "example_value": 100,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisKeyResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Keys deleted count",
                "desc": "Number of keys deleted",
                "type": IntOrNothingType,
                "example_value": 1,
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Existing keys count",
                "desc": "Number of existing keys",
                "type": IntOrNothingType,
                "example_value": 2,
            },
            "success": {
                "display_name": "Success",
                "short_desc": "Operation success",
                "desc": "Operation success",
                "type": SoftBoolType,
                "example_value": True,
            },
            "ttl": {
                "display_name": "TTL",
                "short_desc": "Time to live",
                "desc": "TTL in seconds/milliseconds (-1 = no expiry, -2 = key doesn't exist)",
                "type": IntOrNothingType,
                "example_value": 3600,
            },
            "keys": {
                "display_name": "Keys",
                "short_desc": "Matching keys",
                "desc": "List of keys matching pattern",
                "type": SoftListOrNothingType,
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "Next SCAN cursor",
                "desc": "Next cursor for SCAN (0 = complete)",
                "type": IntOrNothingType,
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Key type",
                "desc": "Key type (string, list, set, zset, hash, stream)",
                "type": StringOrNothingType,
                "example_value": "string",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
