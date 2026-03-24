# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsEditVoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for editing a voice
public class ElevenLabsEditVoiceDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "edit-voice",
            "desc": "Edit an existing ElevenLabs voice",
            "type": "ElevenLabsEditVoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsEditVoiceRequestType();

        #! Response type
        const ResponseType = new ElevenLabsEditVoiceResponseType();
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
        string voice_id = remove req.voice_id;
        auto resp = doRestCommand("POST", "v1/voices/" + voice_id + "/edit", req).body;
        return resp ?? {"status": "ok"};
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

#! Request type for editing a voice
public class ElevenLabsEditVoiceRequestType inherits HashDataType {
    public {
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the voice to edit",
                "desc": "The unique identifier of the voice to edit",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "voices",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The new name for the voice",
                "desc": "The new name for the voice",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The new description for the voice",
                "desc": "The new description for the voice",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "labels": {
                "display_name": "Labels",
                "short_desc": "Labels for the voice",
                "desc": "Key-value labels to associate with the voice",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsEditVoiceRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for editing a voice
public class ElevenLabsEditVoiceResponseType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the operation",
                "desc": "The status of the edit operation",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsEditVoiceResponse") {
        addQoreFields(Fields);
    }
}

}
