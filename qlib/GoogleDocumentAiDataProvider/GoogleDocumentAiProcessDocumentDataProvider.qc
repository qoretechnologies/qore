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

        static GoogleDocumentAiProcessDocumentRequestDataType RequestType;
        static GoogleDocumentAiProcessDocumentResponseDataType ResponseType;
    }

    private { static Mutex type_lock(); }

    #! Initializes schema-backed helper types after the independent inventory is published
    static initializeTypes() {
        AutoLock al(type_lock);
        if (!RequestType) {
            GoogleDocumentAiProcessDocumentRequestDataType request();
            GoogleDocumentAiProcessDocumentResponseDataType response();
            RequestType = request;
            ResponseType = response;
        }
    }

    constructor(GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo rest,
            string project_id, *string region, *string sa_json_path)
            : GoogleDocumentAiDataProviderCommon(rest, project_id, region, sa_json_path) {}
    constructor(*hash<auto> options) : GoogleDocumentAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Validates source presence and fundamental types before optional-field coercion
    /** @param req document request
        @param request_options transport request options
        @return the typed process response
        @throw GOOGLE-DOCUMENTAI-REQUEST-ERROR invalid source combination or content
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() == NT_HASH) {
            int sources = 0;
            foreach hash<auto> source in (({"name": "document_bytes", "type": NT_BINARY},
                    {"name": "document_path", "type": NT_STRING}, {"name": "gcs_uri", "type": NT_STRING},
                    {"name": "inline_document", "type": NT_HASH})) {
                if (!req.hasKey(source.name)) { continue; }
                ++sources;
                if (req{source.name}.typeCode() != source.type || !req{source.name}.val()) {
                    throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "document sources must have the declared type and cannot be empty";
                }
            }
            if (sources != 1) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "supply exactly one document source";
            }
            foreach string field in ("process_options", "labels") {
                if (req.hasKey(field) && req{field}.typeCode() != NT_HASH) {
                    throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "process options and labels must be objects when supplied";
                }
            }
            if (req.hasKey("mime_type") && (req.mime_type.typeCode() != NT_STRING || !req.mime_type.val())) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "mime_type must be a nonempty string";
            }
            if (req.hasKey("imageless_mode") && req.imageless_mode.typeCode() != NT_BOOLEAN) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "imageless_mode must be a boolean";
            }
        }
        return AbstractDataProvider::doRequest(req, request_options);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto!> body = req{"processor_id", "processor_version_id"};
        string mime = req.mime_type ?? "application/pdf";
        if (req.hasKey("document_bytes")) {
            body.rawDocument = {
                "content":  make_base64_string(req.document_bytes),
                "mimeType": mime,
            };
        } else if (req.hasKey("document_path")) {
            binary content = File::readBinaryFile(req.document_path);
            if (!content.size()) { throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "document content cannot be empty"; }
            body.rawDocument = {
                "content":  make_base64_string(content),
                "mimeType": mime,
            };
        } else if (req.hasKey("gcs_uri")) {
            GoogleDocumentAiResources::validateGcs(req.gcs_uri);
            body.gcsDocument = {
                "gcsUri":   req.gcs_uri,
                "mimeType": mime,
            };
        } else {
            body.inlineDocument = req.inline_document;
        }
        foreach hash<auto> field in (({"from": "process_options", "to": "processOptions"},
                {"from": "field_mask", "to": "fieldMask"}, {"from": "imageless_mode", "to": "imagelessMode"},
                {"from": "labels", "to": "labels"})) {
            if (req.hasKey(field.from)) { body{field.to} = req{field.from}; }
        }
        RestSchemaActionSet set = GoogleDocumentAiSchema::getActionSets()[0];
        GoogleDocumentAiActionDataProvider action(set,
            set.getOperationEx(req.hasKey("processor_version_id") ? "_process-version" : "_process"),
            rest, project_id, region);
        return action.doRequest(body, request_options);
    }

    *hash<string, bool> getSupportedReferenceData() { return {"processors": True, "processor-versions": True}; }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        return GoogleDocumentAiResources::references(rest, project_id, region, type, action_opts);
    }
}
}
