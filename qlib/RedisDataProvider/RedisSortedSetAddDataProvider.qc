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
            "key": {
                "display_name": "Key",
                "short_desc": "The sorted set key",
                "desc": "The sorted set key",
                "type": StringType,
                "example_value": "myzset",
            },
            "operation": {
                "display_name": "Operation",
                "short_desc": "Sorted set operation",
                "desc": "Operation: 'zadd' (default), 'zrem', 'zincrby'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "zadd",
                        "desc": "Add/update members",
                    },
                    <AllowedValueInfo>{
                        "value": "zrem",
                        "desc": "Remove members",
                    },
                    <AllowedValueInfo>{
                        "value": "zincrby",
                        "desc": "Increment member score",
                    },
                ),
            },
            "member": {
                "display_name": "Member",
                "short_desc": "Single member",
                "desc": "Single member",
                "type": AutoType,
                "example_value": "member1",
            },
            "score": {
                "display_name": "Score",
                "short_desc": "Score for member",
                "desc": "Score for single member",
                "type": FloatOrNothingType,
                "example_value": 1.0,
            },
            "members": {
                "display_name": "Members",
                "short_desc": "Score-member pairs",
                "desc": "List of {score, member} hashes",
                "type": SoftListOrNothingType,
            },
            "increment": {
                "display_name": "Increment",
                "short_desc": "Increment value",
                "desc": "Increment value for ZINCRBY",
                "type": FloatOrNothingType,
                "example_value": 1.5,
            },
            "nx": {
                "display_name": "Only New?",
                "short_desc": "Only add new elements",
                "desc": "Only add new elements (don't update scores)",
                "type": SoftBoolOrNothingType,
            },
            "xx": {
                "display_name": "Only Existing?",
                "short_desc": "Only update existing",
                "desc": "Only update existing elements",
                "type": SoftBoolOrNothingType,
            },
            "gt": {
                "display_name": "Greater Than?",
                "short_desc": "Update if new > current",
                "desc": "Only update when new score > current score",
                "type": SoftBoolOrNothingType,
            },
            "lt": {
                "display_name": "Less Than?",
                "short_desc": "Update if new < current",
                "desc": "Only update when new score < current score",
                "type": SoftBoolOrNothingType,
            },
            "ch": {
                "display_name": "Changed?",
                "short_desc": "Return changed count",
                "desc": "Return number of changed elements instead of added",
                "type": SoftBoolOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisSortedSetAddResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "added": {
                "display_name": "Added",
                "short_desc": "Elements added/changed",
                "desc": "Number of elements added/changed",
                "type": IntOrNothingType,
                "example_value": 1,
            },
            "removed": {
                "display_name": "Removed",
                "short_desc": "Elements removed",
                "desc": "Number of elements removed",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "score": {
                "display_name": "Score",
                "short_desc": "New score",
                "desc": "New score after ZINCRBY",
                "type": FloatOrNothingType,
                "example_value": 2.5,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
