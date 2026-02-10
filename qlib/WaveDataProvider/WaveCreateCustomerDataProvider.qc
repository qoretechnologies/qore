# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveCreateCustomerDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Data provider for creating Wave customers via request API
public class WaveCreateCustomerDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customer",
            "desc": "Create a new customer in Wave",
            "type": "WaveCreateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the customer data to create
        static WaveCustomerCreateDataType RequestType();

        #! Response type - the created customer
        static WaveCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-customer";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new customer in Wave";
    }

    #! Makes a request and returns the response
    /** @param req the request data (customer fields to create)
        @param request_options the request options

        @return the created customer data
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.businessId.val(), "businessId is required");
        @assert(req.name.val(), "name is required");
        hash<auto> result = doMutation(CustomerCreateMutation, {"input": req}, "customerCreate", "customer");
        return flattenEdges(result);
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
