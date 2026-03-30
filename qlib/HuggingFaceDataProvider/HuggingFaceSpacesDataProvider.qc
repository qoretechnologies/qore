# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceSpacesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Container provider for Hugging Face Spaces API
public class HuggingFaceSpacesDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "spaces",
            "type": "HuggingFaceSpacesDataProvider",
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
            "list": Class::forName("HuggingFaceDataProvider::HuggingFaceSpaceListDataProvider"),
            "get": Class::forName("HuggingFaceDataProvider::HuggingFaceSpaceGetDataProvider"),
            "create": Class::forName("HuggingFaceDataProvider::HuggingFaceSpaceCreateDataProvider"),
            "update": Class::forName("HuggingFaceDataProvider::HuggingFaceSpaceUpdateDataProvider"),
            "delete": Class::forName("HuggingFaceDataProvider::HuggingFaceSpaceDeleteDataProvider"),
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

#! Lists Hugging Face Spaces
public class HuggingFaceSpaceListDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "HuggingFaceSpaceListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        static HuggingFaceSpaceListRequestDataType RequestType();
        const ResponseType = new ListDataType("HuggingFaceSpaceList", HuggingFaceSpaceDataType, True);
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
        string path = "spaces";
        if (params) {
            path += "?" + (foldl $1 + "&" + $2, (map sprintf("%s=%s",
                encode_url($1.key, True), encode_url(string($1.value), True)),
                params.pairIterator()));
        }
        return doRestCommand("GET", path).body;
    }
}

#! Gets a specific Hugging Face Space
public class HuggingFaceSpaceGetDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "HuggingFaceSpaceGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceSpaceIdRequestDataType;
        const ResponseType = HuggingFaceSpaceDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "spaces/" + req.repo_id).body;
    }
}

#! Creates a new Hugging Face Space
public class HuggingFaceSpaceCreateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "HuggingFaceSpaceCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceSpaceCreateRequestDataType;
        const ResponseType = HuggingFaceRepoResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : HuggingFaceDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : HuggingFaceDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # remap space_sdk -> sdk for the API
        hash<auto> body = req - ("space_sdk", "space_hardware") + {
            "type": "space",
            "sdk": req.space_sdk,
        };
        hash<auto> result = doRestCommand("POST", "repos/create", body).body;

        # set hardware via separate endpoint if requested
        if (req.space_hardware) {
            # determine repo_id from the create response or request
            string repo_id;
            if (req.organization) {
                repo_id = req.organization + "/" + req.name;
            } else {
                # extract from the returned URL
                *string repo_url = result.repoUrl ?? result.url;
                if (repo_url) {
                    list<string> parts = repo_url.split("/");
                    if (parts.size() >= 2) {
                        repo_id = parts[parts.size() - 2] + "/" + parts[parts.size() - 1];
                    }
                }
            }
            if (repo_id) {
                doRestCommand("POST", "spaces/" + repo_id + "/hardware",
                    {"flavor": req.space_hardware});
            }
        }
        return result;
    }
}

#! Updates a Hugging Face Space settings
public class HuggingFaceSpaceUpdateDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "type": "HuggingFaceSpaceUpdateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceSpaceUpdateRequestDataType;
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
        return doRestCommand("PUT", "spaces/" + repo_id + "/settings", body).body;
    }
}

#! Deletes a Hugging Face Space
public class HuggingFaceSpaceDeleteDataProvider inherits HuggingFaceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "HuggingFaceSpaceDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = HuggingFaceSpaceDeleteRequestDataType;
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
            HuggingFaceDataProviderCommon::makeDeleteBody("space", req.repo_id)).body;
    }
}
}
