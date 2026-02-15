# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCreateCreditNoteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a credit note
public class UnleashedCreateCreditNoteDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-credit-note",
            "desc": "Creates a new free credit note in Unleashed",
            "type": "UnleashedCreateCreditNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCreateCreditNoteRequestDataType RequestType();

        #! Response type
        static UnleashedCreditNoteDataType ResponseType();
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
        return "create-credit-note";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new free credit note in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};

        if (exists req.Customer) body.Customer = req.Customer;
        if (exists req.CreditNoteNumber) body.CreditNoteNumber = req.CreditNoteNumber;
        if (exists req.CreditNoteDate) body.CreditNoteDate = formatDate(req.CreditNoteDate);
        if (exists req.CreditNoteLines) body.CreditNoteLines = req.CreditNoteLines;
        if (exists req.Comments) body.Comments = req.Comments;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating credit note"));
        hash<auto> response = doPost("/CreditNotes/FreeCredit", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created credit note %y", response.CreditNoteNumber));
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

#! Request data type for creating a credit note
public class UnleashedCreateCreditNoteRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "Customer": {
                "type": AutoHashOrNothingType,
                "display_name": "Customer",
                "short_desc": "Customer information",
                "desc": "The customer for this credit note (must include Guid)",
            },
            "CreditNoteNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Credit Note Number",
                "short_desc": "Credit note number",
                "desc": "The credit note number",
            },
            "CreditNoteDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Credit Note Date",
                "short_desc": "Credit note date",
                "desc": "The credit note date",
            },
            "CreditNoteLines": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Credit Note Lines",
                "short_desc": "Credit note line items",
                "desc": "List of line items for this credit note",
            },
            "Comments": {
                "type": SoftStringOrNothingType,
                "display_name": "Comments",
                "short_desc": "Credit note comments",
                "desc": "Comments or notes for this credit note",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateCreditNoteRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
