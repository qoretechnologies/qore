# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveUploadSessionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OneDrive upload session data type
public class OneDriveUploadSessionDataType inherits HashDataType {
    public {
        const Fields = {
            "uploadUrl": {
                "display_name": "Upload URL",
                "short_desc": "URL to upload file chunks to",
                "desc": "The URL endpoint that accepts PUT requests for byte range uploads. This URL is "
                    "pre-authenticated and does not require an authorization header",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expirationDateTime": {
                "display_name": "Expiration Date",
                "short_desc": "When the upload session expires",
                "desc": "The date and time in UTC that the upload session will expire. The complete file "
                    "must be uploaded before this time",
                "type": DateOrNothingType,
            },
            "nextExpectedRanges": {
                "display_name": "Next Expected Ranges",
                "short_desc": "Byte ranges the server is missing",
                "desc": "A collection of byte ranges that the server is missing for the file. These ranges "
                    "are zero-indexed and in the format `start-end`",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveUploadSessionDataType = new OneDriveUploadSessionDataType();
}
