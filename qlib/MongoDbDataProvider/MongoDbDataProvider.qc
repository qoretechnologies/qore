# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MongoDbDataProvider class definition

/** MongoDbDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MongoDbDataProvider {
#! MongoDB app name
public const AppName = "MongoDB";

#! The MongoDB data provider class
public class MongoDbDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MongoDbDataProvider",
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
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "client": <DataProviderOptionInfo>{
                "display_name": "MongoDB Client",
                "short_desc": "The MongoDB client object for accessing the server",
                "type": AbstractDataProviderType::get(new Type("MongoClient")),
                "desc": "The MongoDB client object for accessing the server",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the MongoDB server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the MongoDB server; overrides any URL in any MongoClient object passed "
                    "as an option",
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The MongoDB client
        MongoClient client;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.client) {
            client = copts.client;
        } else if (copts.url) {
            client = new MongoClient(copts.url);
        } else {
            throw "CONSTRUCTOR-ERROR", "no 'client' or 'url' option passed; cannot create MongoDB client without "
                "connection information";
        }
    }

    #! Creates the object from a MongoClient
    constructor(MongoClient client) {
        self.client = client;
    }

    #! Returns the MongoDB client
    MongoClient getClient() {
        return client;
    }

    #! Returns the data provider name
    string getName() {
        return "mongodb";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "MongoDB data provider";
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return (
            <DataProviderSummaryInfo>{
                "name": "databases",
                "desc": "MongoDB databases",
                "type": "MongoDbDatabasesDataProvider",
                "supports_children": True,
            },
        );
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return ("databases",);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "databases") {
            return new MongoDbDatabasesDataProvider(client);
        }
        return;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
