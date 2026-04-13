# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SystemeIoUpdateContactFieldDataProvider.qc Systeme.io update contact field action data provider

/*  SystemeIoUpdateContactFieldDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io update contact field action data provider
public class SystemeIoUpdateContactFieldDataProvider inherits SystemeIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing contact field in Systeme.io",
            "type": "SystemeIoUpdateContactFieldDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SystemeIoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SystemeIoUpdateContactFieldRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SystemeIoContactFieldDataType();
    }

    #! Executes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        auto slug = req.slug;
        hash<auto> body = req - "slug";
        return doPatch("contact_fields/" + string(slug), body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SystemeIoDataProvider
