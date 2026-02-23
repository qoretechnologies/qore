# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisStringIncrDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis string INCR/DECR API data provider
/** This API allows the caller to increment or decrement a numeric string value in Redis
*/
public class RedisStringIncrDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "incr",
            "desc": "Redis string `INCR`/`DECR` API data provider; increments or decrements a numeric value",
            "type": "RedisStringIncrDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisStringIncrRequestDataType();

        #! Response type
        const ResponseType = new RedisStringIncrResponseDataType();
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
        return sprintf("Redis string INCR/DECR provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        auto result;

        if (exists req.increment_float) {
            # INCRBYFLOAT
            result = redis.command("INCRBYFLOAT", req.key, req.increment_float);
            return {"value": result.toFloat()};
        } else if (exists req.increment) {
            if (req.increment == 1) {
                # INCR
                result = redis.command("INCR", req.key);
            } else if (req.increment == -1) {
                # DECR
                result = redis.command("DECR", req.key);
            } else if (req.increment >= 0) {
                # INCRBY
                result = redis.command("INCRBY", req.key, req.increment);
            } else {
                # DECRBY (for negative increments)
                result = redis.command("DECRBY", req.key, -req.increment);
            }
        } else if (exists req.decrement) {
            if (req.decrement == 1) {
                # DECR
                result = redis.command("DECR", req.key);
            } else {
                # DECRBY
                result = redis.command("DECRBY", req.key, req.decrement);
            }
        } else {
            # Default to INCR
            result = redis.command("INCR", req.key);
        }

        return {"value": result};
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

#! String INCR request data type
public class RedisStringIncrRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The key to increment/decrement",
                "desc": "The key to increment/decrement",
                "type": StringType,
                "example_value": "counter",
            },
            "increment": {
                "display_name": "Increment",
                "short_desc": "Integer increment value",
                "desc": "The integer increment value (use negative for decrement). If not specified, defaults to 1",
                "type": IntOrNothingType,
                "example_value": 5,
            },
            "decrement": {
                "display_name": "Decrement",
                "short_desc": "Integer decrement value",
                "desc": "The integer decrement value. Alternative to using negative increment",
                "type": IntOrNothingType,
                "example_value": 1,
            },
            "increment_float": {
                "display_name": "Float Increment",
                "short_desc": "Floating-point increment",
                "desc": "The floating-point increment value (uses INCRBYFLOAT command)",
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

#! String INCR response data type
public class RedisStringIncrResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "New value after operation",
                "desc": "The new value after the increment/decrement operation (integer or float depending on command)",
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
