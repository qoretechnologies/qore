# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisStreamDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis stream data provider
public class RedisStreamDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "streams",
            "desc": "Redis stream data provider; stream operations (XADD, XREAD, XRANGE, XLEN, XINFO, XGROUP)",
            "type": "RedisStreamDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisStreamRequestDataType();
        const ResponseType = new RedisStreamResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis stream provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "xread":
                list<auto> args = ();
                if (req.count) { args += "COUNT"; args += req.count; }
                if (req.block) { args += "BLOCK"; args += req.block; }
                args += "STREAMS";
                args += req.streams ?? (req.key,);
                args += req.ids ?? (req.id ?? "$",);
                *list<auto> result = redis.commandWithArgs("XREAD", args);
                return {"entries": result};

            case "xrange":
                list<auto> args = (req.key, req.start ?? "-", req.end ?? "+");
                if (req.count) { args += "COUNT"; args += req.count; }
                list<auto> result = redis.commandWithArgs("XRANGE", args);
                return {"entries": parseStreamEntries(result)};

            case "xrevrange":
                list<auto> args = (req.key, req.end ?? "+", req.start ?? "-");
                if (req.count) { args += "COUNT"; args += req.count; }
                list<auto> result = redis.commandWithArgs("XREVRANGE", args);
                return {"entries": parseStreamEntries(result)};

            case "xlen":
                int length = redis.command("XLEN", req.key);
                return {"length": length};

            case "xinfo_stream":
                hash<auto> info = parseInfoResult(redis.command("XINFO", "STREAM", req.key));
                return {"info": info};

            case "xinfo_groups":
                list<auto> groups = redis.command("XINFO", "GROUPS", req.key);
                return {"groups": groups};

            case "xgroup_create":
                redis.commandWithArgs("XGROUP", ("CREATE", req.key, req.group, req.id ?? "$")
                    + (req.mkstream ? ("MKSTREAM",) : ()));
                return {"success": True};

            case "xgroup_destroy":
                int result = redis.command("XGROUP", "DESTROY", req.key, req.group);
                return {"destroyed": result == 1};

            case "xack":
                int count = redis.commandWithArgs("XACK", (req.key, req.group) + req.ids);
                return {"acknowledged": count};

            case "xdel":
                int count = redis.commandWithArgs("XDEL", (req.key,) + req.ids);
                return {"deleted": count};

            case "xtrim":
                list<auto> args = (req.key, req.strategy ?? "MAXLEN");
                if (req.approximate) args += "~";
                args += req.threshold;
                int trimmed = redis.commandWithArgs("XTRIM", args);
                return {"trimmed": trimmed};

            default:  # xadd
                list<auto> args = (req.key,);
                if (req.maxlen) { args += "MAXLEN"; if (req.approximate) args += "~"; args += req.maxlen; }
                args += req.id ?? "*";
                foreach string field in (keys req.fields) {
                    args += field;
                    args += req.fields{field};
                }
                string entry_id = redis.commandWithArgs("XADD", args);
                return {"id": entry_id};
        }
    }

    private list<hash<auto>> parseStreamEntries(list<auto> result) {
        list<hash<auto>> entries = ();
        foreach list<auto> entry in (result) {
            hash<auto> fields = {};
            list<auto> field_list = entry[1];
            for (int i = 0; i < field_list.size(); i += 2) {
                fields{field_list[i]} = field_list[i + 1];
            }
            entries += {"id": entry[0], "fields": fields};
        }
        return entries;
    }

    private hash<auto> parseInfoResult(auto result) {
        if (result.typeCode() == NT_LIST) {
            hash<auto> h = {};
            list<auto> l = result;
            for (int i = 0; i < l.size(); i += 2) {
                h{l[i]} = l[i + 1];
            }
            return h;
        }
        return {"raw": result};
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisStreamRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {
                "display_name": "Operation",
                "short_desc": "Stream operation",
                "desc": "Operation: 'xadd' (default), 'xread', 'xrange', 'xrevrange', 'xlen', 'xinfo_stream', 'xinfo_groups', 'xgroup_create', 'xgroup_destroy', 'xack', 'xdel', 'xtrim'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "xadd", "desc": "Add entry to stream"},
                    <AllowedValueInfo>{"value": "xread", "desc": "Read entries from stream(s)"},
                    <AllowedValueInfo>{"value": "xrange", "desc": "Get range of entries"},
                    <AllowedValueInfo>{"value": "xrevrange", "desc": "Get reverse range"},
                    <AllowedValueInfo>{"value": "xlen", "desc": "Get stream length"},
                    <AllowedValueInfo>{"value": "xinfo_stream", "desc": "Get stream info"},
                    <AllowedValueInfo>{"value": "xinfo_groups", "desc": "Get consumer groups"},
                    <AllowedValueInfo>{"value": "xgroup_create", "desc": "Create consumer group"},
                    <AllowedValueInfo>{"value": "xgroup_destroy", "desc": "Destroy consumer group"},
                    <AllowedValueInfo>{"value": "xack", "desc": "Acknowledge entries"},
                    <AllowedValueInfo>{"value": "xdel", "desc": "Delete entries"},
                    <AllowedValueInfo>{"value": "xtrim", "desc": "Trim stream"},
                ),
            },
            "key": {
                "display_name": "Key",
                "short_desc": "Stream key",
                "desc": "Stream key",
                "type": StringOrNothingType,
                "example_value": "mystream",
            },
            "streams": {
                "display_name": "Streams",
                "short_desc": "Stream keys for XREAD",
                "desc": "Stream keys for XREAD",
                "type": SoftListOrNothingType,
            },
            "id": {
                "display_name": "ID",
                "short_desc": "Entry ID",
                "desc": "Entry ID (default: '*' for XADD, '$' for XREAD)",
                "type": StringOrNothingType,
                "example_value": "*",
            },
            "ids": {
                "display_name": "IDs",
                "short_desc": "Entry IDs",
                "desc": "Entry IDs for XREAD/XACK/XDEL",
                "type": SoftListOrNothingType,
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Field-value pairs",
                "desc": "Field-value pairs for XADD",
                "type": AutoHashOrNothingType,
            },
            "start": {
                "display_name": "Start",
                "short_desc": "Start ID for XRANGE",
                "desc": "Start ID for XRANGE (default: '-')",
                "type": StringOrNothingType,
                "example_value": "-",
            },
            "end": {
                "display_name": "End",
                "short_desc": "End ID for XRANGE",
                "desc": "End ID for XRANGE (default: '+')",
                "type": StringOrNothingType,
                "example_value": "+",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Max entries to return",
                "desc": "Max entries to return",
                "type": IntOrNothingType,
                "example_value": 10,
            },
            "block": {
                "display_name": "Block",
                "short_desc": "Block timeout in ms",
                "desc": "Block timeout in ms for XREAD",
                "type": IntOrNothingType,
                "example_value": 5000,
            },
            "group": {
                "display_name": "Group",
                "short_desc": "Consumer group name",
                "desc": "Consumer group name",
                "type": StringOrNothingType,
                "example_value": "mygroup",
            },
            "mkstream": {
                "display_name": "Make Stream",
                "short_desc": "Create stream if not exists",
                "desc": "Create stream if not exists for XGROUP CREATE",
                "type": SoftBoolOrNothingType,
            },
            "maxlen": {
                "display_name": "Max Length",
                "short_desc": "Max stream length",
                "desc": "Max length for XADD/XTRIM",
                "type": IntOrNothingType,
                "example_value": 1000,
            },
            "approximate": {
                "display_name": "Approximate",
                "short_desc": "Use approximate trimming",
                "desc": "Use approximate trimming (~)",
                "type": SoftBoolOrNothingType,
            },
            "strategy": {
                "display_name": "Strategy",
                "short_desc": "Trim strategy",
                "desc": "Trim strategy: MAXLEN or MINID",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "MAXLEN", "desc": "Trim by maximum length"},
                    <AllowedValueInfo>{"value": "MINID", "desc": "Trim by minimum ID"},
                ),
            },
            "threshold": {
                "display_name": "Threshold",
                "short_desc": "Trim threshold",
                "desc": "Trim threshold value",
                "type": AutoType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

public class RedisStreamResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Entry ID",
                "desc": "Entry ID for XADD",
                "type": StringOrNothingType,
                "example_value": "1609459200000-0",
            },
            "entries": {
                "display_name": "Entries",
                "short_desc": "Stream entries",
                "desc": "Stream entries",
                "type": SoftListOrNothingType,
            },
            "length": {
                "display_name": "Length",
                "short_desc": "Stream length",
                "desc": "Stream length",
                "type": IntOrNothingType,
                "example_value": 100,
            },
            "info": {
                "display_name": "Info",
                "short_desc": "Stream info",
                "desc": "Stream info",
                "type": AutoHashOrNothingType,
            },
            "groups": {
                "display_name": "Groups",
                "short_desc": "Consumer groups",
                "desc": "Consumer groups",
                "type": SoftListOrNothingType,
            },
            "success": {
                "display_name": "Success",
                "short_desc": "Operation success",
                "desc": "Operation success",
                "type": SoftBoolType,
                "example_value": True,
            },
            "destroyed": {
                "display_name": "Destroyed",
                "short_desc": "Group was destroyed",
                "desc": "Group was destroyed",
                "type": SoftBoolType,
            },
            "acknowledged": {
                "display_name": "Acknowledged",
                "short_desc": "Acknowledged entries",
                "desc": "Number of acknowledged entries",
                "type": IntOrNothingType,
            },
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Deleted entries",
                "desc": "Number of deleted entries",
                "type": IntOrNothingType,
            },
            "trimmed": {
                "display_name": "Trimmed",
                "short_desc": "Trimmed entries",
                "desc": "Number of trimmed entries",
                "type": IntOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}
}
