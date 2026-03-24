# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsCreatePronunciationDictionaryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a pronunciation dictionary
public class ElevenLabsCreatePronunciationDictionaryDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-pronunciation-dictionary",
            "desc": "Create a new ElevenLabs pronunciation dictionary",
            "type": "ElevenLabsCreatePronunciationDictionaryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsCreatePronunciationDictionaryRequestType();

        #! Response type
        const ResponseType = new ElevenLabsCreatePronunciationDictionaryResponseType();
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
        return doRestCommand("POST", "v1/pronunciation-dictionaries/add-from-rules", req).body;
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

#! Request type for creating a pronunciation dictionary
public class ElevenLabsCreatePronunciationDictionaryRequestType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the dictionary",
                "desc": "The name for the new pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Dictionary description",
                "desc": "A description of the pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {"preselected": True},
            },
            "workspace_access": {
                "display_name": "Workspace Access",
                "short_desc": "Access level for workspace members",
                "desc": "The access level for workspace members",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Editor",
                        "value": "editor",
                        "desc": "Workspace members can edit the dictionary",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Viewer",
                        "value": "viewer",
                        "desc": "Workspace members can only view the dictionary",
                    },
                ),
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreatePronunciationDictionaryRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for creating a pronunciation dictionary
public class ElevenLabsCreatePronunciationDictionaryResponseType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the created dictionary",
                "desc": "The unique identifier of the newly created pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the dictionary",
                "desc": "The name of the pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the dictionary",
                "desc": "The description of the pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreatePronunciationDictionaryResponse") {
        addQoreFields(Fields);
    }
}

}
