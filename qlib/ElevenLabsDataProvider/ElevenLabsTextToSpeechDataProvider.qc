# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsTextToSpeechDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! ElevenLabs text-to-speech data provider
/** Converts text to speech audio using the ElevenLabs API
*/
public class ElevenLabsTextToSpeechDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "text-to-speech",
            "desc": "Converts text to speech audio using a specified voice",
            "type": "ElevenLabsTextToSpeechDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsTextToSpeechRequestType();

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
        string voice_id = remove req.voice_id;
        hash<auto> resp = doRawPost("v1/text-to-speech/" + voice_id, req);
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

#! Text-to-speech request type
public class ElevenLabsTextToSpeechRequestType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the voice to use for speech synthesis",
                "desc": "The ID of the voice to use for speech synthesis",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "voices",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The text to convert to speech",
                "desc": "The text to convert to speech",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Hello, world! This is a test of text-to-speech.",
            },
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "The TTS model to use",
                "desc": "The TTS model to use for speech synthesis",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": TtsModelAllowedValues,
                "ref_data": "models",
            },
            "voice_settings": {
                "display_name": "Voice Settings",
                "short_desc": "Voice settings for the synthesis",
                "desc": "Voice settings overriding stored settings for the given voice. They are applied only on "
                    "the given request",
                "type": VoiceSettingsType,
            },
            "output_format": {
                "display_name": "Output Format",
                "short_desc": "The output audio format",
                "desc": "The output audio format for the generated speech",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": OutputFormatAllowedValues,
            },
            "language_code": {
                "display_name": "Language Code",
                "short_desc": "Language code for the speech (ISO 639-1)",
                "desc": "Language code (ISO 639-1) used to enforce a language for the model. Currently only "
                    "supported by `eleven_turbo_v2_5`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Seed for deterministic generation",
                "desc": "If specified, the system will make a best effort to sample deterministically, such that "
                    "repeated requests with the same seed and parameters should return the same result. "
                    "Determinism is not guaranteed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "speed": {
                "display_name": "Speed",
                "short_desc": "Speech speed multiplier",
                "desc": "Controls the speed of the generated speech. Range: `0.25` to `4.0`. Default: `1.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 1.0,
            },
            "previous_text": {
                "display_name": "Previous Text",
                "short_desc": "Text that came before for continuity",
                "desc": "Text that came before the current text in a multi-part generation. Improves prosody "
                    "and continuity when splitting long text across multiple requests",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "next_text": {
                "display_name": "Next Text",
                "short_desc": "Text that comes after for continuity",
                "desc": "Text that comes after the current text in a multi-part generation. Enhances "
                    "continuity when concatenating audio from multiple requests",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "apply_text_normalization": {
                "display_name": "Text Normalization",
                "short_desc": "Controls text normalization behavior",
                "desc": "Controls how text is normalized before synthesis",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "value": "auto",
                        "desc": "Automatically decide whether to normalize text (default)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "On",
                        "value": "on",
                        "desc": "Always apply text normalization",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Off",
                        "value": "off",
                        "desc": "Disable text normalization",
                    },
                ),
            },
            "pronunciation_dictionary_locators": {
                "display_name": "Pronunciation Dictionaries",
                "short_desc": "Pronunciation dictionaries to apply",
                "desc": "A list of pronunciation dictionary locators to apply during synthesis. Maximum of "
                    "3 dictionaries",
                "type": new SoftListDataType("PronunciationDictionaryLocators",
                    new ElevenLabsPronunciationDictionaryLocatorType(), True),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsTextToSpeechRequest") {
        addQoreFields(Fields);
    }
}
}
