# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PineconeDataProvider class definitions

/** PineconeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace PineconeDataProvider {
public class PineconeDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Pinecone API key (Api-Key header)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Pinecone API key — sent as the `Api-Key` header.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Control-Plane URL",
                "short_desc": "Pinecone control-plane URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Pinecone control plane.",
                "default_value": "https://api.pinecone.io",
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
                    "missing required 'apikey' argument for Pinecone API communication";
            }
            rest = new PineconeRestClient::PineconeRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

public class PineconeDataProvider inherits PineconeDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "pinecone",
            "type": "PineconeDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "list-indexes":
                Class::forName("PineconeDataProvider::PineconeListIndexesDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : PineconeDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : PineconeDataProviderCommon(options) {}

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

public class PineconeDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("PineconeDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "pinecone",
            "desc": "Pinecone vector DB data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return PineconeDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class PineconeIndexInfoDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {"display_name": "Name", "desc": "Index name.",
                "type": AbstractDataProviderTypeMap."*string"},
            "host": {"display_name": "Host", "desc": "Data-plane host.",
                "type": AbstractDataProviderTypeMap."*string"},
            "dimension": {"display_name": "Dimension", "desc": "Vector dimension.",
                "type": AbstractDataProviderTypeMap."*int"},
            "metric": {"display_name": "Metric", "desc": "Distance metric (cosine, euclidean, dotproduct).",
                "type": AbstractDataProviderTypeMap."*string"},
            "status": {"display_name": "Status", "desc": "Index status hash.",
                "type": AbstractDataProviderTypeMap."*hash"},
            "spec": {"display_name": "Spec", "desc": "Index deployment spec (serverless or pod-based).",
                "type": AbstractDataProviderTypeMap."*hash"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class PineconeListIndexesResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "indexes": {
                "display_name": "Indexes",
                "desc": "List of Pinecone indexes for the current account.",
                "type": new ListDataType("PineconeIndexInfoList",
                    new PineconeIndexInfoDataType(), True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class PineconeListIndexesDataProvider inherits PineconeDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-indexes",
            "type": "PineconeListIndexesDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static PineconeListIndexesResponseDataType ResponseType =
            new PineconeListIndexesResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : PineconeDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : PineconeDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return NOTHING; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return rest.restGet("/indexes").body;
    }
}
}
