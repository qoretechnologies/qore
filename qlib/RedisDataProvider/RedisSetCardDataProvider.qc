# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSetCardDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis set CARD API data provider
public class RedisSetCardDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "card",
            "desc": "Redis set cardinality and random member API (SCARD, SRANDMEMBER, SPOP)",
            "type": "RedisSetCardDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSetCardRequestDataType();
        const ResponseType = new RedisSetCardResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis set CARD provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "srandmember":
                if (req.count) {
                    list<auto> members = redis.command("SRANDMEMBER", req.key, req.count);
                    return {"members": members};
                }
                auto member = redis.command("SRANDMEMBER", req.key);
                return {"member": member};

            case "spop":
                if (req.count) {
                    list<auto> members = redis.command("SPOP", req.key, req.count);
                    return {"members": members};
                }
                auto member = redis.command("SPOP", req.key);
                return {"member": member};

            default:  # scard
                int count = redis.command("SCARD", req.key);
                return {"count": count};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSetCardRequestDataType inherits DataProvider::HashDataType {
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
                "short_desc": "Cardinality operation",
                "desc": "Operation: 'scard' (default), 'srandmember', 'spop'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "scard",
                        "desc": "Get set cardinality",
                    },
                    <AllowedValueInfo>{
                        "value": "srandmember",
                        "desc": "Get random member(s)",
                    },
                    <AllowedValueInfo>{
                        "value": "spop",
                        "desc": "Pop random member(s)",
                    },
                ),
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of random members",
                "desc": "Number of random members to return/pop",
                "type": IntOrNothingType,
                "example_value": 3,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisSetCardResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "count": {
                "display_name": "Count",
                "short_desc": "Set cardinality",
                "desc": "Set cardinality for SCARD",
                "type": IntOrNothingType,
                "example_value": 10,
            },
            "member": {
                "display_name": "Member",
                "short_desc": "Random member",
                "desc": "Random member for single SRANDMEMBER/SPOP",
                "type": AutoType,
            },
            "members": {
                "display_name": "Members",
                "short_desc": "Random members",
                "desc": "Random members for SRANDMEMBER/SPOP with count",
                "type": SoftListOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
