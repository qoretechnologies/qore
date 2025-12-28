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
            "key": {"type": StringType, "desc": "The list key"},
            "right": {"type": SoftBoolOrNothingType, "desc": "Pop from right side (RPOP) instead of left (LPOP)"},
            "count": {"type": IntOrNothingType, "desc": "Number of elements to pop (Redis 6.2+)"},
            "blocking": {"type": SoftBoolOrNothingType, "desc": "Use blocking pop (BLPOP/BRPOP)"},
            "timeout": {"type": IntOrNothingType, "desc": "Timeout in seconds for blocking pop (0 = infinite)"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

public class RedisListPopResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "value": {"type": AutoType, "desc": "The popped value, or NOTHING if list is empty"},
            "values": {"type": SoftListOrNothingType, "desc": "Multiple popped values (when count > 1)"},
            "key": {"type": StringOrNothingType, "desc": "The key from which value was popped (for blocking ops)"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
