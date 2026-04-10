# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SystemeIoRemoveContactTagDataProvider.qc Systeme.io remove tag from contact action data provider

/*  SystemeIoRemoveContactTagDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Systeme.io remove tag from contact action data provider
public class SystemeIoRemoveContactTagDataProvider inherits SystemeIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove-tag",
            "desc": "Remove a tag from a contact in Systeme.io",
            "type": "SystemeIoRemoveContactTagDataProvider",
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
        return "remove-tag";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SystemeIoRemoveContactTagRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
    }

    #! Executes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("contacts/" + string(req.id) + "/tags/" + string(req.tagId));
        return {};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SystemeIoDataProvider
