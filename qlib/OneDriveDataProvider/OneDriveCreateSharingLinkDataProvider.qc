# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveCreateSharingLinkDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for creating a sharing link
public class OneDriveCreateSharingLinkRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The item ID to share",
                "desc": "The unique identifier of the item to create a sharing link for",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "01BYE5RZ6QN3ZWBTUFOFD3GSPGOHDJD36K",
                "ref_data": "items",
            },
            "type": {
                "display_name": "Link Type",
                "short_desc": "The type of sharing link",
                "desc": "The type of sharing link to create:\n"
                    "- `view`: read-only link\n"
                    "- `edit`: read-write link\n"
                    "- `embed`: embeddable link",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "view",
                        "display_name": "View",
                        "desc": "Read-only access",
                    },
                    <AllowedValueInfo>{
                        "value": "edit",
                        "display_name": "Edit",
                        "desc": "Read-write access",
                    },
                    <AllowedValueInfo>{
                        "value": "embed",
                        "display_name": "Embed",
                        "desc": "Embeddable link",
                    },
                ),
            },
            "scope": {
                "display_name": "Link Scope",
                "short_desc": "The scope of the sharing link",
                "desc": "The scope of the link:\n"
                    "- `anonymous`: anyone with the link can access\n"
                    "- `organization`: only users in the same organization can access",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "anonymous",
                        "display_name": "Anonymous",
                        "desc": "Anyone with the link can access",
                    },
                    <AllowedValueInfo>{
                        "value": "organization",
                        "display_name": "Organization",
                        "desc": "Only users in the same organization",
                    },
                ),
            },
            "password": {
                "display_name": "Password",
                "short_desc": "Password for the sharing link",
                "desc": "The password for the sharing link (optional)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expirationDateTime": {
                "display_name": "Expiration Date",
                "short_desc": "When the link expires",
                "desc": "The date and time when the sharing link expires",
                "type": AbstractDataProviderTypeMap."*date",
                "example_value": 2026-12-31T23:59:59Z,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveCreateSharingLinkRequestDataType = new OneDriveCreateSharingLinkRequestDataType();

#! Data provider for creating a sharing link
public class OneDriveCreateSharingLinkDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-sharing-link",
            "type": "OneDriveCreateSharingLinkDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveCreateSharingLinkRequestDataType;
        const ResponseType = OneDriveSharingLinkDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "type": req.type,
        };
        if (req.scope) {
            body.scope = req.scope;
        }
        if (req.password) {
            body.password = req.password;
        }
        if (req.expirationDateTime) {
            body.expirationDateTime = req.expirationDateTime.format("YYYY-MM-DDTHH:mm:SSZ");
        }

        return doRestCommand("POST", getDriveBasePath() + "/items/" + req.id + "/createLink", body).body;
    }
}
}
