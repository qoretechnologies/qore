# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsCreateVoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating/cloning a voice
public class ElevenLabsCreateVoiceDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-voice",
            "desc": "Create a new ElevenLabs voice with metadata",
            "type": "ElevenLabsCreateVoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsCreateVoiceRequestType();

        #! Response type
        const ResponseType = new ElevenLabsCreateVoiceResponseType();
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
        return doMultipartPost("v1/voices/add", req).body;
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

#! Request type for creating a voice
public class ElevenLabsCreateVoiceRequestType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the voice",
                "desc": "The name for the new voice",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "files": {
                "display_name": "Audio Files",
                "short_desc": "Audio samples for voice cloning",
                "desc": "A list of audio sample files (as binary data) to use for voice cloning. Each file should "
                    "be a clear recording of the target voice",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."binary", True),
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Voice description",
                "desc": "A description of the voice",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {"preselected": True},
            },
            "labels": {
                "display_name": "Labels",
                "short_desc": "Labels for the voice",
                "desc": "Key-value labels to associate with the voice",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreateVoiceRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for creating a voice
public class ElevenLabsCreateVoiceResponseType inherits HashDataType {
    public {
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the created voice",
                "desc": "The unique identifier of the newly created voice",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreateVoiceResponse") {
        addQoreFields(Fields);
    }
}

}
