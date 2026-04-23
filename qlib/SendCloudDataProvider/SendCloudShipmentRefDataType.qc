# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudShipmentRefDataType.qc shipping-method reference type (as returned on a parcel)

/*  SendCloudShipmentRefDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Shipping-method reference as used on parcel requests and returned on parcel responses (id + name)
public class SendCloudShipmentRefDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "Shipping Method ID",
                "short_desc": "Numeric ID of the shipping method",
                "desc": "Numeric ID of the shipping method used for the parcel",
                "ref_data": "shipping_methods",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Method Name",
                "short_desc": "Human-readable name",
                "desc": "Display name of the shipping method (e.g., 'Unstamped letter')",
            },
        };
    }

    constructor() : HashDataType("SendCloudShipmentRefDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudShipmentRefDataType") {
        addQoreFields(Fields);
    }
}

#! Parcel status as returned by SendCloud
public class SendCloudParcelStatusDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "Status ID",
                "short_desc": "Numeric status ID",
                "desc": "Numeric identifier of the parcel status",
            },
            "message": {
                "type": SoftStringOrNothingType,
                "display_name": "Status Message",
                "short_desc": "Human-readable status",
                "desc": "Current status message (e.g., 'Ready to send', 'Delivered')",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelStatusDataType") {
        addQoreFields(Fields);
    }
}

#! Parcel document as returned by SendCloud (available label/customs documents)
public class SendCloudParcelDocumentDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": SoftStringOrNothingType,
                "display_name": "Document Type",
                "short_desc": "Document category",
                "desc": "Type of document (e.g., label, cn22, cn23, commercial-invoice)",
            },
            "link": {
                "type": SoftStringOrNothingType,
                "display_name": "Link",
                "short_desc": "Download URL",
                "desc": "URL to download the document PDF",
            },
            "size": {
                "type": SoftStringOrNothingType,
                "display_name": "Size",
                "short_desc": "Printer format",
                "desc": "Document printer/format (e.g., a4, a6, zpl)",
            },
        };
    }

    constructor() : HashDataType("SendCloudParcelDocumentDataType") {
        addQoreFields(Fields);
    }
}

#! Country reference object as returned by SendCloud (on parcels, users, sender addresses, etc.)
public class SendCloudCountryObjectDataType inherits HashDataType {
    public {
        const Fields = {
            "iso_2": {
                "type": SoftStringOrNothingType,
                "display_name": "ISO 2",
                "short_desc": "ISO 3166-1 alpha-2 code",
                "desc": "Two-letter ISO 3166-1 alpha-2 country code (e.g., `NL`, `US`)",
            },
            "iso_3": {
                "type": SoftStringOrNothingType,
                "display_name": "ISO 3",
                "short_desc": "ISO 3166-1 alpha-3 code",
                "desc": "Three-letter ISO 3166-1 alpha-3 country code (e.g., `NLD`, `USA`)",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Country display name",
                "desc": "Full English name of the country (e.g., `Netherlands`)",
            },
        };
    }

    constructor() : HashDataType("SendCloudCountryObjectDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudCountryObjectDataType") {
        addQoreFields(Fields);
    }
}

#! Carrier reference object as returned by SendCloud on parcel records
public class SendCloudCarrierObjectDataType inherits HashDataType {
    public {
        const Fields = {
            "code": {
                "type": SoftStringOrNothingType,
                "display_name": "Code",
                "short_desc": "Carrier code",
                "desc": "Short carrier code (e.g., `postnl`, `dhl`, `ups`)",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Carrier display name",
                "desc": "Human-readable carrier name (e.g., `PostNL`)",
            },
        };
    }

    constructor() : HashDataType("SendCloudCarrierObjectDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudCarrierObjectDataType") {
        addQoreFields(Fields);
    }
}

#! Refund-type sub-object as returned inside a return's refund response
public class SendCloudReturnRefundTypeResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "code": {
                "type": SoftStringOrNothingType,
                "display_name": "Code",
                "short_desc": "Refund type code",
                "desc": "Short refund-type code (e.g., `money`, `store_credit`, `exchange`)",
            },
            "label": {
                "type": SoftStringOrNothingType,
                "display_name": "Label",
                "short_desc": "Refund type display label",
                "desc": "Human-readable refund-type label",
            },
            "require_message": {
                "type": SoftBoolOrNothingType,
                "display_name": "Require Message",
                "short_desc": "Whether a message is required",
                "desc": "`True` if this refund type requires a customer message",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnRefundTypeResponseDataType") {
        addQoreFields(Fields);
    }
}

#! Refund details as returned on a SendCloud return record (response-side shape)
public class SendCloudReturnRefundResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "total_refund": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Refund",
                "short_desc": "Total refund amount",
                "desc": "Total refund amount (string-encoded decimal)",
            },
            "refunded_at": {
                "type": SoftDateOrNothingType,
                "display_name": "Refunded At",
                "short_desc": "Timestamp of the refund",
                "desc": "Timestamp when the refund was processed",
            },
            "message": {
                "type": SoftStringOrNothingType,
                "display_name": "Message",
                "short_desc": "Refund message",
                "desc": "Human-readable message associated with the refund",
            },
            "refund_type": {
                "type": new SendCloudReturnRefundTypeResponseDataType(),
                "display_name": "Refund Type",
                "short_desc": "Refund-type sub-object",
                "desc": "Sub-object describing the refund-type code, label, and whether a message is required",
            },
        };
    }

    constructor() : HashDataType("SendCloudReturnRefundResponseDataType") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "SendCloudReturnRefundResponseDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
