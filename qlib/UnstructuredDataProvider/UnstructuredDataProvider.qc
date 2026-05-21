# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnstructuredDataProvider class definitions

/** UnstructuredDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace UnstructuredDataProvider {
public class UnstructuredDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Unstructured.io API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Unstructured.io API key (sent as `unstructured-api-key` header).",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Unstructured.io REST API URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Unstructured.io REST API.",
                "default_value": "https://api.unstructuredapp.io",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("UnstructuredRestClient::UnstructuredRestClientIo"),
                    NOTHING, {DTT_ClientOnly: True}),
                "desc": "The Unstructured REST client to use",
            },
        };
    }

    private {
        *UnstructuredRestClient::UnstructuredRestClientIo rest;
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Unstructured API "
                    "communication";
            }
            rest = new UnstructuredRestClient::UnstructuredRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

public class UnstructuredDataProvider inherits UnstructuredDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unstructured",
            "type": "UnstructuredDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "partition-text":
                Class::forName("UnstructuredDataProvider::UnstructuredPartitionTextDataProvider"),
        };
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest)
            : UnstructuredDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : UnstructuredDataProviderCommon(options) {}

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

public class UnstructuredDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("UnstructuredDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "unstructured",
            "desc": "Unstructured.io data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return UnstructuredDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class UnstructuredPartitionTextRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "Document text content to partition",
                "desc": "Raw text content of the document to partition.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "strategy": {
                "display_name": "Strategy",
                "short_desc": "Partitioning strategy",
                "desc": "Partitioning strategy — `auto`, `fast`, `hi_res`, or `ocr_only`.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
                    <AllowedValueInfo>{"value": "fast", "display_name": "Fast"},
                    <AllowedValueInfo>{"value": "hi_res", "display_name": "Hi-Res"},
                    <AllowedValueInfo>{"value": "ocr_only", "display_name": "OCR Only"},
                ),
            },
            "chunking_strategy": {
                "display_name": "Chunking Strategy",
                "short_desc": "Server-side chunking strategy",
                "desc": "Server-side chunking strategy — `by_title` or `basic`.  When "
                    "set, Unstructured groups elements into chunks server-side.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "by_title", "display_name": "By Title"},
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                ),
            },
            "languages": {
                "display_name": "Languages",
                "short_desc": "Document language hints (ISO codes)",
                "desc": "List of ISO language codes for the document.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "coordinates": {
                "display_name": "Coordinates",
                "short_desc": "Include layout coordinates in each element",
                "desc": "When `True`, each element includes layout coordinates in "
                    "its metadata.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class UnstructuredElementDataType inherits HashDataType {
    private {
        const Fields = {
            "type": {"display_name": "Type",
                "desc": "Element type (Title, NarrativeText, ListItem, Table, "
                    "Image, etc.).",
                "type": AbstractDataProviderTypeMap."*string"},
            "text": {"display_name": "Text",
                "desc": "Element text content.",
                "type": AbstractDataProviderTypeMap."*string"},
            "element_id": {"display_name": "Element ID",
                "desc": "Unstructured-assigned element ID.",
                "type": AbstractDataProviderTypeMap."*string"},
            "metadata": {"display_name": "Metadata",
                "desc": "Per-element metadata (filename, page_number, "
                    "parent_id, languages, etc.).",
                "type": AbstractDataProviderTypeMap."*hash"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class UnstructuredPartitionTextResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "elt_list": {
                "display_name": "Elements",
                "desc": "List of typed elements returned by Unstructured.io.",
                "type": new ListDataType("UnstructuredElementList",
                    new UnstructuredElementDataType(), True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class UnstructuredPartitionTextDataProvider inherits UnstructuredDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "partition-text",
            "type": "UnstructuredPartitionTextDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static UnstructuredPartitionTextRequestDataType RequestType =
            new UnstructuredPartitionTextRequestDataType();
        static UnstructuredPartitionTextResponseDataType ResponseType =
            new UnstructuredPartitionTextResponseDataType();
    }

    constructor(UnstructuredRestClient::UnstructuredRestClientIo rest)
            : UnstructuredDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : UnstructuredDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string text = req.text;
        hash<auto> opts = req - "text";
        list<hash<auto>> elt_list = rest.partitionText(text, opts);
        return {"elt_list": elt_list};
    }
}
}
