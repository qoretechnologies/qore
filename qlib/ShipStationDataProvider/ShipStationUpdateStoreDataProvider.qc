# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationUpdateStoreDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a store
public class ShipStationUpdateStoreRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "storeId": {
                "type": IntType,
                "display_name": "Store ID",
                "short_desc": "The store ID to update",
                "desc": "The unique ShipStation store ID to update",
                "required": True,
            },
            "storeName": {
                "type": SoftStringOrNothingType,
                "display_name": "Store Name",
                "short_desc": "Display name for the store",
                "desc": "The display name for the store",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Contact email",
                "desc": "The contact email address for the store",
            },
            "companyName": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name for the store",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Contact phone number",
                "desc": "The contact phone number for the store",
            },
            "integrationUrl": {
                "type": SoftStringOrNothingType,
                "display_name": "Integration URL",
                "short_desc": "Integration URL",
                "desc": "The integration URL for manual stores",
            },
            "statusMappings": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Status Mappings",
                "short_desc": "Order status mappings",
                "desc": "Array of status mappings for syncing order statuses",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a store
public class ShipStationUpdateStoreDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-store",
            "desc": "Update a ShipStation store (selling channel)",
            "type": "ShipStationUpdateStoreDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationUpdateStoreRequestDataType RequestType();

        #! Response type
        static ShipStationStoreDataType ResponseType();
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
        return "update-store";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a ShipStation store (selling channel)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int storeId = req.storeId;
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating store %d", storeId));

        hash<auto> body = req - "storeId";
        hash<auto> response = doPut(sprintf("/stores/%d", storeId), body);
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
