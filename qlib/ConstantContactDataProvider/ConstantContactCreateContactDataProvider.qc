# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactCreateContactDataProvider.qc Constant Contact create contact action data provider

/*  ConstantContactCreateContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Constant Contact create contact action data provider
public class ConstantContactCreateContactDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in Constant Contact",
            "type": "ConstantContactCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactCreateContactRequestDataType RequestType();

        #! Static response type
        static ConstantContactContactDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new contact in Constant Contact";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactCreateContactRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConstantContactContactDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("contacts", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
