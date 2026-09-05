# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiTranscribeAudioDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! Transcribes audio into text with the OpenAI audio transcription API
/** The audio file is sent as a \c multipart/form-data upload; the response depends on the \c response_format
    request field: JSON formats are returned as parsed hashes, while the \c text, \c srt, and \c vtt formats are
    returned in the \c text key of the response.

    @since OpenAiDataProvider 1.7
*/
public class OpenAiTranscribeAudioDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "transcribe-audio",
            "desc": "Transcribes an audio file into text",
            "type": "OpenAiTranscribeAudioDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiTranscribeAudioRequestType;

        #! Response type
        const ResponseType = OpenAiTranscriptionDataType;

        #! The REST path for audio transcriptions
        const TranscriptionsPath = "audio/transcriptions";

        #! Audio file name extensions accepted by the transcription API
        const SupportedAudioExtensions = {
            "flac": True,
            "m4a": True,
            "mp3": True,
            "mp4": True,
            "mpeg": True,
            "mpga": True,
            "oga": True,
            "ogg": True,
            "wav": True,
            "webm": True,
        };

        #! The maximum audio file size accepted by the transcription API in bytes (25 MB)
        const MaxAudioFileSize = 25 * 1024 * 1024;

        #! Response formats returned by the API as raw text instead of JSON
        const TextResponseFormats = {
            "text": True,
            "srt": True,
            "vtt": True,
        };
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** The file is decoded from its base64 representation and sent as the \c file part of a
        \c multipart/form-data request together with one part for each other request field with a value.

        @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request

        @throw TRANSCRIBE-AUDIO-ERROR the audio file has an unsupported name extension or exceeds the maximum size
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = remove req.file;
        binary content = parse_base64_string(file.content);
        checkAudioFile(file.name, content);

        Mime::MultiPartFormDataMessage msg();
        msg.addPart(content, "file", file.name, file.mime_type ?? MimeTypeOctetStream);
        foreach hash<auto> i in (req.pairIterator()) {
            if (exists i.value) {
                msg.addPart(string(i.value), i.key, {"Content-Type": MimeTypeText});
            }
        }
        hash<MessageInfo> mh = msg.getMsgAndHeaders();

        auto body = doRestRawCommand("POST", TranscriptionsPath, mh.body, mh.hdr."Content-Type").body;
        # the text, srt, and vtt formats are returned by the API as a raw text body
        if (TextResponseFormats{req.response_format} || body.typeCode() != NT_HASH) {
            return {
                "text": body.typeCode() == NT_BINARY ? body.toString() : string(body),
            };
        }
        return body;
    }

    #! Validates the audio file name extension and size before uploading
    /** @param name the file name
        @param content the decoded file content

        @throw TRANSCRIBE-AUDIO-ERROR the audio file has an unsupported name extension or exceeds the maximum size
    */
    private checkAudioFile(string name, binary content) {
        *string ext = (name =~ x/\.([^.\/]+)$/)[0];
        if (!ext.val() || !SupportedAudioExtensions{ext.lwr()}) {
            throw "TRANSCRIBE-AUDIO-ERROR", sprintf("audio file %y has an unsupported format; the file name must "
                "have one of the following extensions: %y", name, keys SupportedAudioExtensions);
        }
        if (content.size() > MaxAudioFileSize) {
            throw "TRANSCRIBE-AUDIO-ERROR", sprintf("audio file %y has %d bytes; the maximum size accepted by the "
                "transcription API is %d bytes", name, content.size(), MaxAudioFileSize);
        }
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The audio transcription request type
/** @since OpenAiDataProvider 1.7
*/
public class OpenAiTranscribeAudioRequestType inherits HashDataType {
    public {
        #! The default transcription model
        const DefaultModel = "gpt-4o-transcribe";

        #! The default response format
        const DefaultResponseFormat = "json";

        #! fields
        const Fields = {
            "file": {
                "display_name": "Audio File",
                "short_desc": "The audio file to transcribe",
                "desc": "The audio file to transcribe, in one of the following formats: `flac`, `mp3`, `mp4`, "
                    "`mpeg`, `mpga`, `m4a`, `ogg`, `wav`, or `webm`; the file name must have a matching "
                    "extension. The maximum file size is 25 MB",
                "type": FileDataType,
            },
            "model": {
                "display_name": "Model",
                "short_desc": "The speech-to-text model to use",
                "desc": "The speech-to-text model to use for the transcription",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "GPT-4o Transcribe",
                        "value": "gpt-4o-transcribe",
                        "desc": "The most accurate speech-to-text model; supports the `json` response format only",
                    },
                    <AllowedValueInfo>{
                        "display_name": "GPT-4o Mini Transcribe",
                        "value": "gpt-4o-mini-transcribe",
                        "desc": "A faster and cheaper speech-to-text model; supports the `json` response format "
                            "only",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Whisper",
                        "value": "whisper-1",
                        "desc": "The open source Whisper V2 model; supports all response formats",
                    },
                ),
                "default_value": DefaultModel,
            },
            "language": {
                "display_name": "Language",
                "short_desc": "The language of the input audio as an ISO-639-1 code",
                "desc": "The language of the input audio in "
                    "[ISO-639-1](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes) format (for example "
                    "`en`); supplying the input language improves accuracy and latency",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "en",
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Optional text to guide the model's style or continue a previous audio segment",
                "desc": "Optional text to guide the model's style or to continue a previous audio segment; the "
                    "prompt should match the audio language",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "The format of the transcription output",
                "desc": "The format of the transcription output.\n\n"
                    "**Note**: `gpt-4o-transcribe` and `gpt-4o-mini-transcribe` support only `json`; the `text`, "
                    "`srt`, `verbose_json`, and `vtt` formats require `whisper-1`. Non-JSON output is returned in "
                    "the `text` field of the response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "JSON",
                        "value": "json",
                        "desc": "A JSON object with the transcribed text",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Plain Text",
                        "value": "text",
                        "desc": "The transcribed text only (`whisper-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "SRT Subtitles",
                        "value": "srt",
                        "desc": "SubRip subtitles with timestamps (`whisper-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Verbose JSON",
                        "value": "verbose_json",
                        "desc": "A JSON object with the transcribed text, language, duration, and timestamped "
                            "segments (`whisper-1` only)",
                    },
                    <AllowedValueInfo>{
                        "display_name": "WebVTT Subtitles",
                        "value": "vtt",
                        "desc": "Web Video Text Tracks subtitles with timestamps (`whisper-1` only)",
                    },
                ),
                "default_value": DefaultResponseFormat,
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "The sampling temperature between 0 and 1",
                "desc": "The sampling temperature, between `0` and `1`; higher values like `0.8` make the output "
                    "more random, while lower values like `0.2` make it more focused and deterministic. If set to "
                    "`0`, the model uses log probability to automatically increase the temperature until certain "
                    "thresholds are hit",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTranscribeAudioRequest") {
        addQoreFields(Fields);
    }
}

#! Audio transcription request type constant
/** @since OpenAiDataProvider 1.7
*/
public const OpenAiTranscribeAudioRequestType = new OpenAiTranscribeAudioRequestType();

#! A timestamped word in a verbose transcription
/** @since OpenAiDataProvider 1.7
*/
public class OpenAiTranscriptionWordDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "word": {
                "display_name": "Word",
                "short_desc": "The text content of the word",
                "desc": "The text content of the word",
                "type": AbstractDataProviderTypeMap."string",
            },
            "start": {
                "display_name": "Start Time",
                "short_desc": "Start time of the word in seconds",
                "desc": "Start time of the word in seconds",
                "type": AbstractDataProviderTypeMap."float",
            },
            "end": {
                "display_name": "End Time",
                "short_desc": "End time of the word in seconds",
                "desc": "End time of the word in seconds",
                "type": AbstractDataProviderTypeMap."float",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTranscriptionWord") {
        addQoreFields(Fields);
    }
}

#! Transcription word type constant
/** @since OpenAiDataProvider 1.7
*/
public const OpenAiTranscriptionWordDataType = new OpenAiTranscriptionWordDataType();

#! A timestamped segment in a verbose transcription
/** @since OpenAiDataProvider 1.7
*/
public class OpenAiTranscriptionSegmentDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Segment ID",
                "short_desc": "Unique identifier of the segment",
                "desc": "Unique identifier of the segment",
                "type": AbstractDataProviderTypeMap."int",
            },
            "seek": {
                "display_name": "Seek Offset",
                "short_desc": "Seek offset of the segment",
                "desc": "Seek offset of the segment",
                "type": AbstractDataProviderTypeMap."int",
            },
            "start": {
                "display_name": "Start Time",
                "short_desc": "Start time of the segment in seconds",
                "desc": "Start time of the segment in seconds",
                "type": AbstractDataProviderTypeMap."float",
            },
            "end": {
                "display_name": "End Time",
                "short_desc": "End time of the segment in seconds",
                "desc": "End time of the segment in seconds",
                "type": AbstractDataProviderTypeMap."float",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text content of the segment",
                "desc": "Text content of the segment",
                "type": AbstractDataProviderTypeMap."string",
            },
            "tokens": {
                "display_name": "Tokens",
                "short_desc": "Array of token IDs for the text content",
                "desc": "Array of token IDs for the text content",
                "type": new ListDataType("TranscriptionSegmentTokenList", AbstractDataProviderTypeMap."int"),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Temperature parameter used for generating the segment",
                "desc": "Temperature parameter used for generating the segment",
                "type": AbstractDataProviderTypeMap."float",
            },
            "avg_logprob": {
                "display_name": "Average Log Probability",
                "short_desc": "Average logprob of the segment",
                "desc": "Average logprob of the segment; if the value is lower than `-1`, consider the logprobs "
                    "failed",
                "type": AbstractDataProviderTypeMap."float",
            },
            "compression_ratio": {
                "display_name": "Compression Ratio",
                "short_desc": "Compression ratio of the segment",
                "desc": "Compression ratio of the segment; if the value is greater than `2.4`, consider the "
                    "compression failed",
                "type": AbstractDataProviderTypeMap."float",
            },
            "no_speech_prob": {
                "display_name": "No Speech Probability",
                "short_desc": "Probability of no speech in the segment",
                "desc": "Probability of no speech in the segment; if the value is higher than `1.0` and the "
                    "`avg_logprob` is below `-1`, consider this segment silent",
                "type": AbstractDataProviderTypeMap."float",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTranscriptionSegment") {
        addQoreFields(Fields);
    }
}

#! Transcription segment type constant
/** @since OpenAiDataProvider 1.7
*/
public const OpenAiTranscriptionSegmentDataType = new OpenAiTranscriptionSegmentDataType();

#! The audio transcription response type
/** Only \c text is always present; the other fields depend on the model and the \c response_format used

    @since OpenAiDataProvider 1.7
*/
public class OpenAiTranscriptionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The transcribed text",
                "desc": "The transcribed text; for the `text`, `srt`, and `vtt` response formats, the raw output in "
                    "the requested format",
                "type": AbstractDataProviderTypeMap."string",
            },
            "task": {
                "display_name": "Task",
                "short_desc": "The task performed (verbose JSON only)",
                "desc": "The task performed; always `transcribe` (`verbose_json` response format only)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "language": {
                "display_name": "Language",
                "short_desc": "The language of the input audio (verbose JSON only)",
                "desc": "The language of the input audio (`verbose_json` response format only)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "duration": {
                "display_name": "Duration",
                "short_desc": "The duration of the input audio in seconds (verbose JSON only)",
                "desc": "The duration of the input audio in seconds (`verbose_json` response format only)",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "segments": {
                "display_name": "Segments",
                "short_desc": "Timestamped segments of the transcribed text (verbose JSON only)",
                "desc": "Timestamped segments of the transcribed text and their corresponding details "
                    "(`verbose_json` response format only)",
                "type": new ListDataType("*TranscriptionSegmentList", OpenAiTranscriptionSegmentDataType, True),
            },
            "words": {
                "display_name": "Words",
                "short_desc": "Timestamped words of the transcribed text (verbose JSON only)",
                "desc": "Timestamped words of the transcribed text and their corresponding details "
                    "(`verbose_json` response format only)",
                "type": new ListDataType("*TranscriptionWordList", OpenAiTranscriptionWordDataType, True),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token or duration usage statistics for the request (JSON only)",
                "desc": "Usage statistics for the request; token counts (`type` = `tokens`) for `gpt-4o-transcribe` "
                    "and `gpt-4o-mini-transcribe`, or the audio duration in seconds (`type` = `duration`) for "
                    "`whisper-1` (`json` response format only)",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTranscription") {
        addQoreFields(Fields);
    }
}

#! Audio transcription response type constant
/** @since OpenAiDataProvider 1.7
*/
public const OpenAiTranscriptionDataType = new OpenAiTranscriptionDataType();
}
