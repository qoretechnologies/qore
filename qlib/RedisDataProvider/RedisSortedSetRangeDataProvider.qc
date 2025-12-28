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
            "key": {
                "display_name": "Key",
                "short_desc": "The sorted set key",
                "desc": "The sorted set key",
                "type": StringType,
                "example_value": "myzset",
            },
            "operation": {
                "display_name": "Operation",
                "short_desc": "Range operation type",
                "desc": "Operation: 'zrange' (default), 'zrevrange', 'zrangebyscore', 'zrevrangebyscore'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "zrange",
                        "desc": "Get range by index",
                    },
                    <AllowedValueInfo>{
                        "value": "zrevrange",
                        "desc": "Get reverse range by index",
                    },
                    <AllowedValueInfo>{
                        "value": "zrangebyscore",
                        "desc": "Get range by score",
                    },
                    <AllowedValueInfo>{
                        "value": "zrevrangebyscore",
                        "desc": "Get reverse range by score",
                    },
                ),
            },
            "start": {
                "display_name": "Start",
                "short_desc": "Start index",
                "desc": "Start index for ZRANGE (default: 0)",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "stop": {
                "display_name": "Stop",
                "short_desc": "Stop index",
                "desc": "Stop index for ZRANGE (default: -1)",
                "type": IntOrNothingType,
                "example_value": -1,
            },
            "min": {
                "display_name": "Min Score",
                "short_desc": "Minimum score",
                "desc": "Min score for ZRANGEBYSCORE (default: '-inf')",
                "type": AutoType,
                "example_value": 0,
            },
            "max": {
                "display_name": "Max Score",
                "short_desc": "Maximum score",
                "desc": "Max score for ZRANGEBYSCORE (default: '+inf')",
                "type": AutoType,
                "example_value": 100,
            },
            "withscores": {
                "display_name": "With Scores",
                "short_desc": "Include scores",
                "desc": "Include scores in response",
                "type": SoftBoolOrNothingType,
            },
            "offset": {
                "display_name": "Offset",
                "short_desc": "LIMIT offset",
                "desc": "Offset for LIMIT clause",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "count": {
                "display_name": "Count",
                "short_desc": "LIMIT count",
                "desc": "Count for LIMIT clause",
                "type": IntOrNothingType,
                "example_value": 10,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisSortedSetRangeResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "members": {
                "display_name": "Members",
                "short_desc": "List of members",
                "desc": "List of members",
                "type": SoftListOrNothingType,
            },
            "members_with_scores": {
                "display_name": "Members With Scores",
                "short_desc": "Members with their scores",
                "desc": "List of {member, score} hashes",
                "type": SoftListOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
