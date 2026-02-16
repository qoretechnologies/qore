# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantDataProvider class definition

/** QdrantDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! Qdrant app name
const AppName = "Qdrant";

#! The Qdrant data provider class
public class QdrantDataProvider inherits QdrantDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QdrantDataProvider",
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
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client for accessing the Qdrant server",
                "type": AbstractDataProviderType::get(new Type("RestClient")),
                "desc": "The REST client for accessing the Qdrant server",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the Qdrant server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the Qdrant REST server; overrides any URL in any RestClient object passed "
                    "as an option",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options to any REST client created",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the `RestClient` constructor; only used if a `RestClient` object is created for "
                    "a call to the Qdrant server",
            },
        };
    }

    private {
        const ChildMap = {
            "collections": Class::forName("QdrantDataProvider::QdrantCollectionsDataProvider"),
            "aliases": Class::forName("QdrantDataProvider::QdrantAliasesDataProvider"),
            "points": Class::forName("QdrantDataProvider::QdrantPointsDataProvider"),
            "search": Class::forName("QdrantDataProvider::QdrantSearchDataProvider"),
            "snapshots": Class::forName("QdrantDataProvider::QdrantSnapshotsDataProvider"),
            "cluster": Class::forName("QdrantDataProvider::QdrantClusterDataProvider"),
            "service": Class::forName("QdrantDataProvider::QdrantServiceDataProvider"),
            "tables": Class::forName("QdrantDataProvider::QdrantTablesDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = QdrantDataProvider::getRestConnection(copts);
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a REST connection from the given options
    static RestClient::RestClient getRestConnection(*hash<auto> options) {
        RestClient rest;
        if (options.restclient) {
            rest = options.restclient;
            if (options.url) {
                rest.setURL(options.url);
            }
        } else {
            if (!options.url && !options.restclient_options) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient', 'url', or 'restclient_options' option passed; "
                    "cannot create REST client to Qdrant instance without a URL";
            }

            hash<auto> opts;
            opts += options.restclient_options;
            if (options.url) {
                opts.url = options.url;
            }
            rest = new RestClient(opts);
        }
        return rest;
    }

    #! Returns the data provider name
    string getName() {
        return "qdrant";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Qdrant data provider for `%s`", rest.getSafeURL());
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
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
