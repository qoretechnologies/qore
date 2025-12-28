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
            "desc": "Redis hash GET API data provider; gets hash field values (HGET, HMGET, HGETALL, HKEYS, HVALS)",
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
                "type": StringType,
                "desc": "The hash key to operate on",
            },
            "operation": {
                "type": StringOrNothingType,
                "desc": "The operation: 'hget' (default), 'hmget', 'hgetall', 'hkeys', 'hvals', 'hlen'",
            },
            "field": {
                "type": StringOrNothingType,
                "desc": "The field name for HGET operation",
            },
            "fields": {
                "type": SoftListOrNothingType,
                "desc": "The list of field names for HMGET operation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}

#! Hash GET response data type
public class RedisHashGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "value": {
                "type": AutoType,
                "desc": "The field value for HGET operation",
            },
            "values": {
                "type": SoftListOrNothingType,
                "desc": "The field values for HMGET/HVALS operations",
            },
            "hash": {
                "type": AutoHashOrNothingType,
                "desc": "The full hash for HGETALL operation",
            },
            "keys": {
                "type": SoftListOrNothingType,
                "desc": "The field names for HKEYS operation",
            },
            "length": {
                "type": IntOrNothingType,
                "desc": "The number of fields for HLEN operation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator();
    }
}
}
