# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationAddFundsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Request type for adding funds to a carrier
public class ShipStationAddFundsRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "carrierCode": {
                "type": StringType,
                "display_name": "Carrier Code",
                "short_desc": "The carrier code",
                "desc": "The carrier code to add funds to (typically 'stamps_com')",
                "required": True,
            },
            "amount": {
                "type": SoftFloatType,
                "display_name": "Amount",
                "short_desc": "Amount to add",
                "desc": "The amount to add to the carrier account (in USD)",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for adding funds
public class ShipStationAddFundsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier Name",
                "short_desc": "The carrier name",
                "desc": "The name of the carrier",
            },
            "code": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier Code",
                "short_desc": "The carrier code",
                "desc": "The carrier code",
            },
            "balance": {
                "type": SoftFloatOrNothingType,
                "display_name": "Balance",
                "short_desc": "New account balance",
                "desc": "The new balance after adding funds",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding funds to a carrier account
public class ShipStationAddFundsDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-funds",
            "desc": "Add funds to a carrier account (e.g., Stamps.com)",
            "type": "ShipStationAddFundsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationAddFundsRequestDataType RequestType();

        #! Response type
        static ShipStationAddFundsResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "add-funds";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add funds to a carrier account (e.g., Stamps.com)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding %f funds to carrier %s",
            req.amount, req.carrierCode));

        hash<auto> response = doPost("/carriers/addfunds", req);
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

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
