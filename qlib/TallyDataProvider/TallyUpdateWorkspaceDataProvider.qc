# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyUpdateWorkspaceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Request type for updating a workspace
public class TallyUpdateWorkspaceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID to update",
                "desc": "The unique identifier of the workspace to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Updated workspace name",
                "desc": "The new display name for the workspace",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const TallyUpdateWorkspaceRequestDataType = new TallyUpdateWorkspaceRequestDataType();

#! Update result data type for workspace updates (API returns 204)
public class TallyUpdateWorkspaceResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Update status",
                "desc": "The result status of the update operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "Workspace ID",
                "short_desc": "The updated workspace ID",
                "desc": "The ID of the updated workspace",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyUpdateWorkspaceResultDataType = new TallyUpdateWorkspaceResultDataType();

#! Data provider for updating a workspace
public class TallyUpdateWorkspaceDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-workspace",
            "type": "TallyUpdateWorkspaceDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = TallyUpdateWorkspaceRequestDataType;
        const ResponseType = TallyUpdateWorkspaceResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {}
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        doRestCommand("PATCH", "workspaces/" + id, body);
        return {
            "status": "updated",
            "id": id,
        };
    }
}
}
