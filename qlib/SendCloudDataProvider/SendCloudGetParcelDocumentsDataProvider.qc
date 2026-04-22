# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudGetParcelDocumentsDataProvider.qc SendCloud get-parcel-documents action data provider

/*  SendCloudGetParcelDocumentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Get-parcel-documents request type
public class SendCloudGetParcelDocumentsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Parcel ID",
                "short_desc": "Parcel whose document to retrieve",
                "desc": "Numeric ID of the parcel",
                "required": True,
                "ref_data": "parcels",
            },
            "type": {
                "type": SoftStringType,
                "display_name": "Document Type",
                "short_desc": "Type of document to download",
                "desc": "Type of document to download for the parcel",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "label", "display_name": "Label",
                        "desc": "Shipping label PDF"},
                    <AllowedValueInfo>{"value": "cn22", "display_name": "CN22",
                        "desc": "CN22 customs declaration form"},
                    <AllowedValueInfo>{"value": "cn23", "display_name": "CN23",
                        "desc": "CN23 customs declaration form"},
                    <AllowedValueInfo>{"value": "commercial-invoice", "display_name": "Commercial Invoice",
                        "desc": "Commercial invoice PDF"},
                ),
            },
            "format": {
                "type": SoftStringType,
                "display_name": "Format",
                "short_desc": "Output format",
                "desc": "Output format of the document",
                "required": True,
                "default_value": "pdf",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pdf", "display_name": "PDF", "desc": "PDF format"},
                    <AllowedValueInfo>{"value": "zpl", "display_name": "ZPL",
                        "desc": "ZPL (Zebra printer) format"},
                ),
            },
        };
    }

    constructor() : HashDataType("SendCloudGetParcelDocumentsRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Get-parcel-documents action provider (GET /parcels/{id}/documents/{type}?format={format})
public class SendCloudGetParcelDocumentsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-parcel-documents",
            "desc": "Download a parcel document (label, CN22, CN23, or commercial invoice) as base64-encoded bytes",
            "type": "SendCloudGetParcelDocumentsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudGetParcelDocumentsRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudParcelDocumentDownloadDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "get-parcel-documents";
    }

    *string getDesc() {
        return "Download a parcel document as base64-encoded bytes";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = req.id;
        string type = req.type;
        string fmt = req.format ?? "pdf";
        string path = sprintf("parcels/%d/documents/%s", id, type);
        hash<auto> got = doGetBytes(path, {"format": fmt}, "application/pdf");
        string ext = fmt == "zpl" ? "zpl" : "pdf";
        string filename = sprintf("parcel-%d-%s.%s", id, type, ext);
        string content_type = got.content_type ?? (fmt == "zpl" ? "application/zpl" : "application/pdf");
        string encoded;
        auto body = got.body;
        if (body.typeCode() == NT_BINARY) {
            encoded = make_base64_string(body);
        } else if (body.typeCode() == NT_STRING) {
            encoded = make_base64_string(binary(body));
        } else {
            encoded = "";
        }
        return {
            "filename": filename,
            "content_type": content_type,
            "data": encoded,
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
