# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WeaviateDataProvider class definitions

/** WeaviateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace WeaviateDataProvider {
public class WeaviateDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Weaviate server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Weaviate server.",
                "default_value": "http://localhost:8080",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Optional Bearer token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional API key (Weaviate Cloud Services / proxied).",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
        };
    }

    private {
        *RestClientIo::RestClientIo rest;
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            hash<auto!> opts = {
                "url": copts.url ?? "http://localhost:8080",
            };
            if (copts.apikey.val()) {
                opts.apikey = copts.apikey;
            }
            rest = new WeaviateRestClient::WeaviateRestClientIo(opts);
        }
    }
}

public class WeaviateDataProvider inherits WeaviateDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "weaviate",
            "type": "WeaviateDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "list-classes":
                Class::forName("WeaviateDataProvider::WeaviateListClassesDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : WeaviateDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : WeaviateDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys childMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        return cls ? cls.newObject(rest) : NOTHING;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class WeaviateDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("WeaviateDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "weaviate",
            "desc": "Weaviate vector DB data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return WeaviateDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class WeaviateListClassesResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "classes": {
                "display_name": "Classes",
                "desc": "List of Weaviate schema classes.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class WeaviateListClassesDataProvider inherits WeaviateDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-classes",
            "type": "WeaviateListClassesDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static WeaviateListClassesResponseDataType ResponseType =
            new WeaviateListClassesResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : WeaviateDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : WeaviateDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return NOTHING; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return rest.restGet("/schema").body;
    }
}
}
