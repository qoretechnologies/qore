# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedDataProvider class definition

/** MemcachedDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

public namespace MemcachedDataProvider {
#! Application name for the data provider
public const AppName = "Memcached";

#! Root memcached data provider class
public class MemcachedDataProvider inherits MemcachedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MemcachedDataProvider",
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
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "memcached": <DataProviderOptionInfo>{
                "display_name": "Memcached Client",
                "short_desc": "The memcached client for accessing the server",
                "type": AbstractDataProviderType::get(new Type("MemcachedClient")),
                "desc": "The memcached client object for accessing the memcached server",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "URL to the memcached server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the memcached server in the format memcached://host:port or memcacheds://host:port for SSL",
                "example_value": "memcached://localhost:11211",
            },
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "Connection timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Connection timeout in milliseconds (default: 30000)",
                "default_value": 30000,
            },
            "io_timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "Read/write timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "I/O timeout in milliseconds for read/write operations (default: 15000)",
                "default_value": 15000,
            },
        };
    }

    private {
        #! Child provider map
        const ChildMap = {
            "storage": Class::forName("MemcachedDataProvider::MemcachedStorageDataProvider"),
            "retrieval": Class::forName("MemcachedDataProvider::MemcachedRetrievalDataProvider"),
            "delete": Class::forName("MemcachedDataProvider::MemcachedDeleteDataProvider"),
            "touch": Class::forName("MemcachedDataProvider::MemcachedTouchDataProvider"),
            "numeric": Class::forName("MemcachedDataProvider::MemcachedNumericDataProvider"),
            "stats": Class::forName("MemcachedDataProvider::MemcachedStatsDataProvider"),
            "admin": Class::forName("MemcachedDataProvider::MemcachedAdminDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        memcached = MemcachedDataProvider::getMemcachedClient(copts);
    }

    #! Creates the object from a MemcachedClient
    constructor(MemcachedClient memcached) : MemcachedDataProviderBase(memcached) {
    }

    #! Returns the name of the data provider
    string getName() {
        return "memcached";
    }

    #! Returns a description of the data provider
    *string getDesc() {
        return sprintf("Memcached data provider for `%s`", memcached.getSafeURL());
    }

    #! Returns child provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(memcached);
    }

    #! Returns provider info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a MemcachedClient from constructor options
    static MemcachedClient getMemcachedClient(hash<auto> options) {
        if (options.memcached) {
            return options.memcached;
        }
        if (!options.url) {
            throw "MEMCACHED-CONSTRUCTOR-ERROR", "either 'memcached' or 'url' option is required";
        }
        hash<auto> client_opts = {
            "url": options.url,
        };
        if (options.connect_timeout) {
            client_opts.connect_timeout = options.connect_timeout;
        }
        if (options.io_timeout) {
            client_opts.io_timeout = options.io_timeout;
        }
        MemcachedClient client(client_opts);
        client.connect();
        return client;
    }
}
}
