# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepgramTranscribeFileDataProvider class definition

/** DeepgramTranscribeFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DeepgramDataProvider {
public class DeepgramTranscribeFileDataProvider inherits DeepgramDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "transcribe-file",
            "type": "DeepgramTranscribeFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static DeepgramTranscribeFileRequestDataType RequestType =
            new DeepgramTranscribeFileRequestDataType();
        static DeepgramTranscribeFileResponseDataType ResponseType =
            new DeepgramTranscribeFileResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : DeepgramDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : DeepgramDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        bool has_url = req.url.val();
        bool has_audio = exists req.audio;
        if (has_url == has_audio) {
            throw "DEEPGRAM-TRANSCRIBE-ERROR", sprintf("exactly one of \"url\" or \"audio\" must be "
                "provided; got %s", has_url ? "both" : "neither");
        }
        # The audio source goes in the request body; all other parameters go in the query string
        hash<auto> query_params = req - ("url", "audio");
        string path = "/v1/listen"
            + DeepgramDataProviderCommon::buildQueryString(query_params);
        if (has_audio) {
            # the audio bytes are the body; Deepgram reads the container format from the Content-Type
            return doRawRestCommand("POST", path, getAudioContent(req.audio),
                getAudioMimeType(req.audio)).body;
        }
        hash<auto> body = {"url": req.url};
        return doRestCommand("POST", path, body).body;
    }

    #! Returns the binary content of a file value
    /** The content is declared as base64 but may already have been decoded upstream, so both forms are
        accepted

        @param file the file value

        @return the content as binary

        @since DeepgramDataProvider 1.1
    */
    static binary getAudioContent(hash<auto> file) {
        if (file.content.typeCode() == NT_BINARY) {
            return file.content;
        }
        return parse_base64_string(file.content.toString());
    }

    #! Returns the MIME type to send as the \c Content-Type for a file value
    /** @param file the file value

        @return the MIME type carried by the file, or one inferred from its name, or
        \c application/octet-stream

        @note \c "auto" is the @ref DataProvider::FileDataType "FileDataType" placeholder meaning "work it
        out from the name", so it is treated as absent rather than sent literally

        @since DeepgramDataProvider 1.1
    */
    static string getAudioMimeType(hash<auto> file) {
        if (file.mime_type.val() && file.mime_type != "auto") {
            return file.mime_type;
        }
        return file.name.val() ? get_mime_type_from_ext(file.name) : MimeTypeOctetStream;
    }
}
}
