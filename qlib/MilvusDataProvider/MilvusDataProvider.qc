# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MilvusDataProvider class definitions

/** MilvusDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MilvusDataProvider {
public class MilvusDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Milvus server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Milvus server.",
                "default_value": "http://localhost:19530",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Milvus / Zilliz Bearer token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Milvus / Zilliz Cloud Bearer token.",
                "required": True,
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
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Milvus API communication";
            }
            rest = new MilvusRestClient::MilvusRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

public class MilvusDataProvider inherits MilvusDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "milvus",
            "type": "MilvusDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "list-collections":
                Class::forName("MilvusDataProvider::MilvusListCollectionsDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : MilvusDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : MilvusDataProviderCommon(options) {}

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

public class MilvusDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("MilvusDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "milvus",
            "desc": "Milvus / Zilliz Cloud vector DB data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return MilvusDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class MilvusListCollectionsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "data": {
                "display_name": "Collections",
                "desc": "List of Milvus collection names.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "code": {
                "display_name": "Code",
                "desc": "Milvus response code (0 = success).",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MilvusListCollectionsDataProvider inherits MilvusDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-collections",
            "type": "MilvusListCollectionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static MilvusListCollectionsResponseDataType ResponseType =
            new MilvusListCollectionsResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : MilvusDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : MilvusDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return NOTHING; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Milvus REST v2: POST /collections/list with empty body
        return rest.restPost("/collections/list", {}).body;
    }
}
}
