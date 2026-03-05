# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreUpdateStockAdjustmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Data provider for updating a stock adjustment
public class Cin7CoreUpdateStockAdjustmentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-stock-adjustment",
            "desc": "Updates an existing stock adjustment in Cin7Core",
            "type": "Cin7CoreUpdateStockAdjustmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreUpdateStockAdjustmentRequestDataType RequestType();

        #! Response type
        static Cin7CoreStockAdjustmentDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update-stock-adjustment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Updates an existing stock adjustment in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.TaskID.val(), "TaskID is required");

        hash<auto> body = {
            "TaskID": req.TaskID,
        };

        # Optional fields
        if (exists req.Location) body.Location = req.Location;
        if (exists req.Lines) body.Lines = req.Lines;
        if (exists req.AdjustmentDate) body.AdjustmentDate = Cin7CoreDataProviderBase::formatDate(req.AdjustmentDate);
        if (exists req.Account) body.Account = req.Account;
        if (exists req.Memo) body.Memo = req.Memo;
        if (exists req.Status) body.Status = req.Status;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating stock adjustment %y", req.TaskID));
        hash<auto> response = doPut("/StockAdjustment", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated stock adjustment"));
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

#! Request data type for updating a stock adjustment
public class Cin7CoreUpdateStockAdjustmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The stock adjustment task ID",
                "desc": "The unique identifier (GUID) of the stock adjustment to update",
                "required": True,
            },
            "Location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for the adjustment",
                "ref_data": "locations",
            },
            "Lines": {
                "type": new ListDataType("Lines", new Cin7CoreStockAdjustmentLineDataType(), True),
                "display_name": "Lines",
                "short_desc": "Adjustment lines",
                "desc": "List of products and quantities to adjust",
            },
            "AdjustmentDate": {
                "type": DateOrNothingType,
                "display_name": "Adjustment Date",
                "short_desc": "Date of adjustment",
                "desc": "The date of the stock adjustment",
            },
            "Account": {
                "type": StringOrNothingType,
                "display_name": "Account",
                "short_desc": "Expense account",
                "desc": "The expense account for the adjustment",
            },
            "Memo": {
                "type": StringOrNothingType,
                "display_name": "Memo",
                "short_desc": "Memo/notes",
                "desc": "Notes about the adjustment",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Adjustment status",
                "desc": "The status of the adjustment (Draft, Authorized, Void)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreUpdateStockAdjustmentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
