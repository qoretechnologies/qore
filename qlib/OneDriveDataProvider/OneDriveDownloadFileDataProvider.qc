# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDownloadFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for downloading a file
public class OneDriveDownloadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to download",
                "desc": "The unique identifier of the file to download",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
                "ref_data": "items",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveDownloadFileRequestDataType = new OneDriveDownloadFileRequestDataType();

#! Data provider for downloading a file
/** Downloads a file by first getting its metadata (which includes the pre-authenticated download URL),
    then returning the download URL and metadata.
*/
public class OneDriveDownloadFileDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "OneDriveDownloadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveDownloadFileRequestDataType;
        const ResponseType = OneDriveDownloadResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Get item metadata which includes the pre-authenticated download URL
        hash<auto> item = doRestCommand("GET", getDriveBasePath(req) + "/items/" + req.id).body;
        return {
            "name": item.name,
            "size": item.size,
            "content_type": item.file.mimeType,
            "download_url": item."@microsoft.graph.downloadUrl",
        };
    }
}
}
