# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipAddReturnShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Request type for adding a shipment to a return
public class AftershipAddReturnShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Return ID",
                "short_desc": "The return ID to add shipment to",
                "desc": "The Aftership return ID to attach the shipping label/tracking to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "tracking_number": {
                "display_name": "Tracking Number",
                "short_desc": "Return shipment tracking number",
                "desc": "Tracking number for the return shipment",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "courier_code": {
                "display_name": "Courier Code",
                "short_desc": "Return shipment courier",
                "desc": "Courier code for the return shipment (e.g., `fedex`, `ups`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AftershipAddReturnShipmentRequestDataType = new AftershipAddReturnShipmentRequestDataType();

#! Data provider for adding a shipment to a return
public class AftershipAddReturnShipmentDataProvider inherits AftershipDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-return-shipment",
            "type": "AftershipAddReturnShipmentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = AftershipAddReturnShipmentRequestDataType;
        const ResponseType = AftershipReturnDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AftershipDataProviderBase(rest) {}
    constructor(*hash<auto> options) : AftershipDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.id;
        return doReturnsCommand("POST", "returns/" + id + "/shipments", req).body.data;
    }
}
}
