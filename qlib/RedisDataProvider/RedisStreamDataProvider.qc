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
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'xadd' (default), 'xread', 'xrange', 'xrevrange', 'xlen', 'xinfo_stream', 'xinfo_groups', 'xgroup_create', 'xgroup_destroy', 'xack', 'xdel', 'xtrim'"},
            "key": {"type": StringOrNothingType, "desc": "Stream key"},
            "streams": {"type": SoftListOrNothingType, "desc": "Stream keys for XREAD"},
            "id": {"type": StringOrNothingType, "desc": "Entry ID (default: '*' for XADD, '$' for XREAD)"},
            "ids": {"type": SoftListOrNothingType, "desc": "Entry IDs for XREAD/XACK/XDEL"},
            "fields": {"type": AutoHashOrNothingType, "desc": "Field-value pairs for XADD"},
            "start": {"type": StringOrNothingType, "desc": "Start ID for XRANGE (default: '-')"},
            "end": {"type": StringOrNothingType, "desc": "End ID for XRANGE (default: '+')"},
            "count": {"type": IntOrNothingType, "desc": "Max entries to return"},
            "block": {"type": IntOrNothingType, "desc": "Block timeout in ms for XREAD"},
            "group": {"type": StringOrNothingType, "desc": "Consumer group name"},
            "mkstream": {"type": SoftBoolOrNothingType, "desc": "Create stream if not exists for XGROUP CREATE"},
            "maxlen": {"type": IntOrNothingType, "desc": "Max length for XADD/XTRIM"},
            "approximate": {"type": SoftBoolOrNothingType, "desc": "Use approximate trimming (~)"},
            "strategy": {"type": StringOrNothingType, "desc": "Trim strategy: MAXLEN or MINID"},
            "threshold": {"type": AutoType, "desc": "Trim threshold value"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisStreamResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "id": {"type": StringOrNothingType, "desc": "Entry ID for XADD"},
            "entries": {"type": SoftListOrNothingType, "desc": "Stream entries"},
            "length": {"type": IntOrNothingType, "desc": "Stream length"},
            "info": {"type": AutoHashOrNothingType, "desc": "Stream info"},
            "groups": {"type": SoftListOrNothingType, "desc": "Consumer groups"},
            "success": {"type": SoftBoolType, "desc": "Operation success"},
            "destroyed": {"type": SoftBoolType, "desc": "Group was destroyed"},
            "acknowledged": {"type": IntOrNothingType, "desc": "Number of acknowledged entries"},
            "deleted": {"type": IntOrNothingType, "desc": "Number of deleted entries"},
            "trimmed": {"type": IntOrNothingType, "desc": "Number of trimmed entries"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
