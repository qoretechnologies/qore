# -*- mode: qore; indent-tabs-mode: nil -*-
#! Schema actions and bounded resource-scoped reference data
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace AzureDocumentIntelligenceDataProvider {
#! Executes the common schema action contract with Azure resource references
public class AzureDocumentIntelligenceActionDataProvider inherits RestSchemaActionDataProvider {
    #! Creates an action over the connection's authenticated client
    /** @param actions the owning action set
        @param operation the declared action
        @param client the resource client
    */
    constructor(RestSchemaActionSet actions, RestSchemaActionOperation operation,
            AzureDocumentIntelligenceRestClientIo client)
            : RestSchemaActionDataProvider(actions, operation, client) {}

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        return AzureDocumentIntelligenceReferences::get(cast<AzureDocumentIntelligenceRestClientIo>(rest),
            type, action_opts);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        AzureDocumentIntelligenceReferences::validateAnalysisOptions(req);
        if (op.getAction() == "build-classifier" && req.docTypes.typeCode() == NT_HASH) {
            foreach auto document_type in (req.docTypes.iterator()) {
                if (int(exists document_type.azureBlobSource)
                        + int(exists document_type.azureBlobFileListSource) != 1) {
                    throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR",
                        "each classifier document type requires exactly one storage source";
                }
            }
        }
        if (op.getAction() =~ /-stream$/ && !req.body.val()) {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "document content cannot be empty";
        }
        if (op.getAction() == "start-analysis" || op.getAction() == "start-classification") {
            if (int(exists req.urlSource) + int(exists req.base64Source) != 1
                    || (exists req.urlSource && !req.urlSource.val())
                    || (exists req.base64Source && !req.base64Source.val())) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "supply exactly one nonempty document source";
            }
            if (exists req.urlSource && req.urlSource !~ /^https?:\/\/[^\s]+$/) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "the document URL must be an HTTP or HTTPS URL";
            }
        }
        return RestSchemaActionDataProvider::doRequestImpl(req, request_options);
    }
}

#! Reuses documented listing endpoints for account-scoped dropdowns
public class AzureDocumentIntelligenceReferences {
    #! Lists available models, classifiers, or operations with bounded pagination
    /** @param client the authenticated client for this resource
        @param kind models, classifiers, or operations
        @param options calling action options; API version is forwarded
        @return distinct identifiers and readable labels
        @throw AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR malformed or unbounded pagination
        @throw AZURE-DOCUMENT-INTELLIGENCE-URL-ERROR an unsafe next link
    */
    static *list<hash<AllowedValueInfo>> get(AzureDocumentIntelligenceRestClientIo client, string kind,
            *hash<auto> options) {
        hash<auto> collections = {"models": {"path": "documentModels", "id": "modelId"},
            "classifiers": {"path": "documentClassifiers", "id": "classifierId"},
            "operations": {"path": "operations", "id": "operationId"}};
        if (!collections{kind}) { return; }
        string path = "/documentintelligence/" + collections{kind}.path + "?api-version="
            + encode_url(options.api_version ?? DefaultApiVersion, True);
        hash<string, bool> seen_pages = {};
        hash<string, bool> seen_ids = {};
        list<hash<AllowedValueInfo>> result = ();
        for (int page = 0; page < 100; ++page) {
            if (seen_pages{path}) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR", "the service repeated a pagination link";
            }
            seen_pages{path} = True;
            hash<auto> response = client.restGet(path);
            if (response.body.typeCode() != NT_HASH || response.body.value.typeCode() != NT_LIST) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR", "the resource listing is malformed";
            }
            foreach auto item in (response.body.value) {
                if (item.typeCode() != NT_HASH || item{collections{kind}.id}.typeCode() != NT_STRING) {
                    throw "AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR", "a resource identifier is missing";
                }
                string id = item{collections{kind}.id};
                if (seen_ids{id}) { continue; }
                seen_ids{id} = True;
                push result, <AllowedValueInfo>{"value": id,
                    "display_name": item.description.val() ? id + " — " + item.description : id};
            }
            if (!exists response.body.nextLink || response.body.nextLink == "") { return result; }
            if (response.body.nextLink.typeCode() != NT_STRING) {
                throw "AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR", "the pagination link is malformed";
            }
            path = AzureDocumentIntelligenceRestClientBase::servicePath(client.getUrl(), response.body.nextLink);
        }
        throw "AZURE-DOCUMENT-INTELLIGENCE-REFERENCE-ERROR", "reference data exceeds the 100-page limit";
    }

    #! Rejects documented incompatible analysis options before uploading a document
    /** @param options the action request
        @throw AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR an incompatible feature or output selection
    */
    static validateAnalysisOptions(hash<auto> options) {
        if ((options.api_version ?? DefaultApiVersion) == DefaultApiVersion
                && inlist(options.model_id, ("prebuilt-document", "prebuilt-businessCard"))) {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR",
                "this legacy model is retired in API v4; use prebuilt-layout or an available custom model";
        }
        if (options.output && inlist("pdf", options.output) && options.model_id != "prebuilt-read") {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "searchable PDF output requires prebuilt-read";
        }
        if (options.output && inlist("figures", options.output) && options.model_id != "prebuilt-layout") {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "figure output requires prebuilt-layout";
        }
        if (options.queryFields && (!options.features || !inlist("queryFields", options.features))) {
            throw "AZURE-DOCUMENT-INTELLIGENCE-CONFIG-ERROR", "queryFields requires the queryFields feature";
        }
    }
}
}
