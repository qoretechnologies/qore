# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis data provider class
public class RedisDataProvider inherits RedisDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "RedisDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": NOTHING,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "transaction_management": False,
            "supports_schema": False,
            "children_can_support_apis": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "redis": <DataProviderOptionInfo>{
                "display_name": "Redis Client",
                "short_desc": "The Redis client for accessing the Redis server",
                "type": AbstractDataProviderType::get(new Type("RedisDataProvider::RedisClient")),
                "desc": "The Redis client for accessing the Redis server",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the Redis server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the Redis server; overrides any URL in any RedisClient object passed as an option",
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>({
            "name": "redis",
            "desc": "Redis data provider; parent provider for Redis operation APIs",
            "type": "RedisDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        });
    }

    private {
        #! Child provider map
        const ChildMap = {
            "strings": Class::forName("RedisDataProvider::RedisStringDataProvider"),
            "hashes": Class::forName("RedisDataProvider::RedisHashDataProvider"),
            "lists": Class::forName("RedisDataProvider::RedisListDataProvider"),
            "sets": Class::forName("RedisDataProvider::RedisSetDataProvider"),
            "sortedsets": Class::forName("RedisDataProvider::RedisSortedSetDataProvider"),
            "keys": Class::forName("RedisDataProvider::RedisKeyDataProvider"),
            "server": Class::forName("RedisDataProvider::RedisServerDataProvider"),
            "pubsub": Class::forName("RedisDataProvider::RedisPubSubDataProvider"),
            "streams": Class::forName("RedisDataProvider::RedisStreamDataProvider"),
            "transactions": Class::forName("RedisDataProvider::RedisTransactionDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(copts);
    }

    #! Creates the object from a RedisClient
    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    #! Returns a Redis client from constructor options
    static RedisClient getRedisClient(*hash<auto> options) {
        RedisClient redis;
        if (options.redis) {
            redis = options.redis;
        } else if (options.url) {
            redis = new RedisClient(options.url);
            redis.connect();
        } else {
            throw "CONSTRUCTOR-ERROR", "no 'redis' or 'url' option passed; cannot create Redis client";
        }
        return redis;
    }

    #! Returns the data provider name
    string getName() {
        return "redis";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Redis data provider for `%s`", redis.getSafeURL());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(redis);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
