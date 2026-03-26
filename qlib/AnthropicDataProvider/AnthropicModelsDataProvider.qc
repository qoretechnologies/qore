# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicModelsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Model data type for Anthropic models
public class AnthropicModelDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Model ID",
                "short_desc": "Unique model identifier",
                "desc": "Unique model identifier (e.g. `claude-sonnet-4-6-20250514`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "display_name": {
                "display_name": "Display Name",
                "short_desc": "Human-readable model name",
                "desc": "A human-readable name for the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Object type",
                "desc": "Object type, always `model`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "RFC 3339 datetime when the model was created",
                "desc": "RFC 3339 datetime string when the model was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_input_tokens": {
                "display_name": "Max Input Tokens",
                "short_desc": "Maximum input context window size",
                "desc": "The maximum number of input tokens the model supports (context window size)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_tokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "Maximum output tokens the model can generate",
                "desc": "The maximum number of output tokens the model can generate in a single response",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AnthropicModelDataType = new AnthropicModelDataType();

#! Container provider for Anthropic Models API
public class AnthropicModelsDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "models",
            "type": "AnthropicModelsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            "list": Class::forName("AnthropicDataProvider::AnthropicModelListDataProvider"),
            "get": Class::forName("AnthropicDataProvider::AnthropicModelGetDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) { return; }
        return cls.newObject(rest);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Lists available models
public class AnthropicModelListDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "AnthropicModelListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = new ListDataType("AnthropicModelList", AnthropicModelDataType, True);
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return AnthropicModelListRequestDataType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params = {};
        if (req.limit) { params.limit = req.limit; }
        if (req.after_id) { params.after_id = req.after_id; }
        if (req.before_id) { params.before_id = req.before_id; }
        string path = "models";
        if (params) {
            path += "?" + (map sprintf("%s=%s", encode_url($1.key, True), encode_url(string($1.value), True)), params.pairIterator()).join("&");
        }
        return doRestCommand("GET", path).body."data";
    }
}

#! Model ID request type
public class AnthropicModelIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "The ID of the model to retrieve",
                "desc": "The unique identifier or alias for the model (e.g. `claude-sonnet-4-6-20250514`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "example_value": "claude-sonnet-4-6-20250514",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicModelIdRequestDataType = new AnthropicModelIdRequestDataType();

#! List models request type with pagination
public class AnthropicModelListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of models to return",
                "desc": "Maximum number of models to return per page. Default: `20`, range: `1`-`1000`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 20,
            },
            "after_id": {
                "display_name": "After ID",
                "short_desc": "Cursor for forward pagination",
                "desc": "Return models after this model ID (cursor for forward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "before_id": {
                "display_name": "Before ID",
                "short_desc": "Cursor for backward pagination",
                "desc": "Return models before this model ID (cursor for backward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicModelListRequestDataType = new AnthropicModelListRequestDataType();

#! Gets a specific model by ID
public class AnthropicModelGetDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "AnthropicModelGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicModelIdRequestDataType;
        const ResponseType = AnthropicModelDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "models/" + req.model_id).body;
    }
}
}
