# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationListServicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for listing carrier services
public class ShipStationListServicesRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "carrierCode": {
                "type": StringType,
                "display_name": "Carrier Code",
                "short_desc": "The carrier code",
                "desc": "The carrier code to list services for (e.g., 'ups', 'fedex', 'stamps_com')",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for listing services
public class ShipStationListServicesResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "services": {
                "type": SoftAutoListType,
                "display_name": "Services",
                "short_desc": "List of available services",
                "desc": "Array of shipping services available for the carrier",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for listing carrier services
public class ShipStationListServicesDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-services",
            "desc": "List available shipping services for a carrier",
            "type": "ShipStationListServicesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationListServicesRequestDataType RequestType();

        #! Response type
        static ShipStationListServicesResponseDataType ResponseType();
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
        return "list-services";
    }

    #! Returns the provider description
    *string getDesc() {
        return "List available shipping services for a carrier";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string carrierCode = req.carrierCode;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: listing services for carrier %s", carrierCode));

        hash<auto> response = doGet("/carriers/listservices", {"carrierCode": carrierCode});
        # Response is a list, wrap it
        return {"services": response.typeCode() == NT_LIST ? response : ()};
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
