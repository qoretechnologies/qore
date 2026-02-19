# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisHashDelDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis hash DEL API data provider
/** This API allows the caller to delete hash fields or check field existence
*/
public class RedisHashDelDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "del",
            "desc": "Redis hash DEL API data provider; deletes hash fields and checks existence (`HDEL`, `HEXISTS`)",
            "type": "RedisHashDelDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisHashDelRequestDataType();

        #! Response type
        const ResponseType = new RedisHashDelResponseDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    #! Creates the object from a Redis client
    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Redis hash DEL provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.operation == "hexists") {
            # HEXISTS - check if field exists
            int result = redis.command("HEXISTS", req.key, req.field);
            return {"exists": result == 1};
        } else if (req.fields) {
            # HDEL with multiple fields
            int deleted = redis.commandWithArgs("HDEL", (req.key,) + req.fields);
            return {"deleted": deleted};
        } else {
            # HDEL - single field
            int deleted = redis.command("HDEL", req.key, req.field);
            return {"deleted": deleted};
        }
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Hash DEL request data type
public class RedisHashDelRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The hash key",
                "desc": "The hash key to operate on",
                "type": StringType,
                "example_value": "myhash",
            },
            "operation": {
                "display_name": "Operation",
                "short_desc": "Delete or check existence",
                "desc": "The operation: 'hdel' (default), 'hexists'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "hdel",
                        "desc": "Delete field(s)",
                    },
                    <AllowedValueInfo>{
                        "value": "hexists",
                        "desc": "Check if field exists",
                    },
                ),
            },
            "field": {
                "display_name": "Field",
                "short_desc": "Field name",
                "desc": "The field name for single field operations",
                "type": StringOrNothingType,
                "example_value": "username",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Multiple field names",
                "desc": "A list of field names for deleting multiple fields",
                "type": SoftListOrNothingType,
                "example_value": ("field1", "field2"),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Hash DEL response data type
public class RedisHashDelResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Number of fields deleted",
                "desc": "The number of fields deleted",
                "type": IntOrNothingType,
                "example_value": 2,
            },
            "exists": {
                "display_name": "Exists?",
                "short_desc": "Field exists",
                "desc": "`True` if the field exists (for `HEXISTS` operation)",
                "type": SoftBoolType,
                "example_value": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
