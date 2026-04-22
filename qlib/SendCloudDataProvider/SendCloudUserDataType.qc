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
                "type": AutoHashOrNothingType,
                "display_name": "Country",
                "short_desc": "Country info",
                "desc": "Country object (iso_2, iso_3, name)",
            },
            "registered": {
                "type": SoftStringOrNothingType,
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
                "type": SoftAutoListOrNothingType,
                "display_name": "Normal Printer",
                "short_desc": "A4 label URLs",
                "desc": "List of URLs for A4-size PDFs (one per collo in a multi-collo shipment)",
            },
            "label_printer": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Label Printer",
                "short_desc": "A6 thermal label URLs",
                "desc": "List of URLs for A6 thermal-printer PDFs (one per collo in a multi-collo shipment)",
            },
        };
    }

    constructor() : HashDataType("SendCloudLabelUrlsDataType") {
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
                "desc": "Hash with normal_printer and label_printer arrays of PDF URLs",
            },
        };
    }

    constructor() : HashDataType("SendCloudGetParcelLabelResponseDataType") {
        addQoreFields(Fields);
    }
}

#! Response type for the get-parcel-documents action
public class SendCloudParcelDocumentDownloadDataType inherits HashDataType {
    public {
        const Fields = {
            "filename": {
                "type": SoftStringType,
                "display_name": "File Name",
                "short_desc": "Suggested filename",
                "desc": "Suggested filename for the downloaded document",
            },
            "content_type": {
                "type": SoftStringType,
                "display_name": "Content Type",
                "short_desc": "MIME content type",
                "desc": "MIME content type of the downloaded document (e.g., application/pdf)",
            },
            "data": {
                "type": SoftStringType,
                "display_name": "Data (Base64)",
                "short_desc": "Base64-encoded document bytes",
                "desc": "The document contents encoded as a base64 string",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelDocumentDownloadDataType") {
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
