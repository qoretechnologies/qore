# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceMarkEstimateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice mark estimate data provider - marks an estimate as sent, accepted, or declined
public class ZohoInvoiceMarkEstimateDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-estimate",
            "desc": "Marks an estimate status as sent, accepted, or declined",
            "type": "ZohoInvoiceMarkEstimateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "estimate_id": <DataProviderOptionInfo>{
                "display_name": "Estimate ID",
                "short_desc": "The estimate to mark",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the estimate to mark",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("MarkEstimateRequest", {
            "estimate_id": {
                "display_name": "Estimate ID",
                "type": SoftStringType,
                "desc": "The ID of the estimate to mark",
            },
            "status": {
                "display_name": "Status",
                "type": SoftStringType,
                "desc": "The status to set",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "sent", "desc": "Mark as sent"},
                    <AllowedValueInfo>{"value": "accepted", "desc": "Mark as accepted"},
                    <AllowedValueInfo>{"value": "declined", "desc": "Mark as declined"},
                ),
            },
        });

        #! Response type
        const ResponseType = new HashDataType("MarkEstimateResponse", {
            "code": {
                "display_name": "Code",
                "type": SoftIntType,
                "desc": "Response code (0 for success)",
            },
            "message": {
                "display_name": "Message",
                "type": SoftStringType,
                "desc": "Response message",
            },
        });
    }

    private {
        #! The estimate ID
        string estimate_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options) : ZohoInvoiceDataProviderBase(rest) {
        if (options.estimate_id) {
            estimate_id = options.estimate_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "mark-estimate";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Marks an estimate status as sent, accepted, or declined";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.estimate_id ?? request_options.estimate_id ?? estimate_id;
        if (!id) {
            throw "ZOHOINVOICE-ERROR", "estimate_id is required for mark operation";
        }
        string status = req.status;
        if (!status) {
            throw "ZOHOINVOICE-ERROR", "status is required for mark operation (sent, accepted, declined)";
        }

        # Validate status
        if (status !~ /^(sent|accepted|declined)$/i) {
            throw "ZOHOINVOICE-ERROR", sprintf("Invalid status '%s'; must be one of: sent, accepted, declined", status);
        }

        return doPost(sprintf("/estimates/%s/status/%s", id, status.lwr()));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
