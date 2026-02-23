# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisHashGetDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis hash GET API data provider
/** This API allows the caller to get hash field values from Redis
*/
public class RedisHashGetDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Redis hash GET API data provider; gets hash field values (`HGET`, `HMGET`, `HGETALL`, `HKEYS`, `HVALS`)",
            "type": "RedisHashGetDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisHashGetRequestDataType();

        #! Response type
        const ResponseType = new RedisHashGetResponseDataType();
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
        return sprintf("Redis hash GET provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.operation == "hgetall") {
            # HGETALL - get all fields and values
            list<auto> result = redis.command("HGETALL", req.key);
            # Convert list to hash (alternating key/value pairs)
            hash<auto> h = {};
            for (int i = 0; i < result.size(); i += 2) {
                h{result[i]} = result[i + 1];
            }
            return {"hash": h};
        } else if (req.operation == "hkeys") {
            # HKEYS - get all field names
            list<auto> field_names = redis.command("HKEYS", req.key);
            return {"keys": field_names};
        } else if (req.operation == "hvals") {
            # HVALS - get all values
            list<auto> values = redis.command("HVALS", req.key);
            return {"values": values};
        } else if (req.operation == "hlen") {
            # HLEN - get number of fields
            int length = redis.command("HLEN", req.key);
            return {"length": length};
        } else if (req.fields) {
            # HMGET - get multiple fields
            list<auto> values = redis.commandWithArgs("HMGET", (req.key,) + req.fields);
            return {"values": values};
        } else {
            # HGET - get single field
            auto value = redis.command("HGET", req.key, req.field);
            return {"value": value};
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

#! Hash GET request data type
public class RedisHashGetRequestDataType inherits DataProvider::HashDataType {
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
                "short_desc": "Hash get operation type",
                "desc": "The operation: 'hget' (default), 'hmget', 'hgetall', 'hkeys', 'hvals', 'hlen'",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "hget",
                        "desc": "Get single field value",
                    },
                    <AllowedValueInfo>{
                        "value": "hmget",
                        "desc": "Get multiple field values",
                    },
                    <AllowedValueInfo>{
                        "value": "hgetall",
                        "desc": "Get all fields and values",
                    },
                    <AllowedValueInfo>{
                        "value": "hkeys",
                        "desc": "Get all field names",
                    },
                    <AllowedValueInfo>{
                        "value": "hvals",
                        "desc": "Get all values",
                    },
                    <AllowedValueInfo>{
                        "value": "hlen",
                        "desc": "Get number of fields",
                    },
                ),
            },
            "field": {
                "display_name": "Field",
                "short_desc": "Field name for HGET",
                "desc": "The field name for `HGET` operation",
                "type": StringOrNothingType,
                "example_value": "username",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Field names for HMGET",
                "desc": "The list of field names for `HMGET` operation",
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

#! Hash GET response data type
public class RedisHashGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "Single field value",
                "desc": "The field value for `HGET` operation",
                "type": AutoType,
                "example_value": "john_doe",
            },
            "values": {
                "display_name": "Values",
                "short_desc": "Multiple field values",
                "desc": "The field values for `HMGET`/`HVALS` operations",
                "type": SoftListOrNothingType,
            },
            "hash": {
                "display_name": "Hash",
                "short_desc": "Full hash contents",
                "desc": "The full hash for `HGETALL` operation",
                "type": AutoHashOrNothingType,
            },
            "keys": {
                "display_name": "Keys",
                "short_desc": "Field names",
                "desc": "The field names for HKEYS operation",
                "type": SoftListOrNothingType,
            },
            "length": {
                "display_name": "Length",
                "short_desc": "Number of fields",
                "desc": "The number of fields for HLEN operation",
                "type": IntOrNothingType,
                "example_value": 5,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
