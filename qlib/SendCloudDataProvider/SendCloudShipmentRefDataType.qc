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

#! Shipping-method reference as returned on a parcel record (id + name)
public class SendCloudShipmentRefDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "Shipping Method ID",
                "short_desc": "Numeric ID of the shipping method",
                "desc": "Numeric ID of the shipping method used for the parcel",
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

} # namespace SendCloudDataProvider
