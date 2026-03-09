# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceGetCustomerDataProvider.qc BigCommerce get customer action data provider

/*  BigCommerceGetCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Request type for getting a customer
public class BigCommerceGetCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "The customer to retrieve",
                "type": IntType,
                "desc": "The ID of the customer to retrieve",
                "required": True,
                "ref_data": "customers",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce get customer action data provider
public class BigCommerceGetCustomerDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Get a customer from BigCommerce by ID",
            "type": "BigCommerceGetCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "get";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get a customer from BigCommerce by ID";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceGetCustomerRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceCustomerDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doGetV3("customers", {"id:in": req.customer_id});
        if (response.data.typeCode() == NT_LIST && response.data.size() > 0) {
            return response.data[0];
        }
        throw "BIGCOMMERCE-API-ERROR", sprintf("Customer %d not found", req.customer_id);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
