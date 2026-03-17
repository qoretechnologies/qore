# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoAddDeviceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for adding a device
public class CustomerIoAddDeviceRequestDataType inherits HashDataType {
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
            "device_id": {
                "type": StringType,
                "display_name": "Device ID",
                "short_desc": "The device token",
                "desc": "The device token",
                "required": True,
                "preselected": True,
            },
            "platform": {
                "type": StringType,
                "display_name": "Platform",
                "short_desc": "Device platform",
                "desc": "Device platform (ios or android)",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ios", "desc": "iOS device"},
                    <AllowedValueInfo>{"value": "android", "desc": "Android device"},
                ),
            },
            "last_used": {
                "type": IntOrNothingType,
                "display_name": "Last Used",
                "short_desc": "Unix timestamp of last device use",
                "desc": "Unix timestamp of last device use",
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "Device attributes",
                "desc": "Device attributes",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding a device to a customer profile via the Track API
public class CustomerIoAddDeviceDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-device",
            "desc": "Add a mobile device to a customer profile for push notifications",
            "type": "CustomerIoAddDeviceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoAddDeviceRequestDataType RequestType();

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
        return "add-device";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add a mobile device to a customer profile for push notifications";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> device = {
            "id": req.device_id,
            "platform": req.platform,
        };
        if (exists req.last_used) {
            device.last_used = req.last_used;
        }
        if (exists req.attributes) {
            device += req.attributes;
        }
        hash<auto> body = {
            "device": device,
        };
        auto response = doTrackPut(sprintf("/customers/%s/devices", req.customer_id), body);
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
