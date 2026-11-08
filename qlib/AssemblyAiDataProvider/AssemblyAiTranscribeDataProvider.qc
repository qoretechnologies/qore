# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiTranscribeDataProvider class definition

/** AssemblyAiTranscribeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
public class AssemblyAiTranscribeDataProvider inherits AssemblyAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "transcribe",
            "type": "AssemblyAiTranscribeDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The request type, built on first use; see requestType()
        static *AssemblyAiTranscribeRequestDataType request_type;
        #! The response type, built on first use; see responseType()
        static *AssemblyAiTranscribeResponseDataType response_type;
        #! The ID of the builder of the types; see initializeTypes()
        static int types_id = DataProviderActionCatalog::registerLazyValue(
            \AssemblyAiTranscribeDataProvider::buildTypes());
    }

    #! Returns the request type, which is built on first use
    /** It is built from the vendored API description, so loading the module does not parse it
    */
    static AssemblyAiTranscribeRequestDataType requestType() {
        initializeTypes();
        return request_type;
    }

    #! Returns the response type, which is built on first use
    /** It is built from the vendored API description, so loading the module does not parse it
    */
    static AssemblyAiTranscribeResponseDataType responseType() {
        initializeTypes();
        return response_type;
    }

    #! Sets the types the first time they are used
    private static initializeTypes() {
        if (!request_type) {
            hash<auto> types = DataProviderActionCatalog::getLazyValue(types_id);
            # the request type is set last: it tells that both are set
            response_type = types.response;
            request_type = types.request;
        }
    }

    #! Builds the types; see initializeTypes()
    private static hash<auto> buildTypes() {
        AssemblyAiTranscribeRequestDataType request();
        AssemblyAiTranscribeResponseDataType response();
        return {"request": request, "response": response};
    }

    constructor(AssemblyAiRestClient::AssemblyAiRestClientIo rest)
            : AssemblyAiDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AssemblyAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string url = req.audio_url;
        hash<auto> opts = req - "audio_url";
        return rest.transcribe(url, opts);
    }
}
}
