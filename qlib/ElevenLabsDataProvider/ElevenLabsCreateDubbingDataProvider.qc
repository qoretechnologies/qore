# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsCreateDubbingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating a dubbing project
public class ElevenLabsCreateDubbingDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-dubbing",
            "desc": "Create a new ElevenLabs dubbing project",
            "type": "ElevenLabsCreateDubbingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsCreateDubbingRequestType();

        #! Response type
        const ResponseType = new ElevenLabsCreateDubbingResponseType();
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
        return doMultipartPost("v1/dubbing", req).body;
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

#! Request type for creating a dubbing project
public class ElevenLabsCreateDubbingRequestType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the dubbing project",
                "desc": "The name for the new dubbing project",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "source_url": {
                "display_name": "Source URL",
                "short_desc": "URL of the source content",
                "desc": "The URL of the source audio or video content to dub",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source_lang": {
                "display_name": "Source Language",
                "short_desc": "The source language code",
                "desc": "The language code of the source content (e.g., `en`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "target_lang": {
                "display_name": "Target Language",
                "short_desc": "The target language code",
                "desc": "The language code to dub the content into (e.g., `es`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "file": {
                "display_name": "Source File",
                "short_desc": "Source audio or video file for dubbing",
                "desc": "The source audio or video file to dub. Provide either `file` or `source_url`, not both",
                "type": AbstractDataProviderTypeMap."*binary",
            },
            "num_speakers": {
                "display_name": "Number of Speakers",
                "short_desc": "Number of speakers in the content",
                "desc": "The number of speakers in the source content. Set to `0` to automatically detect the "
                    "number of speakers",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "watermark": {
                "display_name": "Watermark",
                "short_desc": "Whether to add a watermark",
                "desc": "If `True`, adds a watermark to the dubbed content",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "highest_resolution": {
                "display_name": "Highest Resolution",
                "short_desc": "Whether to use the highest resolution",
                "desc": "If `True`, uses the highest resolution available for the dubbed content",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "drop_background_audio": {
                "display_name": "Drop Background Audio",
                "short_desc": "Whether to remove background audio",
                "desc": "If `True`, removes background audio from the final dubbed output",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "start_time": {
                "display_name": "Start Time",
                "short_desc": "Start position in seconds",
                "desc": "The start position in seconds to clip the source content before dubbing",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "end_time": {
                "display_name": "End Time",
                "short_desc": "End position in seconds",
                "desc": "The end position in seconds to clip the source content before dubbing",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "use_profanity_filter": {
                "display_name": "Profanity Filter",
                "short_desc": "Whether to censor profanities in transcripts",
                "desc": "If `True`, profanities in the transcription will be censored. **Note**: this is a "
                    "beta feature",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreateDubbingRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for creating a dubbing project
public class ElevenLabsCreateDubbingResponseType inherits HashDataType {
    public {
        const Fields = {
            "dubbing_id": {
                "display_name": "Dubbing ID",
                "short_desc": "The ID of the created dubbing project",
                "desc": "The unique identifier of the newly created dubbing project",
                "type": AbstractDataProviderTypeMap."string",
            },
            "expected_duration_sec": {
                "display_name": "Expected Duration (seconds)",
                "short_desc": "Expected processing duration",
                "desc": "The expected duration in seconds for the dubbing to complete",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsCreateDubbingResponse") {
        addQoreFields(Fields);
    }
}

}
