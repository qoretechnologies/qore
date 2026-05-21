# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDocumentAiProcessDocumentDataProvider class definition

/** GoogleDocumentAiProcessDocumentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace GoogleDocumentAiDataProvider {
public class GoogleDocumentAiProcessDocumentDataProvider inherits GoogleDocumentAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "process-document",
            "type": "GoogleDocumentAiProcessDocumentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static GoogleDocumentAiProcessDocumentRequestDataType RequestType =
            new GoogleDocumentAiProcessDocumentRequestDataType();
        static GoogleDocumentAiProcessDocumentResponseDataType ResponseType =
            new GoogleDocumentAiProcessDocumentResponseDataType();
    }

    constructor(GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo rest,
            string project_id, *string region, *string sa_json_path)
            : GoogleDocumentAiDataProviderCommon(rest, project_id, region, sa_json_path) {}
    constructor(*hash<auto> options) : GoogleDocumentAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("/v1/projects/%s/locations/%s/processors/%s:process",
            project_id, region, req.processor_id);
        hash<auto!> body;
        string mime = req.mime_type ?? "application/pdf";
        if (exists req.document_bytes) {
            body.rawDocument = {
                "content":  make_base64_string(req.document_bytes),
                "mimeType": mime,
            };
        } else if (req.document_path.val()) {
            body.rawDocument = {
                "content":  make_base64_string(File::readBinaryFile(req.document_path)),
                "mimeType": mime,
            };
        } else if (req.gcs_uri.val()) {
            body.gcsDocument = {
                "gcsUri":   req.gcs_uri,
                "mimeType": mime,
            };
        } else {
            throw "GOOGLE-DOCUMENTAI-CONFIG-ERROR",
                "one of document_bytes / document_path / gcs_uri is required";
        }
        if (req.process_options) {
            body.processOptions = req.process_options;
        }
        return doDocAiRequest(path, body);
    }
}
}
