# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoCreateUpdateObjectDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating or updating an object
public class CustomerIoCreateUpdateObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "object_type_id": {
                "type": StringType,
                "display_name": "Object Type ID",
                "short_desc": "The object type identifier",
                "desc": "The object type identifier",
                "required": True,
                "preselected": True,
            },
            "object_id": {
                "type": StringType,
                "display_name": "Object ID",
                "short_desc": "The object identifier",
                "desc": "The object identifier",
                "required": True,
                "preselected": True,
            },
            "attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Attributes",
                "short_desc": "Custom attributes to set",
                "desc": "Custom attributes to set on the object",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating or updating a custom object via the Track API
public class CustomerIoCreateUpdateObjectDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-update-object",
            "desc": "Create or update a custom object in Customer.io",
            "type": "CustomerIoCreateUpdateObjectDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoCreateUpdateObjectRequestDataType RequestType();

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
        return "create-update-object";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create or update a custom object in Customer.io";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash body = {
            "type": "object",
            "identifiers": {
                "object_type_id": req.object_type_id,
                "object_id": req.object_id,
            },
            "action": "identify",
        };
        if (exists req.attributes) {
            body.attributes = req.attributes;
        }
        auto response = doTrackV2Post("entity", body);
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
