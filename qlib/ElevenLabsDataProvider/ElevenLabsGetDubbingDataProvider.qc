# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsGetDubbingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElevenLabsDataProvider module
public namespace ElevenLabsDataProvider {

#! Data provider for getting dubbing project status
public class ElevenLabsGetDubbingDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-dubbing",
            "desc": "Get the status of an ElevenLabs dubbing project",
            "type": "ElevenLabsGetDubbingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsGetDubbingRequestType();

        #! Response type
        const ResponseType = new ElevenLabsGetDubbingResponseType();
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : ElevenLabsDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "v1/dubbing/" + req.dubbing_id).body;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request type for getting dubbing status
public class ElevenLabsGetDubbingRequestType inherits HashDataType {
    public {
        const Fields = {
            "dubbing_id": {
                "display_name": "Dubbing ID",
                "short_desc": "The ID of the dubbing project",
                "desc": "The unique identifier of the dubbing project to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "dubbing",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetDubbingRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for getting dubbing status
public class ElevenLabsGetDubbingResponseType inherits HashDataType {
    public {
        const Fields = {
            "dubbing_id": {
                "display_name": "Dubbing ID",
                "short_desc": "The unique identifier of the dubbing project",
                "desc": "The unique identifier of the dubbing project",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the dubbing project",
                "desc": "The name of the dubbing project",
                "type": AbstractDataProviderTypeMap."string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The current status of the dubbing",
                "desc": "The current processing status of the dubbing project",
                "type": AbstractDataProviderTypeMap."string",
            },
            "target_languages": {
                "display_name": "Target Languages",
                "short_desc": "List of target languages",
                "desc": "The list of target languages for the dubbing project",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error message if dubbing failed",
                "desc": "The error message if the dubbing project failed",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetDubbingResponse") {
        addQoreFields(Fields);
    }
}

}
