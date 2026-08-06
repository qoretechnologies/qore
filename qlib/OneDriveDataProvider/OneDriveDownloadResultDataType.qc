# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDownloadResultDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OneDrive download result data type
public class OneDriveDownloadResultDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "File Name",
                "short_desc": "The original file name",
                "desc": "The original name of the downloaded file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size of the file in bytes",
                "desc": "The size of the downloaded file content in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type of the file",
                "desc": "The MIME type of the file content (e.g., `application/pdf`, `image/png`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "download_url": {
                "display_name": "Download URL",
                "short_desc": "Pre-authenticated download URL",
                "desc": "A short-lived, pre-authenticated URL for downloading the file content directly",
                "type": AbstractDataProviderTypeMap."*uri",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveDownloadResultDataType = new OneDriveDownloadResultDataType();

#! OneDrive delete result data type
public class OneDriveDeleteResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Deletion status",
                "desc": "The status of the deletion operation (always `deleted`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "Item ID",
                "short_desc": "The deleted item ID",
                "desc": "The unique identifier of the deleted item",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveDeleteResultDataType = new OneDriveDeleteResultDataType();
}
