# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsTextractAnalyzeDocumentDataProvider class definition

/** AwsTextractAnalyzeDocumentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AwsTextractDataProvider {
public class AwsTextractAnalyzeDocumentDataProvider inherits AwsTextractDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "analyze-document",
            "type": "AwsTextractAnalyzeDocumentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static AwsTextractAnalyzeDocumentRequestDataType RequestType =
            new AwsTextractAnalyzeDocumentRequestDataType();
        static AwsTextractAnalyzeDocumentResponseDataType ResponseType =
            new AwsTextractAnalyzeDocumentResponseDataType();
    }

    constructor(AwsTextractRestClient::AwsTextractRestClientIo rest)
            : AwsTextractDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AwsTextractDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return AwsTextractSchema::getLegacyRequestType("analyze-document");
    }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AwsTextractSchema::getType("AnalyzeDocumentResponse");
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        # Validate original values before the common provider applies soft-type conversion.
        buildRequest(req, False);
        return getRequestTypeImpl();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doTextractAction("AnalyzeDocument", AwsTextractSchema::convert("AnalyzeDocumentRequest", buildRequest(req)));
    }

    private static hash<auto> buildRequest(hash<auto> req, bool read_file = True) {
        hash<auto!> body;
        body += AwsTextractDataProviderCommon::buildDocument(req,
            ("feature_types", "queries", "adapters_config", "human_loop_config"), read_file);
        body.FeatureTypes = req.feature_types;
        if (req.hasKey("queries")) {
            body.QueriesConfig = {"Queries": req.queries};
        }
        if (req.hasKey("adapters_config")) { body.AdaptersConfig = req.adapters_config; }
        if (req.hasKey("human_loop_config")) { body.HumanLoopConfig = req.human_loop_config; }
        AwsTextractSchema::validate("AnalyzeDocumentRequest", body);
        return body;
    }
}
}
