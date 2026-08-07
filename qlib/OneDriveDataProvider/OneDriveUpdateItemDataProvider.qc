# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveUpdateItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for updating/renaming/moving an item
public class OneDriveUpdateItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to update",
                "desc": "The unique identifier of the item to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
                "ref_data": "items",
            },
            "name": {
                "display_name": "New Name",
                "short_desc": "The new name for the item",
                "desc": "The new name for the file or folder (including extension for files)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "renamed-document.pdf",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "A user-visible description of the item",
                "desc": "Provides a user-visible description of the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parent_id": {
                "display_name": "New Parent Folder ID",
                "short_desc": "The destination folder ID for moving",
                "desc": "The ID of the destination folder to move the item to. Set this to move an item "
                    "to a different folder",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": "folders",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveUpdateItemRequestDataType = new OneDriveUpdateItemRequestDataType();

#! Data provider for updating/renaming/moving an item
public class OneDriveUpdateItemDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-item",
            "type": "OneDriveUpdateItemDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveUpdateItemRequestDataType;
        const ResponseType = OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};
        if (req.name) {
            body.name = req.name;
        }
        if (req.description) {
            body.description = req.description;
        }
        if (req.parent_id) {
            body.parentReference = {
                "id": req.parent_id,
            };
        }
        if (!body) {
            throw "ONEDRIVE-UPDATE-ITEM-ERROR", "at least one of 'name', 'description', or 'parent_id' "
                "must be provided";
        }

        return doRestCommand("PATCH", getDriveBasePath() + "/items/" + req.id, body).body;
    }
}
}
