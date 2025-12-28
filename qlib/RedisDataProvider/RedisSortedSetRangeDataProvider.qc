# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSortedSetRangeDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis sorted set RANGE API data provider
public class RedisSortedSetRangeDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "range",
            "desc": "Redis sorted set RANGE API; range queries (ZRANGE, ZREVRANGE, ZRANGEBYSCORE)",
            "type": "RedisSortedSetRangeDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSortedSetRangeRequestDataType();
        const ResponseType = new RedisSortedSetRangeResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis sorted set RANGE provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> args = (req.key, req.start ?? 0, req.stop ?? -1);
        list<auto> result;

        switch (req.operation) {
            case "zrevrange":
                if (req.withscores) args += "WITHSCORES";
                result = redis.commandWithArgs("ZREVRANGE", args);
                return parseResult(result, req.withscores);

            case "zrangebyscore":
                args = (req.key, req.min ?? "-inf", req.max ?? "+inf");
                if (req.withscores) args += "WITHSCORES";
                if (exists req.offset && exists req.count) {
                    args += "LIMIT";
                    args += req.offset;
                    args += req.count;
                }
                result = redis.commandWithArgs("ZRANGEBYSCORE", args);
                return parseResult(result, req.withscores);

            case "zrevrangebyscore":
                args = (req.key, req.max ?? "+inf", req.min ?? "-inf");
                if (req.withscores) args += "WITHSCORES";
                if (exists req.offset && exists req.count) {
                    args += "LIMIT";
                    args += req.offset;
                    args += req.count;
                }
                result = redis.commandWithArgs("ZREVRANGEBYSCORE", args);
                return parseResult(result, req.withscores);

            default:  # zrange
                if (req.withscores) args += "WITHSCORES";
                result = redis.commandWithArgs("ZRANGE", args);
                return parseResult(result, req.withscores);
        }
    }

    private hash<auto> parseResult(list<auto> result, *bool withscores) {
        if (withscores) {
            list<hash<auto>> members = ();
            for (int i = 0; i < result.size(); i += 2) {
                members += {"member": result[i], "score": result[i + 1].toFloat()};
            }
            return {"members_with_scores": members};
        }
        return {"members": result};
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSortedSetRangeRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {"type": StringType, "desc": "The sorted set key"},
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'zrange' (default), 'zrevrange', 'zrangebyscore', 'zrevrangebyscore'"},
            "start": {"type": IntOrNothingType, "desc": "Start index for ZRANGE (default: 0)"},
            "stop": {"type": IntOrNothingType, "desc": "Stop index for ZRANGE (default: -1)"},
            "min": {"type": AutoType, "desc": "Min score for ZRANGEBYSCORE (default: '-inf')"},
            "max": {"type": AutoType, "desc": "Max score for ZRANGEBYSCORE (default: '+inf')"},
            "withscores": {"type": SoftBoolOrNothingType, "desc": "Include scores in response"},
            "offset": {"type": IntOrNothingType, "desc": "Offset for LIMIT clause"},
            "count": {"type": IntOrNothingType, "desc": "Count for LIMIT clause"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisSortedSetRangeResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "members": {"type": SoftListOrNothingType, "desc": "List of members"},
            "members_with_scores": {"type": SoftListOrNothingType, "desc": "List of {member, score} hashes"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
