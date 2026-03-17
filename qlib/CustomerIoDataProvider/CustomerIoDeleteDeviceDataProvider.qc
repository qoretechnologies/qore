# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoDeleteDeviceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Request type for deleting a device
public class CustomerIoDeleteDeviceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The unique customer identifier",
                "desc": "The unique customer identifier",
                "required": True,
                "preselected": True,
            },
            "device_token": {
                "type": StringType,
                "display_name": "Device Token",
                "short_desc": "The device token to remove",
                "desc": "The device token to remove",
                "required": True,
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a device from a customer profile via the Track API
public class CustomerIoDeleteDeviceDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-device",
            "desc": "Delete a mobile device from a customer profile",
            "type": "CustomerIoDeleteDeviceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoDeleteDeviceRequestDataType RequestType();

        #! Response type
        #! Response type: auto hash (Track API returns empty body on success)
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-device";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a mobile device from a customer profile";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        auto response = doTrackDelete(sprintf("/customers/%s/devices/%s", req.customer_id, req.device_token));
        return response ?? {"success": True};
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
