# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsTextToSoundEffectsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! ElevenLabs text-to-sound-effects data provider
/** Generates sound effects from a text description using the ElevenLabs API
*/
public class ElevenLabsTextToSoundEffectsDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "text-to-sound-effects",
            "desc": "Generates sound effects audio from a text description",
            "type": "ElevenLabsTextToSoundEffectsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsTextToSoundEffectsRequestType();

        #! Response type
        const ResponseType = AudioResponseType;
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
        hash<auto> resp = doRawPost("v1/text-to-sound-effects/convert", req);
        return {
            "audio": resp.body,
            "content_type": resp."content-type" ?? "audio/mpeg",
        };
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

#! Text-to-sound-effects request type
public class ElevenLabsTextToSoundEffectsRequestType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "Description of the sound effect to generate",
                "desc": "A text description of the sound effect to generate (e.g., `dog barking in a park`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Dog barking in a park on a sunny day",
            },
            "duration_seconds": {
                "display_name": "Duration (Seconds)",
                "short_desc": "Duration of the sound effect in seconds",
                "desc": "The duration of the generated sound effect in seconds. Range: `0.5` to `22.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "attr": {"preselected": True},
            },
            "prompt_influence": {
                "display_name": "Prompt Influence",
                "short_desc": "How much the text prompt influences the output",
                "desc": "How much the text prompt influences the generated sound effect. Range: `0.0` to `1.0`. "
                    "Higher values make the output more closely match the prompt",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "loop": {
                "display_name": "Loop",
                "short_desc": "Whether to create a looping sound effect",
                "desc": "If `True`, creates a seamlessly looping sound effect",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsTextToSoundEffectsRequest") {
        addQoreFields(Fields);
    }
}
}
