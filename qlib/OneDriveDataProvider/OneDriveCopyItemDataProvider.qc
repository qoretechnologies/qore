# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveCopyItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! Request type for copying an item
public class OneDriveCopyItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to copy",
                "desc": "The unique identifier of the file or folder to copy",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
            },
            "name": {
                "display_name": "New Name",
                "short_desc": "New name for the copy",
                "desc": "The new name for the copied item. If not provided, the same name as the original "
                    "is used",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Document (copy).docx",
            },
            "parent_drive_id": {
                "display_name": "Destination Drive ID",
                "short_desc": "Target drive ID for the copy",
                "desc": "The unique identifier of the destination drive. Required when copying to a "
                    "different drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parent_id": {
                "display_name": "Destination Folder ID",
                "short_desc": "Target folder ID for the copy",
                "desc": "The ID of the destination folder. If omitted, the item is copied to the drive root",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "conflict_behavior": {
                "display_name": "Conflict Behavior",
                "short_desc": "How to handle naming conflicts",
                "desc": "The conflict resolution behavior if an item with the same name exists in the "
                    "destination:\n"
                    "- `rename`: automatically rename the new copy\n"
                    "- `replace`: replace the existing file (only supported for files, not folders)\n"
                    "- `fail`: fail the request with an error (default)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "rename",
                        "display_name": "Rename",
                        "desc": "Automatically rename the new copy",
                    },
                    <AllowedValueInfo>{
                        "value": "replace",
                        "display_name": "Replace",
                        "desc": "Replace the existing file",
                    },
                    <AllowedValueInfo>{
                        "value": "fail",
                        "display_name": "Fail",
                        "desc": "Fail the request with an error",
                    },
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveCopyItemRequestDataType = new OneDriveCopyItemRequestDataType();

#! Response type for copy item operation
public class OneDriveCopyItemResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Copy operation status",
                "desc": "The status of the copy operation (e.g., `accepted`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "monitor_url": {
                "display_name": "Monitor URL",
                "short_desc": "URL to monitor copy progress",
                "desc": "A URL to monitor the progress of the asynchronous copy operation. Use HTTP GET "
                    "on this URL to check the status, percentage complete, and final result",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveCopyItemResultDataType = new OneDriveCopyItemResultDataType();

#! Data provider for copying a file or folder
public class OneDriveCopyItemDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-item",
            "type": "OneDriveCopyItemDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveCopyItemRequestDataType;
        const ResponseType = OneDriveCopyItemResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = "me/drive/items/" + req.id + "/copy";
        if (req.conflict_behavior) {
            path += "?@microsoft.graph.conflictBehavior=" + req.conflict_behavior;
        }

        hash<auto> body = {};
        if (req.name) {
            body.name = req.name;
        }
        if (req.parent_id || req.parent_drive_id) {
            body.parentReference = {};
            if (req.parent_id) {
                body.parentReference.id = req.parent_id;
            }
            if (req.parent_drive_id) {
                body.parentReference.driveId = req.parent_drive_id;
            }
        }

        hash<auto> response = doRestCommand("POST", path, body);
        return {
            "status": "accepted",
            "monitor_url": response.headers.location ?? response.headers.Location,
        };
    }
}
}
