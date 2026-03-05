# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCompleteCreditNoteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for completing a credit note
public class UnleashedCompleteCreditNoteDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "complete-credit-note",
            "desc": "Completes a credit note in Unleashed",
            "type": "UnleashedCompleteCreditNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static UnleashedCompleteCreditNoteRequestDataType RequestType();

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
        return "complete-credit-note";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Completes a credit note in Unleashed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string guid = req.guid;
        @assert(guid.val(), "guid is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: completing credit note %y", guid));
        hash<auto> response = doPost("/CreditNotes/" + guid + "/Complete");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: completed credit note %y", response.Guid));
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

#! Request data type for completing a credit note
public class UnleashedCompleteCreditNoteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Credit Note GUID",
                "short_desc": "The credit note GUID",
                "desc": "The unique identifier (GUID) of the credit note to complete",
                "required": True,
                "ref_data": "creditnotes",
            },
        };
    }

    constructor() : HashDataType("UnleashedCompleteCreditNoteRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
