# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisStringAppendDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis string APPEND/STRLEN API data provider
/** This API allows the caller to append to a string or get string length in Redis
*/
public class RedisStringAppendDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "append",
            "desc": "Redis string APPEND/STRLEN/GETRANGE/SETRANGE API data provider",
            "type": "RedisStringAppendDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RedisStringAppendRequestDataType();

        #! Response type
        const ResponseType = new RedisStringAppendResponseDataType();
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
        return sprintf("Redis string APPEND provider for `%s`", redis.getSafeURL());
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.operation == "strlen") {
            # STRLEN - get string length
            int length = redis.command("STRLEN", req.key);
            return {"length": length};
        } else if (req.operation == "getrange") {
            # GETRANGE - get substring
            string value = redis.command("GETRANGE", req.key, req.start, req.end);
            return {"value": value};
        } else if (req.operation == "setrange") {
            # SETRANGE - overwrite part of string
            int new_length = redis.command("SETRANGE", req.key, req.offset, req.value);
            return {"length": new_length};
        } else {
            # Default: APPEND
            int new_length = redis.command("APPEND", req.key, req.value);
            return {"length": new_length};
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

#! String APPEND request data type
public class RedisStringAppendRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringType,
                "desc": "The key to operate on",
            },
            "operation": {
                "type": StringOrNothingType,
                "desc": "The operation to perform: 'append' (default), 'strlen', 'getrange', or 'setrange'",
            },
            "value": {
                "type": StringOrNothingType,
                "desc": "The value to append or set (for 'append' and 'setrange' operations)",
            },
            "start": {
                "type": IntOrNothingType,
                "desc": "Start offset for 'getrange' operation",
            },
            "end": {
                "type": IntOrNothingType,
                "desc": "End offset for 'getrange' operation",
            },
            "offset": {
                "type": IntOrNothingType,
                "desc": "Offset for 'setrange' operation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! String APPEND response data type
public class RedisStringAppendResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "length": {
                "type": IntOrNothingType,
                "desc": "The new length of the string after the operation (for APPEND, STRLEN, SETRANGE)",
            },
            "value": {
                "type": StringOrNothingType,
                "desc": "The substring value (for GETRANGE operation)",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
