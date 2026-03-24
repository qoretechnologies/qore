# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsGetDubbingAudioDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting audio from a dubbing project
public class ElevenLabsGetDubbingAudioDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-dubbing-audio",
            "desc": "Get dubbed audio data from an ElevenLabs dubbing project for a specific language",
            "type": "ElevenLabsGetDubbingAudioDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsGetDubbingAudioRequestType();

        #! Response type - uses shared AudioResponseType from ElevenLabsDataProviderDefs.qc
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
    /** @param req the request hash with \c dubbing_id and \c language_code
        @param request_options request options

        @return a hash with \c audio (binary) and \c content_type (string) keys
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET",
            "v1/dubbing/" + req.dubbing_id + "/audio/" + req.language_code);
        return {
            "audio": resp.body,
            "content_type": resp.content_type ?? "audio/mpeg",
        };
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

#! Request type for getting dubbing audio
public class ElevenLabsGetDubbingAudioRequestType inherits HashDataType {
    public {
        const Fields = {
            "dubbing_id": {
                "display_name": "Dubbing ID",
                "short_desc": "The ID of the dubbing project",
                "desc": "The unique identifier of the dubbing project to get audio for",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "dubbing",
            },
            "language_code": {
                "display_name": "Language Code",
                "short_desc": "The target language code",
                "desc": "The language code for the dubbed audio to download (e.g., `es`, `fr`, `de`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "es",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetDubbingAudioRequest") {
        addQoreFields(Fields);
    }
}

}
