# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreAddSalePackDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for adding a pack/pick record to a sale
public class Cin7CoreAddSalePackDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-sale-pack",
            "desc": "Adds a pack/pick record to a sale in Cin7Core",
            "type": "Cin7CoreAddSalePackDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreAddSalePackRequestDataType RequestType();

        #! Response type
        static Cin7CoreSalePackDataType ResponseType();
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
        return "add-sale-pack";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Adds a pack/pick record to a sale in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.SaleID.val(), "SaleID is required");

        hash<auto> body = {
            "SaleID": req.SaleID,
        };

        # Optional fields
        if (exists req.Lines) body.Lines = req.Lines;
        if (exists req.Memo) body.Memo = req.Memo;
        if (exists req.PickDate) body.PickDate = Cin7CoreDataProviderBase::formatDate(req.PickDate);
        if (exists req.Location) body.Location = req.Location;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding pack to sale %y", req.SaleID));
        hash<auto> response = doPost("/SalePack", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: added pack %y", response.TaskID ?? "n/a"));
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

#! Request data type for adding a sale pack
public class Cin7CoreAddSalePackRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "SaleID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale",
                "required": True,
                "ref_data": "sales",
            },
            "Lines": {
                "type": AutoListOrNothingType,
                "display_name": "Lines",
                "short_desc": "Pack lines",
                "desc": "List of lines to pack with quantities",
            },
            "Memo": {
                "type": StringOrNothingType,
                "display_name": "Memo",
                "short_desc": "Pack memo",
                "desc": "Notes or memo for the pack operation",
            },
            "PickDate": {
                "type": DateOrNothingType,
                "display_name": "Pick Date",
                "short_desc": "Pick date",
                "desc": "The date the items were picked",
            },
            "Location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Location",
                "desc": "The warehouse location for the pack",
                "ref_data": "locations",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAddSalePackRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Response data type for sale pack
public class Cin7CoreSalePackDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Fulfilment task ID",
                "desc": "The unique identifier for the fulfilment task",
            },
            "SaleID": {
                "type": StringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Associated sale ID",
                "desc": "The sale this pack belongs to",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Pack status",
                "desc": "The status of the pack operation",
            },
            "Lines": {
                "type": AutoListOrNothingType,
                "display_name": "Lines",
                "short_desc": "Pack lines",
                "desc": "List of packed lines",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSalePackDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
