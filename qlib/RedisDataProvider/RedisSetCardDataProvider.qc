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
            "key": {"type": StringType, "desc": "The set key"},
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'scard' (default), 'srandmember', 'spop'"},
            "count": {"type": IntOrNothingType, "desc": "Number of random members to return/pop"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisSetCardResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "count": {"type": IntOrNothingType, "desc": "Set cardinality for SCARD"},
            "member": {"type": AutoType, "desc": "Random member for single SRANDMEMBER/SPOP"},
            "members": {"type": SoftListOrNothingType, "desc": "Random members for SRANDMEMBER/SPOP with count"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
