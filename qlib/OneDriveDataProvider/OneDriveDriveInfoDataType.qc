# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDriveInfoDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OneDrive Drive info data type
public class OneDriveDriveInfoDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Drive ID",
                "short_desc": "The unique identifier of the drive",
                "desc": "The unique identifier of the drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "driveType": {
                "display_name": "Drive Type",
                "short_desc": "The type of drive",
                "desc": "Describes the type of drive represented by this resource",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "personal", "display_name": "Personal"},
                    <AllowedValueInfo>{"value": "business", "display_name": "Business"},
                    <AllowedValueInfo>{"value": "documentLibrary", "display_name": "Document Library"},
                ),
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the drive",
                "desc": "The name of the drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "owner": {
                "display_name": "Owner",
                "short_desc": "The owner of the drive",
                "desc": "Information about the owner of the drive",
                "type": new OneDriveIdentitySetDataType(AutoHashOrNothingType),
            },
            "quota": {
                "display_name": "Quota",
                "short_desc": "Storage quota information",
                "desc": "Information about the drive's storage quota",
                "type": new OneDriveQuotaDataType(AutoHashOrNothingType),
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the drive in a browser",
                "desc": "URL that displays the drive in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time the drive was created",
                "desc": "Date and time the drive was created",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the drive was last modified",
                "desc": "Date and time the drive was last modified",
                "type": DateOrNothingType,
            },
        };
    }
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) { addQoreFields(Fields); }
}

public const OneDriveDriveInfoDataType = new OneDriveDriveInfoDataType();
}
