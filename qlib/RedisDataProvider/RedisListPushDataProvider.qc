# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisListPushDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis list PUSH API data provider
public class RedisListPushDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "push",
            "desc": "Redis list PUSH API data provider; pushes elements to lists (LPUSH, RPUSH, LPUSHX, RPUSHX)",
            "type": "RedisListPushDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisListPushRequestDataType();
        const ResponseType = new RedisListPushResponseDataType();
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
        return sprintf("Redis list PUSH provider for `%s`", redis.getSafeURL());
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string cmd;
        if (req.right) {
            cmd = req.only_if_exists ? "RPUSHX" : "RPUSH";
        } else {
            cmd = req.only_if_exists ? "LPUSHX" : "LPUSH";
        }

        list<auto> args = (req.key,);
        if (req.values) {
            args += req.values;
        } else {
            args += req.value;
        }

        int new_length = redis.commandWithArgs(cmd, args);
        return {"length": new_length};
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

public class RedisListPushRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {"type": StringType, "desc": "The list key"},
            "value": {"type": AutoType, "desc": "Single value to push"},
            "values": {"type": SoftListOrNothingType, "desc": "Multiple values to push"},
            "right": {"type": SoftBoolOrNothingType, "desc": "Push to right side (RPUSH) instead of left (LPUSH)"},
            "only_if_exists": {"type": SoftBoolOrNothingType, "desc": "Only push if the list exists (LPUSHX/RPUSHX)"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

public class RedisListPushResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "length": {"type": IntType, "desc": "The new length of the list"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
