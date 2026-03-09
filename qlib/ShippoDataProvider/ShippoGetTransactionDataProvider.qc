# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoGetTransactionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for getting a single transaction (shipping label)
public class ShippoGetTransactionDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-transaction",
            "desc": "Retrieves a single shipping label (transaction) by ID",
            "type": "ShippoGetTransactionDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoGetTransactionRequestDataType RequestType();

        #! Response type
        static ShippoTransactionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "get-transaction";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single shipping label (transaction) by ID";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string transaction_id = req.transaction_id;
        @assert(transaction_id.val(), "transaction_id is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: getting transaction %y", transaction_id));
        hash<auto> response = doGet("/transactions/" + transaction_id);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: retrieved transaction %y", response.object_id));
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

#! Request data type for getting a transaction
public class ShippoGetTransactionRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "transaction_id": {
                "type": StringType,
                "display_name": "Transaction ID",
                "short_desc": "The transaction object ID",
                "desc": "The unique identifier of the transaction (shipping label) to retrieve",
                "required": True,
                "ref_data": "transactions",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoGetTransactionRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
