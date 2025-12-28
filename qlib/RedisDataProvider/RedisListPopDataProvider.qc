# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisListPopDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis list POP API data provider
public class RedisListPopDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "pop",
            "desc": "Redis list POP API data provider; pops elements from lists (LPOP, RPOP, BLPOP, BRPOP)",
            "type": "RedisListPopDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisListPopRequestDataType();
        const ResponseType = new RedisListPopResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    *string getDesc() {
        return sprintf("Redis list POP provider for `%s`", redis.getSafeURL());
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.blocking) {
            # BLPOP or BRPOP
            string cmd = req.right ? "BRPOP" : "BLPOP";
            int timeout_secs = req.timeout ?? 0;
            *list<auto> result = redis.command(cmd, req.key, timeout_secs);
            if (result) {
                return {"key": result[0], "value": result[1]};
            }
            return {"value": NOTHING};
        } else {
            # LPOP or RPOP
            string cmd = req.right ? "RPOP" : "LPOP";
            if (req.count) {
                list<auto> values = redis.command(cmd, req.key, req.count);
                return {"values": values};
            }
            auto value = redis.command(cmd, req.key);
            return {"value": value};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

public class RedisListPopRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The list key",
                "desc": "The list key",
                "type": StringType,
                "example_value": "mylist",
            },
            "right": {
                "display_name": "Pop Right",
                "short_desc": "Pop from right side (RPOP)",
                "desc": "Pop from right side (RPOP) instead of left (LPOP)",
                "type": SoftBoolOrNothingType,
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of elements to pop",
                "desc": "Number of elements to pop (Redis 6.2+)",
                "type": IntOrNothingType,
                "example_value": 3,
            },
            "blocking": {
                "display_name": "Blocking",
                "short_desc": "Use blocking pop",
                "desc": "Use blocking pop (BLPOP/BRPOP)",
                "type": SoftBoolOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Blocking timeout in seconds",
                "desc": "Timeout in seconds for blocking pop (0 = infinite)",
                "type": IntOrNothingType,
                "example_value": 30,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisListPopResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "Popped value",
                "desc": "The popped value, or NOTHING if list is empty",
                "type": AutoType,
                "example_value": "item1",
            },
            "values": {
                "display_name": "Values",
                "short_desc": "Multiple popped values",
                "desc": "Multiple popped values (when count > 1)",
                "type": SoftListOrNothingType,
            },
            "key": {
                "display_name": "Key",
                "short_desc": "Source key for blocking ops",
                "desc": "The key from which value was popped (for blocking ops)",
                "type": StringOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
