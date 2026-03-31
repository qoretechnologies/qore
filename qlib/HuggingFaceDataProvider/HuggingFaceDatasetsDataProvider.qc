# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceDatasetsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Container provider for Hugging Face Datasets API
public class HuggingFaceDatasetsDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "datasets",
            "type": "HuggingFaceDatasetsDataProvider",
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
            "list": Class::forName("HuggingFaceDataProvider::HuggingFaceDatasetListDataProvider"),
            "get": Class::forName("HuggingFaceDataProvider::HuggingFaceDatasetGetDataProvider"),
            "create": Class::forName("HuggingFaceDataProvider::HuggingFaceDatasetCreateDataProvider"),
            "update": Class::forName("HuggingFaceDataProvider::HuggingFaceDatasetUpdateDataProvider"),
            "delete": Class::forName("HuggingFaceDataProvider::HuggingFaceDatasetDeleteDataProvider"),
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

#! Lists Hugging Face datasets
public class HuggingFaceDatasetListDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "HuggingFaceDatasetListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        static HuggingFaceListRequestDataType RequestType();
        const ResponseType = new ListDataType("HuggingFaceDatasetList", HuggingFaceDatasetDataType, True);
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
        if (req.full) { params.full = req.full; }
        string path = "datasets";
        if (params) {
            path += "?" + (foldl $1 + "&" + $2, (map sprintf("%s=%s",
                encode_url($1.key, True), encode_url(string($1.value), True)),
                params.pairIterator()));
        }
        return doRestCommand("GET", path).body;
    }
}

#! Gets a specific Hugging Face dataset
public class HuggingFaceDatasetGetDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "HuggingFaceDatasetGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceDatasetIdRequestDataType;
        const ResponseType = HuggingFaceDatasetDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "datasets/" + req.repo_id).body;
    }
}

#! Creates a new Hugging Face dataset repository
public class HuggingFaceDatasetCreateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "HuggingFaceDatasetCreateDataProvider",
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
        return doRestCommand("POST", "repos/create", req + {"type": "dataset"}).body;
    }
}

#! Updates a Hugging Face dataset repository settings
public class HuggingFaceDatasetUpdateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "type": "HuggingFaceDatasetUpdateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceDatasetUpdateRequestDataType;
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
        return doRestCommand("PUT", "datasets/" + repo_id + "/settings", body).body;
    }
}

#! Deletes a Hugging Face dataset repository
public class HuggingFaceDatasetDeleteDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "HuggingFaceDatasetDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceDatasetDeleteRequestDataType;
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
            HuggingFaceDataProviderCommon::makeDeleteBody("dataset", req.repo_id)).body;
    }
}
}
