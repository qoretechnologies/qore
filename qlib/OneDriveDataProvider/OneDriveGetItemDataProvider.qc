# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveGetItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for getting an item
public class OneDriveGetItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to retrieve",
                "desc": "The unique identifier of the item to retrieve. Provide either `id` or `path`",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
                "ref_data": "items",
            },
            "path": {
                "display_name": "Item Path",
                "short_desc": "The item path to retrieve",
                "desc": "The path of the item relative to the drive root (e.g., `/Documents/file.txt`). "
                    "Provide either `id` or `path`",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/Documents/report.pdf",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveGetItemRequestDataType = new OneDriveGetItemRequestDataType();

#! Data provider for getting an item by ID or path
public class OneDriveGetItemDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-item",
            "type": "OneDriveGetItemDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveGetItemRequestDataType;
        const ResponseType = OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path;
        if (req.id) {
            path = "me/drive/items/" + req.id;
        } else if (req.path) {
            string item_path = req.path;
            if (item_path !~ /^\//) {
                item_path = "/" + item_path;
            }
            path = "me/drive/root:" + item_path;
        } else {
            throw "ONEDRIVE-GET-ITEM-ERROR", "either 'id' or 'path' must be provided";
        }
        return doRestCommand("GET", path).body;
    }
}
}
