# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreateCreditNoteLineDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Data provider for creating a credit note line
public class UnleashedCreateCreditNoteLineDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-credit-note-line",
            "desc": "Creates a new line item on a credit note in Unleashed",
            "type": "UnleashedCreateCreditNoteLineDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCreateCreditNoteLineRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("UnleashedCreditNoteLine");
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-credit-note-line";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new line item on a credit note in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        hash<auto> body = {};
        if (exists req.Product) body.Product = req.Product;
        if (exists req.Qty) body.Qty = req.Qty;
        if (exists req.UnitPrice) body.UnitPrice = req.UnitPrice;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating credit note line on %y", guid));
        hash<auto> response = doPost("/CreditNotes/" + guid + "/Lines", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created credit note line"));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for creating a credit note line
public class UnleashedCreateCreditNoteLineRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Credit Note GUID",
                "short_desc": "The credit note GUID",
                "desc": "The unique identifier (GUID) of the credit note",
                "required": True,
            },
            "Product": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Product",
                "short_desc": "Product information",
                "desc": "The product for this line (must include Guid)",
            },
            "Qty": {
                "type": SoftFloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Line quantity",
                "desc": "The quantity for this line",
            },
            "UnitPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "Unit price",
                "desc": "The unit price for this line",
            },
        };
    }

    constructor() : HashDataType("UnleashedCreateCreditNoteLineRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
