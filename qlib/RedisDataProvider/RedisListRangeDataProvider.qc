# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisListRangeDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis list RANGE API data provider
public class RedisListRangeDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "range",
            "desc": "Redis list RANGE API; operations on list ranges (LRANGE, LINDEX, LLEN, LSET, LINSERT, LTRIM)",
            "type": "RedisListRangeDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisListRangeRequestDataType();
        const ResponseType = new RedisListRangeResponseDataType();
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
        return sprintf("Redis list RANGE provider for `%s`", redis.getSafeURL());
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "lindex":
                auto value = redis.command("LINDEX", req.key, req.index);
                return {"value": value};

            case "llen":
                int length = redis.command("LLEN", req.key);
                return {"length": length};

            case "lset":
                redis.command("LSET", req.key, req.index, req.value);
                return {"success": True};

            case "linsert":
                string position = req.before ? "BEFORE" : "AFTER";
                int new_length = redis.command("LINSERT", req.key, position, req.pivot, req.value);
                return {"length": new_length};

            case "ltrim":
                redis.command("LTRIM", req.key, req.start, req.stop);
                return {"success": True};

            default:  # lrange
                int start = req.start ?? 0;
                int stop = req.stop ?? -1;
                list<auto> values = redis.command("LRANGE", req.key, start, stop);
                return {"values": values};
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

public class RedisListRangeRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "key": {"type": StringType, "desc": "The list key"},
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'lrange' (default), 'lindex', 'llen', 'lset', 'linsert', 'ltrim'"},
            "start": {"type": IntOrNothingType, "desc": "Start index for LRANGE/LTRIM (default: 0)"},
            "stop": {"type": IntOrNothingType, "desc": "Stop index for LRANGE/LTRIM (default: -1)"},
            "index": {"type": IntOrNothingType, "desc": "Index for LINDEX/LSET"},
            "value": {"type": AutoType, "desc": "Value for LSET/LINSERT"},
            "pivot": {"type": AutoType, "desc": "Pivot element for LINSERT"},
            "before": {"type": SoftBoolOrNothingType, "desc": "Insert before pivot (True) or after (False) for LINSERT"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

public class RedisListRangeResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "values": {"type": SoftListOrNothingType, "desc": "List elements for LRANGE"},
            "value": {"type": AutoType, "desc": "Element value for LINDEX"},
            "length": {"type": IntOrNothingType, "desc": "List length for LLEN/LINSERT"},
            "success": {"type": SoftBoolType, "desc": "Success flag for LSET/LTRIM"},
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
