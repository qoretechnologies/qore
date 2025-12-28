# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSortedSetAddDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis sorted set ADD API data provider
public class RedisSortedSetAddDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add",
            "desc": "Redis sorted set ADD API; add, remove, and increment (ZADD, ZREM, ZINCRBY)",
            "type": "RedisSortedSetAddDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisSortedSetAddRequestDataType();
        const ResponseType = new RedisSortedSetAddResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis sorted set ADD provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "zrem":
                list<auto> args = (req.key,);
                args += req.members ?? (req.member,);
                int removed = redis.commandWithArgs("ZREM", args);
                return {"removed": removed};

            case "zincrby":
                string result = redis.command("ZINCRBY", req.key, req.increment, req.member);
                return {"score": result.toFloat()};

            default:  # zadd
                list<auto> args = (req.key,);
                # Add options
                if (req.nx) args += "NX";
                else if (req.xx) args += "XX";
                if (req.gt) args += "GT";
                else if (req.lt) args += "LT";
                if (req.ch) args += "CH";
                # Add score-member pairs
                foreach hash<auto> sm in (req.members ?? ({"score": req.score, "member": req.member},)) {
                    args += sm.score;
                    args += sm.member;
                }
                int added = redis.commandWithArgs("ZADD", args);
                return {"added": added};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisSortedSetAddRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {"type": StringType, "desc": "The sorted set key"},
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'zadd' (default), 'zrem', 'zincrby'"},
            "member": {"type": AutoType, "desc": "Single member"},
            "score": {"type": FloatOrNothingType, "desc": "Score for single member"},
            "members": {"type": SoftListOrNothingType, "desc": "List of {score, member} hashes"},
            "increment": {"type": FloatOrNothingType, "desc": "Increment value for ZINCRBY"},
            "nx": {"type": SoftBoolOrNothingType, "desc": "Only add new elements (don't update scores)"},
            "xx": {"type": SoftBoolOrNothingType, "desc": "Only update existing elements"},
            "gt": {"type": SoftBoolOrNothingType, "desc": "Only update when new score > current score"},
            "lt": {"type": SoftBoolOrNothingType, "desc": "Only update when new score < current score"},
            "ch": {"type": SoftBoolOrNothingType, "desc": "Return number of changed elements instead of added"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisSortedSetAddResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "added": {"type": IntOrNothingType, "desc": "Number of elements added/changed"},
            "removed": {"type": IntOrNothingType, "desc": "Number of elements removed"},
            "score": {"type": FloatOrNothingType, "desc": "New score after ZINCRBY"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
