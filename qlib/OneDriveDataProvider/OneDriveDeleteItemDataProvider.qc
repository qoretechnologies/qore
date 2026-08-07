# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDeleteItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for deleting an item
public class OneDriveDeleteItemRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to delete",
                "desc": "The unique identifier of the item to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
                "ref_data": "items",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveDeleteItemRequestDataType = new OneDriveDeleteItemRequestDataType();

#! Data provider for deleting an item
public class OneDriveDeleteItemDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-item",
            "type": "OneDriveDeleteItemDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveDeleteItemRequestDataType;
        const ResponseType = OneDriveDeleteResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doRestCommand("DELETE", getDriveBasePath(req) + "/items/" + req.id);
        return {
            "status": "deleted",
            "id": req.id,
        };
    }
}
}
