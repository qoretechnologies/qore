# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsSpeechToSpeechDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! ElevenLabs speech-to-speech data provider
/** Converts speech audio to speech audio using a different voice via the ElevenLabs API.
*/
public class ElevenLabsSpeechToSpeechDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "speech-to-speech",
            "desc": "Converts speech audio to speech in a different voice",
            "type": "ElevenLabsSpeechToSpeechDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsSpeechToSpeechRequestType();

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
        hash<auto> resp = doMultipartPost("v1/speech-to-speech/" + voice_id, req);
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

#! Speech-to-speech request type
public class ElevenLabsSpeechToSpeechRequestType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the target voice for conversion",
                "desc": "The ID of the target voice for conversion",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "voices",
            },
            "audio": {
                "display_name": "Audio",
                "short_desc": "The source audio data to convert",
                "desc": "The source audio data to convert to the target voice",
                "type": AbstractDataProviderTypeMap."binary",
                "required": True,
            },
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "The STS model to use",
                "desc": "The speech-to-speech model to use for voice conversion",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": StsModelAllowedValues,
                "ref_data": "models",
            },
            "voice_settings": {
                "display_name": "Voice Settings",
                "short_desc": "Voice settings for the conversion",
                "desc": "Voice settings overriding stored settings for the given voice. They are applied only on "
                    "the given request",
                "type": VoiceSettingsType,
            },
            "output_format": {
                "display_name": "Output Format",
                "short_desc": "The output audio format",
                "desc": "The output audio format for the converted speech",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": OutputFormatAllowedValues,
            },
            "remove_background_noise": {
                "display_name": "Remove Background Noise",
                "short_desc": "Whether to remove background noise from the input audio",
                "desc": "If `True`, background noise will be removed from the input audio before conversion",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Seed for deterministic generation",
                "desc": "If specified, the system will make a best effort to sample deterministically, such that "
                    "repeated requests with the same seed and parameters should return the same result. "
                    "Determinism is not guaranteed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "file_format": {
                "display_name": "File Format",
                "short_desc": "The format of the input audio",
                "desc": "The format of the input audio file. `pcm_s16le_16` requires 16-bit, 16kHz mono, "
                    "little-endian PCM and offers lower latency",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Other (Auto-detect)",
                        "value": "other",
                        "desc": "Auto-detect the audio format (default)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "PCM 16-bit 16kHz",
                        "value": "pcm_s16le_16",
                        "desc": "PCM 16-bit, 16kHz mono, little-endian — lower latency",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsSpeechToSpeechRequest") {
        addQoreFields(Fields);
    }
}
}
