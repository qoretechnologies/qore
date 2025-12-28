# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSetAddDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis set ADD/REM/MEMBERS API data provider
public class RedisSetAddDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add",
            "desc": "Redis set ADD/REM/MEMBERS API; add, remove, and list set members (SADD, SREM, SMEMBERS, SISMEMBER)",
            "type": "RedisSetAddDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSetAddRequestDataType();
        const ResponseType = new RedisSetAddResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis set ADD provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "srem":
                list<auto> args = (req.key,);
                args += req.members ?? (req.member,);
                int removed = redis.commandWithArgs("SREM", args);
                return {"removed": removed};

            case "smembers":
                list<auto> members = redis.command("SMEMBERS", req.key);
                return {"members": members};

            case "sismember":
                int result = redis.command("SISMEMBER", req.key, req.member);
                return {"is_member": result == 1};

            case "smismember":
                list<auto> results = redis.commandWithArgs("SMISMEMBER", (req.key,) + req.members);
                return {"results": results};

            default:  # sadd
                list<auto> args = (req.key,);
                args += req.members ?? (req.member,);
                int added = redis.commandWithArgs("SADD", args);
                return {"added": added};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSetAddRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The set key",
                "desc": "The set key",
                "type": StringType,
                "example_value": "myset",
            },
            "operation": {
                "display_name": "Operation",
                "short_desc": "Set operation type",
                "desc": "Operation: 'sadd' (default), 'srem', 'smembers', 'sismember', 'smismember'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "sadd",
                        "desc": "Add member(s) to set",
                    },
                    <AllowedValueInfo>{
                        "value": "srem",
                        "desc": "Remove member(s) from set",
                    },
                    <AllowedValueInfo>{
                        "value": "smembers",
                        "desc": "Get all members",
                    },
                    <AllowedValueInfo>{
                        "value": "sismember",
                        "desc": "Check if member exists",
                    },
                    <AllowedValueInfo>{
                        "value": "smismember",
                        "desc": "Check multiple members",
                    },
                ),
            },
            "member": {
                "display_name": "Member",
                "short_desc": "Single member",
                "desc": "Single member to add/remove/check",
                "type": AutoType,
                "example_value": "value1",
            },
            "members": {
                "display_name": "Members",
                "short_desc": "Multiple members",
                "desc": "Multiple members to add/remove/check",
                "type": SoftListOrNothingType,
                "example_value": ("value1", "value2", "value3"),
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisSetAddResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "added": {
                "display_name": "Added",
                "short_desc": "Members added count",
                "desc": "Number of members added",
                "type": IntOrNothingType,
                "example_value": 3,
            },
            "removed": {
                "display_name": "Removed",
                "short_desc": "Members removed count",
                "desc": "Number of members removed",
                "type": IntOrNothingType,
                "example_value": 1,
            },
            "members": {
                "display_name": "Members",
                "short_desc": "Set members",
                "desc": "Set members for SMEMBERS",
                "type": SoftListOrNothingType,
            },
            "is_member": {
                "display_name": "Is Member",
                "short_desc": "Member exists",
                "desc": "True if member exists for SISMEMBER",
                "type": SoftBoolType,
                "example_value": True,
            },
            "results": {
                "display_name": "Results",
                "short_desc": "Membership results",
                "desc": "Membership results for SMISMEMBER",
                "type": SoftListOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
