# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSortedSetScoreDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis sorted set SCORE API data provider
public class RedisSortedSetScoreDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "score",
            "desc": "Redis sorted set SCORE API; score and rank queries (ZSCORE, ZRANK, ZREVRANK, ZCARD)",
            "type": "RedisSortedSetScoreDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSortedSetScoreRequestDataType();
        const ResponseType = new RedisSortedSetScoreResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis sorted set SCORE provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "zrank":
                *int rank = redis.command("ZRANK", req.key, req.member);
                return {"rank": rank};

            case "zrevrank":
                *int rank = redis.command("ZREVRANK", req.key, req.member);
                return {"rank": rank};

            case "zcard":
                int count = redis.command("ZCARD", req.key);
                return {"count": count};

            case "zcount":
                int count = redis.command("ZCOUNT", req.key, req.min ?? "-inf", req.max ?? "+inf");
                return {"count": count};

            default:  # zscore
                *string score_str = redis.command("ZSCORE", req.key, req.member);
                return {"score": score_str ? score_str.toFloat() : NOTHING};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSortedSetScoreRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {"type": StringType, "desc": "The sorted set key"},
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'zscore' (default), 'zrank', 'zrevrank', 'zcard', 'zcount'"},
            "member": {"type": AutoType, "desc": "The member to query"},
            "min": {"type": AutoType, "desc": "Min score for ZCOUNT"},
            "max": {"type": AutoType, "desc": "Max score for ZCOUNT"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisSortedSetScoreResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "score": {"type": FloatOrNothingType, "desc": "The member's score"},
            "rank": {"type": IntOrNothingType, "desc": "The member's rank (0-based)"},
            "count": {"type": IntOrNothingType, "desc": "Cardinality or count in score range"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
