# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsVoiceDesignDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! ElevenLabs voice design data provider
/** Creates voice previews from a text description using the ElevenLabs API
*/
public class ElevenLabsVoiceDesignDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "voice-design",
            "desc": "Creates voice previews from a text description of desired voice characteristics",
            "type": "ElevenLabsVoiceDesignDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsVoiceDesignRequestType();

        #! Response type
        const ResponseType = new ElevenLabsVoiceDesignResponseType();
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : ElevenLabsDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "v1/text-to-voice/create-previews", req).body;
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

#! Voice design request type
public class ElevenLabsVoiceDesignRequestType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "voice_description": {
                "display_name": "Voice Description",
                "short_desc": "Text describing desired voice characteristics",
                "desc": "A text description of the desired voice characteristics (e.g., `a warm friendly female "
                    "voice with a slight British accent`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "A warm friendly female voice with a slight British accent",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Sample text to generate with the designed voice",
                "desc": "Sample text that will be spoken by the generated voice previews",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Hello, how are you today? I hope you're having a wonderful day.",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsVoiceDesignRequest") {
        addQoreFields(Fields);
    }
}

#! Voice design response type
public class ElevenLabsVoiceDesignResponseType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "previews": {
                "display_name": "Previews",
                "short_desc": "List of voice preview objects",
                "desc": "A list of voice preview objects, each containing audio data and metadata for the "
                    "designed voice",
                "type": new SoftListDataType("VoicePreviews", new ElevenLabsVoicePreviewDataType(), False),
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The text spoken in the previews",
                "desc": "The text that was spoken by the generated voice previews",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsVoiceDesignResponse") {
        addQoreFields(Fields);
    }
}
}
