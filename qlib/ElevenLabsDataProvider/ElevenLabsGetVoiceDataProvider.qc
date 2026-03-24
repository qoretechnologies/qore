# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsGetVoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single voice
public class ElevenLabsGetVoiceDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-voice",
            "desc": "Get details for a single ElevenLabs voice",
            "type": "ElevenLabsGetVoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsGetVoiceRequestType();

        #! Response type
        const ResponseType = new ElevenLabsGetVoiceResponseType();
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
        return doRestCommand("GET", "v1/voices/" + req.voice_id).body;
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

#! Request type for getting a voice
public class ElevenLabsGetVoiceRequestType inherits HashDataType {
    public {
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the voice to retrieve",
                "desc": "The unique identifier of the voice to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "voices",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetVoiceRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for getting a voice
public class ElevenLabsGetVoiceResponseType inherits HashDataType {
    public {
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The unique identifier of the voice",
                "desc": "The unique identifier of the voice",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the voice",
                "desc": "The name of the voice",
                "type": AbstractDataProviderTypeMap."string",
            },
            "category": {
                "display_name": "Category",
                "short_desc": "The category of the voice",
                "desc": "The category of the voice (e.g., `premade`, `cloned`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the voice",
                "desc": "The description of the voice",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "labels": {
                "display_name": "Labels",
                "short_desc": "Labels associated with the voice",
                "desc": "Key-value labels associated with the voice",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "samples": {
                "display_name": "Samples",
                "short_desc": "Voice samples",
                "desc": "Audio samples associated with the voice",
                "type": new SoftListDataType("VoiceSamples", new ElevenLabsVoiceSampleDataType(), False),
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Voice settings",
                "desc": "The voice settings including stability, similarity boost, style, and speed",
                "type": VoiceSettingsType,
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetVoiceResponse") {
        addQoreFields(Fields);
    }
}

}
