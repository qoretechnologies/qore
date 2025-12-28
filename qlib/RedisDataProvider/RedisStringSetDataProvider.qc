# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisStringSetDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis string SET API data provider
/** This API allows the caller to set the value of a string key in Redis
*/
public class RedisStringSetDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "set",
            "desc": "Redis string SET API data provider; sets the value of a string key",
            "type": "RedisStringSetDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisStringSetRequestDataType();

        #! Response type
        const ResponseType = new RedisStringSetResponseDataType();
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
        return sprintf("Redis string SET provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Handle MSET for multiple key-value pairs
        if (req.pairs) {
            list<auto> args = ();
            foreach hash<auto> pair in (req.pairs) {
                args += pair.key;
                args += pair.value;
            }
            redis.commandWithArgs("MSET", args);
            return {"success": True};
        }

        # Build SET command with options
        list<auto> args = (req.key, req.value);

        # Add expiration options
        if (req.ex) {
            args += "EX";
            args += req.ex;
        } else if (req.px) {
            args += "PX";
            args += req.px;
        } else if (req.exat) {
            args += "EXAT";
            args += req.exat;
        } else if (req.pxat) {
            args += "PXAT";
            args += req.pxat;
        } else if (req.keepttl) {
            args += "KEEPTTL";
        }

        # Add NX/XX options
        if (req.nx) {
            args += "NX";
        } else if (req.xx) {
            args += "XX";
        }

        # Add GET option
        if (req.get) {
            args += "GET";
        }

        auto result = redis.commandWithArgs("SET", args);

        # Handle response based on options
        if (req.get) {
            return {"previous_value": result};
        }
        return {"success": result == "OK"};
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

#! String SET request data type
public class RedisStringSetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringOrNothingType,
                "desc": "The key to set (for single SET operation)",
            },
            "value": {
                "type": AutoType,
                "desc": "The value to set (for single SET operation)",
            },
            "pairs": {
                "type": SoftListOrNothingType,
                "desc": "A list of {key, value} hashes for MSET operation",
            },
            "ex": {
                "type": IntOrNothingType,
                "desc": "Set expiration time in seconds",
            },
            "px": {
                "type": IntOrNothingType,
                "desc": "Set expiration time in milliseconds",
            },
            "exat": {
                "type": IntOrNothingType,
                "desc": "Set expiration time as Unix timestamp in seconds",
            },
            "pxat": {
                "type": IntOrNothingType,
                "desc": "Set expiration time as Unix timestamp in milliseconds",
            },
            "keepttl": {
                "type": SoftBoolOrNothingType,
                "desc": "Retain the TTL associated with the key",
            },
            "nx": {
                "type": SoftBoolOrNothingType,
                "desc": "Only set the key if it does not already exist",
            },
            "xx": {
                "type": SoftBoolOrNothingType,
                "desc": "Only set the key if it already exists",
            },
            "get": {
                "type": SoftBoolOrNothingType,
                "desc": "Return the old value stored at key, or nil when key did not exist",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! String SET response data type
public class RedisStringSetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "success": {
                "type": SoftBoolType,
                "desc": "True if the operation was successful",
            },
            "previous_value": {
                "type": AutoType,
                "desc": "The previous value if GET option was used, or NOTHING if the key did not exist",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
