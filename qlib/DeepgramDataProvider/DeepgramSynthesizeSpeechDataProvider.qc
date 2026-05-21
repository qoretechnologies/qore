# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepgramSynthesizeSpeechDataProvider class definition

/** DeepgramSynthesizeSpeechDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
public class DeepgramSynthesizeSpeechDataProvider inherits DeepgramDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "synthesize-speech",
            "type": "DeepgramSynthesizeSpeechDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static DeepgramSynthesizeSpeechRequestDataType RequestType =
            new DeepgramSynthesizeSpeechRequestDataType();
        static DeepgramSynthesizeSpeechResponseDataType ResponseType =
            new DeepgramSynthesizeSpeechResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : DeepgramDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : DeepgramDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Text goes in JSON body; all other parameters go in query string
        hash<auto> query_params = req - "text";
        string path = "/v1/speak"
            + DeepgramDataProviderCommon::buildQueryString(query_params);
        hash<auto> body = {"text": req.text};
        hash<auto> resp = doRestCommand("POST", path, body);
        return {
            "audio": resp.body,
            "content_type": resp.headers."content-type" ?? "audio/mpeg",
            "request_id": resp.headers."dg-request-id",
        };
    }
}
}
