# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisHashSetDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis hash SET API data provider
/** This API allows the caller to set hash field values in Redis
*/
public class RedisHashSetDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "set",
            "desc": "Redis hash SET API data provider; sets hash field values (`HSET`, `HMSET`, `HSETNX`)",
            "type": "RedisHashSetDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisHashSetRequestDataType();

        #! Response type
        const ResponseType = new RedisHashSetResponseDataType();
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
        return sprintf("Redis hash SET provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.key.val(), "key is required for hash SET operation");
        if (req.nx) {
            # HSETNX - set only if field doesn't exist
            @assert(req.field.val(), "field is required for HSETNX");
            @debug(log(LoggerLevel::DEBUG, "HSETNX: key=%y field=%y", req.key, req.field));
            int result = redis.command("HSETNX", req.key, req.field, req.value);
            return {"created": result == 1};
        } else if (req.fields) {
            # HSET with multiple field-value pairs
            @assert(req.fields.val(), "fields list cannot be empty");
            @debug(log(LoggerLevel::DEBUG, "HSET: key=%y %d fields", req.key, req.fields.size()));
            list<auto> args = (req.key,);
            foreach hash<auto> fv in (req.fields) {
                @assert(fv.field.val(), "each field must have a field name");
                args += fv.field;
                args += fv.value;
            }
            int count = redis.commandWithArgs("HSET", args);
            return {"fields_added": count};
        } else {
            # HSET - single field-value
            @assert(req.field.val(), "field is required for HSET");
            @debug(log(LoggerLevel::DEBUG, "HSET: key=%y field=%y", req.key, req.field));
            int result = redis.command("HSET", req.key, req.field, req.value);
            return {"fields_added": result};
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

#! Hash SET request data type
public class RedisHashSetRequestDataType inherits DataProvider::HashDataType {
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
            "field": {
                "display_name": "Field",
                "short_desc": "Field name for single operations",
                "desc": "The field name for single field operations",
                "type": StringOrNothingType,
                "example_value": "username",
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Value to set",
                "desc": "The value to set for single field operations",
                "type": AutoType,
                "example_value": "john_doe",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Multiple field-value pairs",
                "desc": "A list of {field, value} hashes for setting multiple fields",
                "type": SoftListOrNothingType,
            },
            "nx": {
                "display_name": "Only If New?",
                "short_desc": "Set only if field doesn't exist",
                "desc": "Set to `True` to only set if the field does not exist (`HSETNX`)",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Hash SET response data type
public class RedisHashSetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "fields_added": {
                "display_name": "Fields Added",
                "short_desc": "Number of new fields added",
                "desc": "The number of new fields added (0 if field was updated, 1 if created)",
                "type": IntOrNothingType,
                "example_value": 1,
            },
            "created": {
                "display_name": "Created?",
                "short_desc": "Field was created",
                "desc": "`True` if the field was created (for `HSETNX` operation)",
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
