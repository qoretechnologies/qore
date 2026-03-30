# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceModelsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HuggingFaceDataProvider module
public namespace HuggingFaceDataProvider {
#! Container provider for Hugging Face Models API
public class HuggingFaceModelsDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "models",
            "type": "HuggingFaceModelsDataProvider",
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
            "list": Class::forName("HuggingFaceDataProvider::HuggingFaceModelListDataProvider"),
            "get": Class::forName("HuggingFaceDataProvider::HuggingFaceModelGetDataProvider"),
            "create": Class::forName("HuggingFaceDataProvider::HuggingFaceModelCreateDataProvider"),
            "update": Class::forName("HuggingFaceDataProvider::HuggingFaceModelUpdateDataProvider"),
            "delete": Class::forName("HuggingFaceDataProvider::HuggingFaceModelDeleteDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
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

#! Lists Hugging Face models
public class HuggingFaceModelListDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "HuggingFaceModelListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        static HuggingFaceModelListRequestDataType RequestType();
        const ResponseType = new ListDataType("HuggingFaceModelList", HuggingFaceModelDataType, True);
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params = {};
        if (req.search) { params.search = req.search; }
        if (req.author) { params.author = req.author; }
        if (req.filter) { params.filter = (foldl $1 + "," + $2, req.filter); }
        if (req.sort) { params.sort = req.sort; }
        if (req.direction) { params.direction = req.direction; }
        if (req.limit) { params.limit = req.limit; }
        if (req.pipeline_tag) { params.pipeline_tag = req.pipeline_tag; }
        if (req.full) { params.full = req.full; }
        if (req.config) { params.config = req.config; }
        if (req.cardData) { params.cardData = req.cardData; }
        string path = "models";
        if (params) {
            path += "?" + (foldl $1 + "&" + $2, (map sprintf("%s=%s",
                encode_url($1.key, True), encode_url(string($1.value), True)),
                params.pairIterator()));
        }
        return doRestCommand("GET", path).body;
    }
}

#! Gets a specific Hugging Face model
public class HuggingFaceModelGetDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "HuggingFaceModelGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceModelIdRequestDataType;
        const ResponseType = HuggingFaceModelDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "models/" + req.repo_id).body;
    }
}

#! Creates a new Hugging Face model repository
public class HuggingFaceModelCreateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "HuggingFaceModelCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceRepoCreateRequestDataType;
        const ResponseType = HuggingFaceRepoResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "repos/create", req + {"type": "model"}).body;
    }
}

#! Updates a Hugging Face model repository settings
public class HuggingFaceModelUpdateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "type": "HuggingFaceModelUpdateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceModelUpdateRequestDataType;
        const ResponseType = HuggingFaceRepoResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string repo_id = req.repo_id;
        hash<auto> body = req - "repo_id";
        return doRestCommand("PUT", "models/" + repo_id + "/settings", body).body;
    }
}

#! Deletes a Hugging Face model repository
public class HuggingFaceModelDeleteDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "HuggingFaceModelDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceModelDeleteRequestDataType;
        const ResponseType = HuggingFaceRepoResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("DELETE", "repos/delete",
            HuggingFaceDataProviderCommon::makeDeleteBody("model", req.repo_id)).body;
    }
}
}
