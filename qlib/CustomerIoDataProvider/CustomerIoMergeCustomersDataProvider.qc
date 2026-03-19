# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoMergeCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for merging customers
public class CustomerIoMergeCustomersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "primary_type": {
                "type": StringType,
                "display_name": "Primary Type",
                "short_desc": "Primary identifier type",
                "desc": "Primary identifier type (id, email, or cio_id)",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "id", "desc": "Customer ID"},
                    <AllowedValueInfo>{"value": "email", "desc": "Email address"},
                    <AllowedValueInfo>{"value": "cio_id", "desc": "Customer.io ID"},
                ),
            },
            "primary_value": {
                "type": StringType,
                "display_name": "Primary Value",
                "short_desc": "Primary identifier value",
                "desc": "Primary identifier value",
                "required": True,
                "preselected": True,
            },
            "secondary_type": {
                "type": StringType,
                "display_name": "Secondary Type",
                "short_desc": "Secondary identifier type",
                "desc": "Secondary identifier type (id, email, or cio_id)",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "id", "desc": "Customer ID"},
                    <AllowedValueInfo>{"value": "email", "desc": "Email address"},
                    <AllowedValueInfo>{"value": "cio_id", "desc": "Customer.io ID"},
                ),
            },
            "secondary_value": {
                "type": StringType,
                "display_name": "Secondary Value",
                "short_desc": "Secondary identifier value",
                "desc": "Secondary identifier value",
                "required": True,
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for merging two customer profiles via the Track API
public class CustomerIoMergeCustomersDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "merge-customers",
            "desc": "Merge two customer profiles in Customer.io",
            "type": "CustomerIoMergeCustomersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoMergeCustomersRequestDataType RequestType();

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
        return "merge-customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Merge two customer profiles in Customer.io";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "primary": {
                req.primary_type: req.primary_value,
            },
            "secondary": {
                req.secondary_type: req.secondary_value,
            },
        };
        auto response = doTrackPost("/merge_customers", body);
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
