# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreDeleteStockAdjustmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for deleting (voiding) a stock adjustment
public class Cin7CoreDeleteStockAdjustmentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-stock-adjustment",
            "desc": "Deletes (voids) a stock adjustment in Cin7Core",
            "type": "Cin7CoreDeleteStockAdjustmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreDeleteStockAdjustmentRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType();
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
        return "delete-stock-adjustment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Deletes (voids) a stock adjustment in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string task_id = req.TaskID;
        @assert(task_id.val(), "TaskID is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting stock adjustment %y", task_id));
        doDelete("/StockAdjustment", {"TaskID": task_id});
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleted stock adjustment"));
        return {"success": True, "TaskID": task_id};
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

#! Request data type for deleting a stock adjustment
public class Cin7CoreDeleteStockAdjustmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "The stock adjustment task ID",
                "desc": "The unique identifier (GUID) of the stock adjustment to delete/void",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreDeleteStockAdjustmentRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
