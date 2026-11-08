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

    }

    private {
        #! The request type, built on first use; see requestType()
        static *AzureDocumentIntelligenceAnalyzeRequestDataType request_type;
        #! The response type, built on first use; see responseType()
        static *AzureDocumentIntelligenceAnalyzeResponseDataType response_type;
    }
    private {
        #! The ID of the builder of the types; see initializeTypes()
        static int types_id = DataProviderActionCatalog::registerLazyValue(
            \AzureDocumentIntelligenceAnalyzeDocumentDataProvider::buildTypes());
    }

    #! Returns the request type, which is built on first use
    /** It is built from the vendored API description, so loading the module does not parse it
    */
    static AzureDocumentIntelligenceAnalyzeRequestDataType requestType() {
        initializeTypes();
        return request_type;
    }

    #! Returns the response type, which is built on first use
    /** It is built from the vendored API description, so loading the module does not parse it
    */
    static AzureDocumentIntelligenceAnalyzeResponseDataType responseType() {
        initializeTypes();
        return response_type;
    }

    #! Initializes complete helper types after the lightweight inventory is registered
    /** @throw AZURE-DOCUMENT-INTELLIGENCE-SCHEMA-ERROR an unreviewed schema resource is installed */
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
        AzureDocumentIntelligenceAnalyzeRequestDataType request();
        AzureDocumentIntelligenceAnalyzeResponseDataType response();
        return {"request": request, "response": response};
    }

    constructor(AzureDocumentIntelligenceRestClient::AzureDocumentIntelligenceRestClientIo rest)
            : AzureDocumentIntelligenceDataProviderCommon(rest) {}
    constructor(*hash<auto> options)
            : AzureDocumentIntelligenceDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        AzureDocumentIntelligenceReferences::validateAnalysisOptions(req);
        if (int(exists req.document_url) + int(exists req.document_bytes) + int(exists req.document_path) != 1) {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "supply exactly one document source";
        }
        hash<auto!> body;
        if (exists req.document_url) {
            hash<auto> url;
            try { url = parse_url(req.document_url); }
            # Convert parser failures below without echoing document URLs or SAS credentials.
            catch () { url = {}; }
            if (req.document_url !~ /^https?:\/\/[^\s]+$/ || !url.host.val()) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR",
                    "document_url must be a nonempty HTTP or HTTPS URL with a host";
            }
            body.urlSource = req.document_url;
        } else if (exists req.document_bytes) {
            body.base64Source = make_base64_string(req.document_bytes);
        } else {
            if (!req.document_path.val()) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "document_path must be a nonempty local file path";
            }
            body.base64Source =
                make_base64_string(File::readBinaryFile(req.document_path));
        }

        if (exists body.base64Source && !body.base64Source.val()) {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "document content cannot be empty";
        }

        hash<auto> opts;
        if (req.api_version.val())      { opts.api_version = req.api_version; }
        if (exists req.poll_interval)   { opts.poll_interval = req.poll_interval; }
        if (exists req.analysis_timeout) { opts.analysis_timeout = req.analysis_timeout; }
        opts += req{"pages", "locale", "stringIndexType", "features", "queryFields", "outputContentFormat", "output"};

        return rest.analyzeDocument(req.model_id, body, opts);
    }
}
}
