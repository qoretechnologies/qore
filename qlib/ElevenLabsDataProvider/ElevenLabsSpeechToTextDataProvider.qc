# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsSpeechToTextDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! ElevenLabs speech-to-text data provider
/** Transcribes audio to text using the ElevenLabs API
*/
public class ElevenLabsSpeechToTextDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "speech-to-text",
            "desc": "Transcribes audio to text with optional word-level timestamps and speaker diarization",
            "type": "ElevenLabsSpeechToTextDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsSpeechToTextRequestType();

        #! Response type
        const ResponseType = new ElevenLabsSpeechToTextResponseType();
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
        # Map 'audio' to 'file' - the API parameter name
        if (req.audio) {
            req.file = remove req.audio;
        }
        return doMultipartPost("v1/speech-to-text", req).body;
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

#! Speech-to-text request type
public class ElevenLabsSpeechToTextRequestType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "audio": {
                "display_name": "Audio",
                "short_desc": "The audio data to transcribe",
                "desc": "The audio data to transcribe to text",
                "type": AbstractDataProviderTypeMap."binary",
                "required": True,
            },
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "The STT model to use",
                "desc": "The speech-to-text model to use for transcription",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "scribe_v2",
                "allowed_values": SttModelAllowedValues,
                "ref_data": "models",
            },
            "language_code": {
                "display_name": "Language Code",
                "short_desc": "Language code for the audio (ISO 639-1)",
                "desc": "An ISO 639-1 language code to specify the language of the audio. If not provided, the "
                    "model will auto-detect the language",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tag_audio_events": {
                "display_name": "Tag Audio Events",
                "short_desc": "Whether to tag audio events like music or laughter",
                "desc": "If `True`, audio events such as music, laughter, and applause will be tagged in the "
                    "transcription",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "diarize": {
                "display_name": "Diarize",
                "short_desc": "Whether to annotate who is speaking",
                "desc": "If `True`, the transcription will include speaker diarization, annotating which speaker "
                    "is talking in each segment",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "timestamps_granularity": {
                "display_name": "Timestamps Granularity",
                "short_desc": "The granularity of timestamps in the response",
                "desc": "The granularity of timestamps in the transcription response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "None",
                        "value": "none",
                        "desc": "No timestamps in the response",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Word",
                        "value": "word",
                        "desc": "Word-level timestamps",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Character",
                        "value": "character",
                        "desc": "Character-level timestamps",
                    },
                ),
            },
            "num_speakers": {
                "display_name": "Number of Speakers",
                "short_desc": "Maximum number of speakers in the audio",
                "desc": "The maximum number of speakers expected in the audio. Maximum: `32`. Only used when "
                    "`diarize` is `True`",
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
    constructor() : HashDataType("ElevenLabsSpeechToTextRequest") {
        addQoreFields(Fields);
    }
}

#! Speech-to-text response type
public class ElevenLabsSpeechToTextResponseType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The transcribed text",
                "desc": "The full transcribed text from the audio",
                "type": AbstractDataProviderTypeMap."string",
            },
            "language_code": {
                "display_name": "Language Code",
                "short_desc": "The detected language code",
                "desc": "The detected or specified language code (ISO 639-1) of the audio",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "language_probability": {
                "display_name": "Language Probability",
                "short_desc": "Confidence of the detected language",
                "desc": "The confidence score of the detected language. Range: `0.0` to `1.0`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "words": {
                "display_name": "Words",
                "short_desc": "Word-level timing data",
                "desc": "A list of word-level timing data, including start and end times for each word. Only "
                    "present when `timestamps_granularity` is set",
                "type": new SoftListDataType("Words", new ElevenLabsWordTimingDataType(), False),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("ElevenLabsSpeechToTextResponse") {
        addQoreFields(Fields);
    }
}
}
