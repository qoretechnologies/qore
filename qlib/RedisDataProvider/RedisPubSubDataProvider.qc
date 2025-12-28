# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisPubSubDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis pub/sub data provider
public class RedisPubSubDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "pubsub",
            "desc": "Redis pub/sub data provider; publish messages to channels (PUBLISH, PUBSUB)",
            "type": "RedisPubSubDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisPubSubRequestDataType();
        const ResponseType = new RedisPubSubResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis pub/sub provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "pubsub_channels":
                list<auto> channels = redis.commandWithArgs("PUBSUB", ("CHANNELS",)
                    + (req.pattern ? (req.pattern,) : ()));
                return {"channels": channels};

            case "pubsub_numsub":
                list<auto> result = redis.commandWithArgs("PUBSUB", ("NUMSUB",) + req.channels);
                hash<auto> subs = {};
                for (int i = 0; i < result.size(); i += 2) {
                    subs{result[i]} = result[i + 1];
                }
                return {"subscribers": subs};

            case "pubsub_numpat":
                int count = redis.command("PUBSUB", "NUMPAT");
                return {"pattern_count": count};

            default:  # publish
                int receivers = redis.command("PUBLISH", req.channel, req.message);
                return {"receivers": receivers};
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisPubSubRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'publish' (default), 'pubsub_channels', 'pubsub_numsub', 'pubsub_numpat'"},
            "channel": {"type": StringOrNothingType, "desc": "Channel name for PUBLISH"},
            "channels": {"type": SoftListOrNothingType, "desc": "Channel names for PUBSUB NUMSUB"},
            "message": {"type": AutoType, "desc": "Message to publish"},
            "pattern": {"type": StringOrNothingType, "desc": "Pattern for PUBSUB CHANNELS"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisPubSubResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "receivers": {"type": IntOrNothingType, "desc": "Number of clients that received the message"},
            "channels": {"type": SoftListOrNothingType, "desc": "Active channels matching pattern"},
            "subscribers": {"type": AutoHashOrNothingType, "desc": "Channel subscriber counts"},
            "pattern_count": {"type": IntOrNothingType, "desc": "Number of pattern subscriptions"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
