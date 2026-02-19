# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisStringGetDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis string GET API data provider
/** This API allows the caller to get the value of a string key from Redis
*/
public class RedisStringGetDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Redis string GET API data provider; gets the value of a string key",
            "type": "RedisStringGetDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisStringGetRequestDataType();

        #! Response type
        const ResponseType = new RedisStringGetResponseDataType();
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
        return sprintf("Redis string GET provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Handle single key or multiple keys
        if (req.keys) {
            @assert(req.keys.val(), "keys list cannot be empty for MGET");
            @debug(log(LoggerLevel::DEBUG, "MGET: %d keys", req.keys.size()));
            # MGET for multiple keys
            list<auto> values = redis.commandWithArgs("MGET", req.keys);
            return {"values": values};
        } else {
            @assert(req.key.val(), "key is required for GET operation");
            @debug(log(LoggerLevel::DEBUG, "GET: key=%y", req.key));
            # GET for single key
            auto value = redis.command("GET", req.key);
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

#! String GET request data type
public class RedisStringGetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "The key to retrieve",
                "desc": "The key to get the value for (use for single key retrieval)",
                "type": StringOrNothingType,
                "example_value": "mykey",
            },
            "keys": {
                "display_name": "Keys",
                "short_desc": "Multiple keys to retrieve",
                "desc": "A list of keys to get values for (use for multiple key retrieval with `MGET`)",
                "type": SoftListOrNothingType,
                "example_value": ("key1", "key2", "key3"),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! String GET response data type
public class RedisStringGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "value": {
                "display_name": "Value",
                "short_desc": "The retrieved value",
                "desc": "The value of the key (for single key `GET`), or `NOTHING` if the key does not exist",
                "type": AutoType,
                "example_value": "myvalue",
            },
            "values": {
                "display_name": "Values",
                "short_desc": "Multiple retrieved values",
                "desc": "The values of the keys (for `MGET`), with `NOTHING` for keys that do not exist",
                "type": SoftListOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
