# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisSetDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The Redis set operations data provider
public class RedisSetDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sets",
            "desc": "Redis set data provider; provides access to set operations (SADD, SREM, SMEMBERS, etc.)",
            "type": "RedisSetDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            "add": Class::forName("RedisDataProvider::RedisSetAddDataProvider"),
            "ops": Class::forName("RedisDataProvider::RedisSetOpsDataProvider"),
            "card": Class::forName("RedisDataProvider::RedisSetCardDataProvider"),
        };
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis set operations provider for `%s`", redis.getSafeURL()); }

    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        return cls ? cls.newObject(redis) : NOTHING;
    }

    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
