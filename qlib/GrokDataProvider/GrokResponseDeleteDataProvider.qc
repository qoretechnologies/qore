# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokResponseDeleteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GrokDataProvider module
public namespace GrokDataProvider {
#! Grok response delete data provider - deletes a stored response by ID
public class GrokResponseDeleteDataProvider inherits GrokDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "GrokResponseDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GrokResponseDeleteRequestType;

        #! Response type
        const ResponseType = GrokDeleteResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : GrokDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("DELETE", "responses/" + req.id).body;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Response delete request type
public class GrokResponseDeleteRequestType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "short_desc": "The ID of the response to delete",
                "desc": "The unique identifier of the response to delete",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() : HashDataType("GrokResponseDeleteRequest") {
        addQoreFields(Fields);
    }
}

public const GrokResponseDeleteRequestType = new GrokResponseDeleteRequestType();

#! Delete response type
public class GrokDeleteResponseType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Deleted response identifier",
                "desc": "The identifier of the deleted response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The object type",
                "desc": "The object type (always 'response')",
                "type": AbstractDataProviderTypeMap."string",
            },
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Confirmation of deletion",
                "desc": "Whether the response was successfully deleted",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }

    constructor() : HashDataType("GrokDeleteResponse") {
        addQoreFields(Fields);
    }
}

public const GrokDeleteResponseType = new GrokDeleteResponseType();
}
