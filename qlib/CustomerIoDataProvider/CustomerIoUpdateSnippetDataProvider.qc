# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoUpdateSnippetDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a snippet
public class CustomerIoUpdateSnippetRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "snippet_name": {
                "type": StringType,
                "display_name": "Snippet Name",
                "short_desc": "The snippet name to update",
                "desc": "The unique name identifier for the snippet to update",
                "required": True,
                "preselected": True,
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "Snippet content",
                "desc": "The updated content of the snippet",
                "required": True,
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a snippet
public class CustomerIoUpdateSnippetDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-snippet",
            "desc": "Update a Customer.io snippet",
            "type": "CustomerIoUpdateSnippetDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoUpdateSnippetRequestDataType RequestType();

        #! Response type
        static CustomerIoSnippetDataType ResponseType();
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
        return "update-snippet";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a Customer.io snippet";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "name": req.snippet_name,
            "value": req.value,
        };
        return doPut("snippets", body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
