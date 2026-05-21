# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureDocumentIntelligenceAnalyzeDocumentDataProvider class definition

/** AzureDocumentIntelligenceAnalyzeDocumentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AzureDocumentIntelligenceDataProvider {
public class AzureDocumentIntelligenceAnalyzeDocumentDataProvider
        inherits AzureDocumentIntelligenceDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "analyze-document",
            "type": "AzureDocumentIntelligenceAnalyzeDocumentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static AzureDocumentIntelligenceAnalyzeRequestDataType RequestType =
            new AzureDocumentIntelligenceAnalyzeRequestDataType();
        static AzureDocumentIntelligenceAnalyzeResponseDataType ResponseType =
            new AzureDocumentIntelligenceAnalyzeResponseDataType();
    }

    constructor(AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo rest)
            : AzureDocumentIntelligenceDataProviderCommon(rest) {}
    constructor(*hash<auto> options)
            : AzureDocumentIntelligenceDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto!> body;
        if (req.document_url.val()) {
            body.urlSource = req.document_url;
        } else if (exists req.document_bytes) {
            body.base64Source = make_base64_string(req.document_bytes);
        } else if (req.document_path.val()) {
            body.base64Source =
                make_base64_string(File::readBinaryFile(req.document_path));
        } else {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR",
                "one of document_url / document_bytes / document_path is required";
        }

        hash<auto> opts;
        if (req.api_version.val())      { opts.api_version = req.api_version; }
        if (exists req.poll_interval)   { opts.poll_interval = req.poll_interval; }
        if (exists req.analysis_timeout) { opts.analysis_timeout = req.analysis_timeout; }

        return rest.analyzeDocument(req.model_id, body, opts);
    }
}
}
