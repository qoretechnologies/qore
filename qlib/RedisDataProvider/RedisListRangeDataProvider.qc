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
            "key": {
                "display_name": "Key",
                "short_desc": "The list key",
                "desc": "The list key",
                "type": StringType,
                "example_value": "mylist",
            },
            "operation": {
                "display_name": "Operation",
                "short_desc": "List range operation",
                "desc": "Operation: 'lrange' (default), 'lindex', 'llen', 'lset', 'linsert', 'ltrim'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "lrange",
                        "desc": "Get range of elements",
                    },
                    <AllowedValueInfo>{
                        "value": "lindex",
                        "desc": "Get element by index",
                    },
                    <AllowedValueInfo>{
                        "value": "llen",
                        "desc": "Get list length",
                    },
                    <AllowedValueInfo>{
                        "value": "lset",
                        "desc": "Set element at index",
                    },
                    <AllowedValueInfo>{
                        "value": "linsert",
                        "desc": "Insert element before/after pivot",
                    },
                    <AllowedValueInfo>{
                        "value": "ltrim",
                        "desc": "Trim list to range",
                    },
                ),
            },
            "start": {
                "display_name": "Start",
                "short_desc": "Start index",
                "desc": "Start index for LRANGE/LTRIM (default: 0)",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "stop": {
                "display_name": "Stop",
                "short_desc": "Stop index",
                "desc": "Stop index for LRANGE/LTRIM (default: -1)",
                "type": IntOrNothingType,
                "example_value": -1,
            },
            "index": {
                "display_name": "Index",
                "short_desc": "Element index",
                "desc": "Index for LINDEX/LSET",
                "type": IntOrNothingType,
                "example_value": 0,
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Value to set/insert",
                "desc": "Value for LSET/LINSERT",
                "type": AutoType,
                "example_value": "new_item",
            },
            "pivot": {
                "display_name": "Pivot",
                "short_desc": "Pivot element for LINSERT",
                "desc": "Pivot element for LINSERT",
                "type": AutoType,
            },
            "before": {
                "display_name": "Before",
                "short_desc": "Insert before pivot",
                "desc": "Insert before pivot (True) or after (False) for LINSERT",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisListRangeResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "values": {
                "display_name": "Values",
                "short_desc": "List elements",
                "desc": "List elements for LRANGE",
                "type": SoftListOrNothingType,
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Element value",
                "desc": "Element value for LINDEX",
                "type": AutoType,
            },
            "length": {
                "display_name": "Length",
                "short_desc": "List length",
                "desc": "List length for LLEN/LINSERT",
                "type": IntOrNothingType,
                "example_value": 10,
            },
            "success": {
                "display_name": "Success",
                "short_desc": "Operation success",
                "desc": "Success flag for LSET/LTRIM",
                "type": SoftBoolType,
                "example_value": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
