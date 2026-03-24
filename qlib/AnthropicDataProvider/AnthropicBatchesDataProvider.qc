# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicBatchesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Container provider for Anthropic Message Batches API
public class AnthropicBatchesDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batches",
            "type": "AnthropicBatchesDataProvider",
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
            "create": Class::forName("AnthropicDataProvider::AnthropicBatchCreateDataProvider"),
            "get": Class::forName("AnthropicDataProvider::AnthropicBatchGetDataProvider"),
            "list": Class::forName("AnthropicDataProvider::AnthropicBatchListDataProvider"),
            "cancel": Class::forName("AnthropicDataProvider::AnthropicBatchCancelDataProvider"),
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

#! Creates a message batch
public class AnthropicBatchCreateDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "AnthropicBatchCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicCreateBatchRequestDataType;
        const ResponseType = AnthropicBatchResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "messages/batches", req).body;
    }
}

#! Gets a specific message batch by ID
public class AnthropicBatchGetDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "AnthropicBatchGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = AnthropicBatchResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return AnthropicBatchIdRequestDataType;
    }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "messages/batches/" + req.batch_id).body;
    }
}

#! Lists message batches
public class AnthropicBatchListDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "AnthropicBatchListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = new ListDataType("AnthropicBatchList", AnthropicBatchResponseDataType, True);
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return AnthropicBatchListRequestDataType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params = {};
        if (req.limit) { params.limit = req.limit; }
        if (req.after_id) { params.after_id = req.after_id; }
        if (req.before_id) { params.before_id = req.before_id; }
        string path = "messages/batches";
        if (params) {
            path += "?" + (map sprintf("%s=%s", $1.key, $1.value), params.pairIterator()).join("&");
        }
        return doRestCommand("GET", path).body."data";
    }
}

#! Cancels a message batch
public class AnthropicBatchCancelDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel",
            "type": "AnthropicBatchCancelDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = AnthropicBatchResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return AnthropicBatchIdRequestDataType;
    }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "messages/batches/" + req.batch_id + "/cancel").body;
    }
}

#! Simple request type with just batch_id
public class AnthropicBatchIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch ID",
                "short_desc": "The ID of the message batch",
                "desc": "The unique identifier for the message batch (e.g. `msgbatch_013Zva2CMbUFEYkeWyl6QV`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "msgbatch_013Zva2CMbUFEYkeWyl6QV",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicBatchIdRequestDataType = new AnthropicBatchIdRequestDataType();

#! List batches request type with pagination
public class AnthropicBatchListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of batches to return",
                "desc": "Maximum number of batches to return per page. Default: `20`, max: `100`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 20,
            },
            "after_id": {
                "display_name": "After ID",
                "short_desc": "Cursor for forward pagination",
                "desc": "Return batches created after this batch ID (cursor for forward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "before_id": {
                "display_name": "Before ID",
                "short_desc": "Cursor for backward pagination",
                "desc": "Return batches created before this batch ID (cursor for backward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicBatchListRequestDataType = new AnthropicBatchListRequestDataType();
}
