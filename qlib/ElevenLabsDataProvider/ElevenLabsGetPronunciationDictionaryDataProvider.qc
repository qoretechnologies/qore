# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsGetPronunciationDictionaryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single pronunciation dictionary
public class ElevenLabsGetPronunciationDictionaryDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-pronunciation-dictionary",
            "desc": "Get details for a single ElevenLabs pronunciation dictionary",
            "type": "ElevenLabsGetPronunciationDictionaryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsGetPronunciationDictionaryRequestType();

        #! Response type
        const ResponseType = new ElevenLabsGetPronunciationDictionaryResponseType();
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
        return doRestCommand("GET", "v1/pronunciation-dictionaries/" + req.dictionary_id).body;
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

#! Request type for getting a pronunciation dictionary
public class ElevenLabsGetPronunciationDictionaryRequestType inherits HashDataType {
    public {
        const Fields = {
            "dictionary_id": {
                "display_name": "Dictionary ID",
                "short_desc": "The ID of the pronunciation dictionary",
                "desc": "The unique identifier of the pronunciation dictionary to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "pronunciation_dictionaries",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetPronunciationDictionaryRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for getting a pronunciation dictionary
public class ElevenLabsGetPronunciationDictionaryResponseType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the dictionary",
                "desc": "The unique identifier of the pronunciation dictionary",
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
            "created_by": {
                "display_name": "Created By",
                "short_desc": "The user who created the dictionary",
                "desc": "The user ID of the person who created the dictionary",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "creation_time_unix": {
                "display_name": "Creation Time",
                "short_desc": "The creation timestamp",
                "desc": "The Unix timestamp when the dictionary was created",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetPronunciationDictionaryResponse") {
        addQoreFields(Fields);
    }
}

}
