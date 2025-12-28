# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSetOpsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis set OPS API data provider for set operations
public class RedisSetOpsDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "ops",
            "desc": "Redis set operations API; union, intersection, difference (SUNION, SINTER, SDIFF, SMOVE)",
            "type": "RedisSetOpsDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSetOpsRequestDataType();
        const ResponseType = new RedisSetOpsResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis set OPS provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "sinter":
                list<auto> members = redis.commandWithArgs("SINTER", req.keys);
                return {"members": members};

            case "sinterstore":
                int count = redis.commandWithArgs("SINTERSTORE", (req.destination,) + req.keys);
                return {"count": count};

            case "sdiff":
                list<auto> members = redis.commandWithArgs("SDIFF", req.keys);
                return {"members": members};

            case "sdiffstore":
                int count = redis.commandWithArgs("SDIFFSTORE", (req.destination,) + req.keys);
                return {"count": count};

            case "sunionstore":
                int count = redis.commandWithArgs("SUNIONSTORE", (req.destination,) + req.keys);
                return {"count": count};

            case "smove":
                int result = redis.command("SMOVE", req.source, req.destination, req.member);
                return {"moved": result == 1};

            default:  # sunion
                list<auto> members = redis.commandWithArgs("SUNION", req.keys);
                return {"members": members};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSetOpsRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {
                "display_name": "Operation",
                "short_desc": "Set operation type",
                "desc": "Operation: 'sunion' (default), 'sinter', 'sdiff', 'sunionstore', 'sinterstore', 'sdiffstore', 'smove'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "sunion",
                        "desc": "Union of sets",
                    },
                    <AllowedValueInfo>{
                        "value": "sinter",
                        "desc": "Intersection of sets",
                    },
                    <AllowedValueInfo>{
                        "value": "sdiff",
                        "desc": "Difference of sets",
                    },
                    <AllowedValueInfo>{
                        "value": "sunionstore",
                        "desc": "Store union result",
                    },
                    <AllowedValueInfo>{
                        "value": "sinterstore",
                        "desc": "Store intersection result",
                    },
                    <AllowedValueInfo>{
                        "value": "sdiffstore",
                        "desc": "Store difference result",
                    },
                    <AllowedValueInfo>{
                        "value": "smove",
                        "desc": "Move member between sets",
                    },
                ),
            },
            "keys": {
                "display_name": "Keys",
                "short_desc": "Set keys for operations",
                "desc": "Set keys for union/intersection/difference operations",
                "type": SoftListOrNothingType,
                "example_value": ("set1", "set2"),
            },
            "source": {
                "display_name": "Source",
                "short_desc": "Source set for SMOVE",
                "desc": "Source set for SMOVE",
                "type": StringOrNothingType,
                "example_value": "source_set",
            },
            "destination": {
                "display_name": "Destination",
                "short_desc": "Destination set",
                "desc": "Destination set for store operations and SMOVE",
                "type": StringOrNothingType,
                "example_value": "dest_set",
            },
            "member": {
                "display_name": "Member",
                "short_desc": "Member to move",
                "desc": "Member to move for SMOVE",
                "type": AutoType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisSetOpsResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "members": {
                "display_name": "Members",
                "short_desc": "Resulting members",
                "desc": "Resulting set members",
                "type": SoftListOrNothingType,
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Elements in result",
                "desc": "Number of elements in resulting set (for store ops)",
                "type": IntOrNothingType,
                "example_value": 5,
            },
            "moved": {
                "display_name": "Moved?",
                "short_desc": "Member was moved",
                "desc": "True if member was moved (for SMOVE)",
                "type": SoftBoolType,
                "example_value": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
