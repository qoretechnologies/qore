# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisHashIncrDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis hash INCR API data provider
/** This API allows the caller to increment hash field values in Redis
*/
public class RedisHashIncrDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "incr",
            "desc": "Redis hash INCR API data provider; increments hash field values (HINCRBY, HINCRBYFLOAT)",
            "type": "RedisHashIncrDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisHashIncrRequestDataType();

        #! Response type
        const ResponseType = new RedisHashIncrResponseDataType();
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
        return sprintf("Redis hash INCR provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (exists req.increment_float) {
            # HINCRBYFLOAT
            string result = redis.command("HINCRBYFLOAT", req.key, req.field, req.increment_float);
            return {"value": result.toFloat()};
        } else {
            # HINCRBY
            int increment = req.increment ?? 1;
            int result = redis.command("HINCRBY", req.key, req.field, increment);
            return {"value": result};
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

#! Hash INCR request data type
public class RedisHashIncrRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The hash key",
                "desc": "The hash key",
                "type": StringType,
                "example_value": "myhash",
            },
            "field": {
                "display_name": "Field",
                "short_desc": "Field to increment",
                "desc": "The field name to increment",
                "type": StringType,
                "example_value": "counter",
            },
            "increment": {
                "display_name": "Increment",
                "short_desc": "Integer increment value",
                "desc": "The integer increment value (default: 1)",
                "type": IntOrNothingType,
                "example_value": 5,
            },
            "increment_float": {
                "display_name": "Float Increment",
                "short_desc": "Floating-point increment",
                "desc": "The floating-point increment value (uses HINCRBYFLOAT)",
                "type": FloatOrNothingType,
                "example_value": 1.5,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Hash INCR response data type
public class RedisHashIncrResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "New value after increment",
                "desc": "The new value after incrementing (integer or float)",
                "type": AutoType,
                "example_value": 10,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
