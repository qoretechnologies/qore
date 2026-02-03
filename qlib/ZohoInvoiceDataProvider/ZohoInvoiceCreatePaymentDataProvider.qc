# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceCreatePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice create payment data provider
public class ZohoInvoiceCreatePaymentDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-payment",
            "desc": "Creates a new customer payment in Zoho Invoice",
            "type": "ZohoInvoiceCreatePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-payment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Creates a new customer payment in Zoho Invoice";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ZohoInvoiceCreatePaymentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInvoicePaymentDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPost("/customerpayments", req);
        return response.payment;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
