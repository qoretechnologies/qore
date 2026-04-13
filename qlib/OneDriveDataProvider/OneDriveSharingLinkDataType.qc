# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveSharingLinkDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! OneDrive sharing link data type
public class OneDriveSharingLinkDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Permission ID",
                "short_desc": "The unique identifier of the permission",
                "desc": "The unique identifier of the permission",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "link": {
                "display_name": "Link Details",
                "short_desc": "The sharing link details",
                "desc": "The sharing link details including URL, type, and scope",
                "type": new OneDriveSharingLinkDetailDataType(),
            },
            "roles": {
                "display_name": "Roles",
                "short_desc": "The permission roles",
                "desc": "The type of permission: `read`, `write`, `owner`",
                "type": new ListDataType("*list<string>", StringType, True),
            },
            "shareId": {
                "display_name": "Share ID",
                "short_desc": "The unique sharing token",
                "desc": "A unique token that can be used to access this shared item via the shares API",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expirationDateTime": {
                "display_name": "Expiration Date",
                "short_desc": "When the link expires",
                "desc": "Date and time when the sharing link expires",
                "type": DateOrNothingType,
            },
            "hasPassword": {
                "display_name": "Has Password",
                "short_desc": "Whether the link is password protected",
                "desc": "Indicates whether the link has a password set",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) { addQoreFields(Fields); }
}

public const OneDriveSharingLinkDataType = new OneDriveSharingLinkDataType();
}
