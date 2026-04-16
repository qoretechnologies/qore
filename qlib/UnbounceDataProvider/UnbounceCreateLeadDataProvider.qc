# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceCreateLeadDataProvider.qc Unbounce create lead action data provider

/*  UnbounceCreateLeadDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce create lead action data provider
public class UnbounceCreateLeadDataProvider inherits UnbounceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new lead in Unbounce on a specific page",
            "type": "UnbounceCreateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new UnbounceLeadDataType();
        static *AbstractDataProviderType RequestType = new UnbounceCreateLeadRequestDataType();
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new lead in Unbounce on a specific page";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new UnbounceCreateLeadRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new UnbounceLeadDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        string page_id = remove body.page_id;
        return doPost("pages/" + page_id + "/leads", body);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
