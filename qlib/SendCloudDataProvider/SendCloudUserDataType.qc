# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudUserDataType.qc SendCloud user account data type

/*  SendCloudUserDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! SendCloud user record (GET /user)
public class SendCloudUserDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "User ID",
                "desc": "Unique numeric identifier of the SendCloud account",
            },
            "username": {
                "type": SoftStringOrNothingType,
                "display_name": "Username",
                "short_desc": "SendCloud username",
                "desc": "Login username for the SendCloud account",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Account company name",
                "desc": "Company name on the SendCloud account",
            },
            "company_logo": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Logo",
                "short_desc": "URL of company logo",
                "desc": "URL to the company's logo image",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Account email",
                "desc": "Email address associated with the account",
            },
            "telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Telephone",
                "short_desc": "Account phone",
                "desc": "Phone number on the account",
            },
            "address": {
                "type": SoftStringOrNothingType,
                "display_name": "Address",
                "short_desc": "Street name",
                "desc": "Street of the account address",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "City of the account address",
            },
            "postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal / ZIP code",
                "desc": "Postal code of the account address",
            },
            "country": {
                "type": new SendCloudCountryObjectDataType(AutoHashOrNothingType),
                "display_name": "Country",
                "short_desc": "Country info",
                "desc": "Country object with `iso_2`, `iso_3`, and `name` fields",
            },
            "registered": {
                "type": SoftDateOrNothingType,
                "display_name": "Registered At",
                "short_desc": "Account registration timestamp",
                "desc": "Timestamp when the account was registered",
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "Additional account data",
                "desc": "Arbitrary additional account data returned by SendCloud",
            },
            "invoices": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Invoices",
                "short_desc": "Invoice references",
                "desc": "Invoice-reference entries from SendCloud",
            },
            "modules": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Modules",
                "short_desc": "Enabled SendCloud modules",
                "desc": "List of SendCloud feature modules enabled on the account",
            },
        };
    }

    constructor() : HashDataType("SendCloudUserDataType") {
        addQoreFields(Fields);
    }
}

#! Label URLs wrapper for get-parcel-label
public class SendCloudLabelUrlsDataType inherits HashDataType {
    public {
        const Fields = {
            "normal_printer": {
                "type": new ListDataType("SendCloudLabelNormalPrinterUrls", SoftStringType, True),
                "display_name": "Normal Printer",
                "short_desc": "A4 label URLs",
                "desc": "List of URLs for A4-size PDFs (one per `start_from` index in a multi-collo shipment)",
            },
            "label_printer": {
                "type": SoftStringOrNothingType,
                "display_name": "Label Printer",
                "short_desc": "A6 thermal label URL",
                "desc": "URL for the A6 thermal-printer PDF that contains all labels in the shipment",
            },
        };
    }

    constructor() : HashDataType("SendCloudLabelUrlsDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudLabelUrlsDataType") {
        addQoreFields(Fields);
    }
}

#! Customs declaration URL wrapper (deprecated by SendCloud, populated for international shipments)
public class SendCloudCustomsDeclarationUrlsDataType inherits HashDataType {
    public {
        const Fields = {
            "normal_printer": {
                "type": SoftStringOrNothingType,
                "display_name": "Normal Printer",
                "short_desc": "Customs declaration PDF URL",
                "desc": "URL of the A4 PDF customs declaration document.  Note: the SendCloud "
                    "label endpoint marks the `customs_declaration` field as deprecated; prefer "
                    "the dedicated customs-document download via `get-parcel-documents`.",
            },
        };
    }

    constructor() : HashDataType("SendCloudCustomsDeclarationUrlsDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudCustomsDeclarationUrlsDataType") {
        addQoreFields(Fields);
    }
}

#! Response type for the get-parcel-label action
public class SendCloudGetParcelLabelResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "label": {
                "type": new SendCloudLabelUrlsDataType(),
                "display_name": "Label",
                "short_desc": "Label URLs",
                "desc": "Hash with `normal_printer` (list of A4 PDF URLs) and `label_printer` (single A6 PDF URL)",
            },
            "customs_declaration": {
                "type": new SendCloudCustomsDeclarationUrlsDataType(AutoHashOrNothingType),
                "display_name": "Customs Declaration",
                "short_desc": "Customs declaration document URLs",
                "desc": "Customs declaration document URL container (only present for international "
                    "shipments).  Marked deprecated by SendCloud; prefer `get-parcel-documents`.",
            },
        };
    }

    constructor() : HashDataType("SendCloudGetParcelLabelResponseDataType") {
        addQoreFields(Fields);
    }
}

#! Response type for cancel-parcel / cancel-return
public class SendCloudCancelResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "type": SoftStringType,
                "display_name": "Status",
                "short_desc": "Cancellation status",
                "desc": "Status returned by SendCloud (e.g., 'cancelled', 'queued', 'failed')",
            },
            "message": {
                "type": SoftStringOrNothingType,
                "display_name": "Message",
                "short_desc": "Human-readable message",
                "desc": "Optional message returned by SendCloud alongside the status",
            },
        };
    }

    constructor() : HashDataType("SendCloudCancelResponseDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
