# -*- mode: qore; indent-tabs-mode: nil -*-
#! Reviewed Azure Document Intelligence action inventory and presentation
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace AzureDocumentIntelligenceDataProvider {
#! Product decisions over the pinned Microsoft contracts
/** JSON and stream uploads have separate contracts and action names. Five typed operation-result
    overloads share one wire endpoint and are exposed by get-operation with the complete discriminator.
    No synthetic overload selector is sent. Inventory construction never parses a schema or connects.
*/
public class AzureDocumentIntelligenceManifest {
    public {
        const Main = makeMain();
        const Streams = (
            action("start-analysis-stream", "POST", "/documentModels/{modelId}:analyze",
                "DocumentModels_AnalyzeDocumentFromStream", "Start Analysis from Bytes",
                "Submit document bytes and return the analysis job location"),
            action("start-classification-stream", "POST", "/documentClassifiers/{classifierId}:analyze",
                "DocumentClassifiers_ClassifyDocumentFromStream", "Start Classification from Bytes",
                "Submit document bytes and return the classification job location"),
        );
        const ReferenceData = {
            "models": <RestSchemaReferenceDataInfo>{"name": "models", "action": "list-models",
                "records_path": ("value",), "value_field": "modelId", "display_field": "modelId"},
            "classifiers": <RestSchemaReferenceDataInfo>{"name": "classifiers", "action": "list-classifiers",
                "records_path": ("value",), "value_field": "classifierId", "display_field": "classifierId"},
            "operations": <RestSchemaReferenceDataInfo>{"name": "operations", "action": "list-operations",
                "records_path": ("value",), "value_field": "operationId", "display_field": "operationId"},
        };
    }

    #! Returns exact identities without materializing action types
    /** @return the application, 29 raw actions, wait helper, and shared generic action */
    static list<hash<DataProviderDiscoveryExpectedIdentityInfo>> inventory() {
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> result = (
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName},
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": "analyze-document"},
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": "make-api-call"},
        );
        result += map <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": $1.action},
            Main + Streams;
        return result;
    }

    private static list<hash<RestSchemaActionInfo>> makeMain() {
        return (
            action("list-classifiers", "GET", "/documentClassifiers", "DocumentClassifiers_ListClassifiers",
                "List Classifiers", "List classifiers available in this Azure resource"),
            action("get-classifier", "GET", "/documentClassifiers/{classifierId}", "DocumentClassifiers_GetClassifier",
                "Get Classifier", "Read a classifier and its document categories"),
            action("delete-classifier", "DELETE", "/documentClassifiers/{classifierId}", "DocumentClassifiers_DeleteClassifier",
                "Delete Classifier", "Delete a custom document classifier"),
            action("start-classification", "POST", "/documentClassifiers/{classifierId}:analyze", "DocumentClassifiers_ClassifyDocument",
                "Start Classification", "Submit a document URL or base64 content for classification"),
            action("copy-classifier", "POST", "/documentClassifiers/{classifierId}:copyTo", "DocumentClassifiers_CopyClassifierTo",
                "Copy Classifier", "Start copying a classifier to an authorized resource"),
            action("get-classification-result", "GET", "/documentClassifiers/{classifierId}/analyzeResults/{resultId}",
                "DocumentClassifiers_GetClassifyResult", "Get Classification Result", "Read classification status and document results"),
            action("authorize-classifier-copy", "POST", "/documentClassifiers:authorizeCopy", "DocumentClassifiers_AuthorizeClassifierCopy",
                "Authorize Classifier Copy", "Authorize an incoming classifier copy in this resource"),
            action("build-classifier", "POST", "/documentClassifiers:build", "DocumentClassifiers_BuildClassifier",
                "Build Classifier", "Train a classifier from labeled documents in Azure storage"),
            action("list-models", "GET", "/documentModels", "DocumentModels_ListModels",
                "List Models", "List extraction models available in this Azure resource"),
            action("start-analysis", "POST", "/documentModels/{modelId}:analyze", "DocumentModels_AnalyzeDocument",
                "Start Analysis", "Submit a document URL or base64 content for extraction"),
            action("start-batch-analysis", "POST", "/documentModels/{modelId}:analyzeBatch", "DocumentModels_AnalyzeBatchDocuments",
                "Start Batch Analysis", "Analyze documents from an Azure storage container"),
            action("get-model", "GET", "/documentModels/{modelId}", "DocumentModels_GetModel",
                "Get Model", "Read an extraction model and its supported document fields"),
            action("delete-model", "DELETE", "/documentModels/{modelId}", "DocumentModels_DeleteModel",
                "Delete Model", "Delete a custom extraction model"),
            action("copy-model", "POST", "/documentModels/{modelId}:copyTo", "DocumentModels_CopyModelTo",
                "Copy Model", "Start copying a model to an authorized resource"),
            action("list-batch-results", "GET", "/documentModels/{modelId}/analyzeBatchResults", "DocumentModels_ListAnalyzeBatchResults",
                "List Batch Results", "List batch extraction jobs for a model"),
            action("get-batch-result", "GET", "/documentModels/{modelId}/analyzeBatchResults/{resultId}", "DocumentModels_GetAnalyzeBatchResult",
                "Get Batch Result", "Read batch progress and individual document outcomes"),
            action("delete-batch-result", "DELETE", "/documentModels/{modelId}/analyzeBatchResults/{resultId}", "DocumentModels_DeleteAnalyzeBatchResult",
                "Delete Batch Result", "Delete a stored batch extraction result"),
            action("get-analysis-result", "GET", "/documentModels/{modelId}/analyzeResults/{resultId}", "DocumentModels_GetAnalyzeResult",
                "Get Analysis Result", "Read analysis status and extracted document content"),
            action("delete-analysis-result", "DELETE", "/documentModels/{modelId}/analyzeResults/{resultId}", "DocumentModels_DeleteAnalyzeResult",
                "Delete Analysis Result", "Delete a stored extraction result"),
            action("get-result-figure", "GET", "/documentModels/{modelId}/analyzeResults/{resultId}/figures/{figureId}",
                "DocumentModels_GetAnalyzeResultFigure", "Download Figure", "Download the original PNG bytes of an extracted figure"),
            action("get-result-pdf", "GET", "/documentModels/{modelId}/analyzeResults/{resultId}/pdf", "DocumentModels_GetAnalyzeResultPdf",
                "Download Searchable PDF", "Download the searchable PDF bytes generated during analysis"),
            action("authorize-model-copy", "POST", "/documentModels:authorizeCopy", "DocumentModels_AuthorizeModelCopy",
                "Authorize Model Copy", "Authorize an incoming model copy in this resource"),
            action("build-model", "POST", "/documentModels:build", "DocumentModels_BuildModel",
                "Build Model", "Train a custom extraction model from labeled documents"),
            action("compose-model", "POST", "/documentModels:compose", "DocumentModels_ComposeModel",
                "Compose Model", "Combine trained models into one extraction workflow"),
            action("get-resource-info", "GET", "/info", "MiscellaneousOperations_GetResourceDetails",
                "Get Resource Information", "Read resource quotas and custom model usage"),
            action("list-operations", "GET", "/operations", "MiscellaneousOperations_ListOperations",
                "List Operations", "List recent model and classifier management operations"),
            action("get-operation", "GET", "/operations/{operationId}", "MiscellaneousOperations_GetOperation",
                "Get Operation", "Read build or copy progress and its typed result"),
        );
    }

    private static hash<RestSchemaActionInfo> action(string id, string method, string path, string operation_id,
            string label, string summary) {
        hash<string, hash<RestSchemaFieldOverlayInfo>> fields = {
            "api-version": <RestSchemaFieldOverlayInfo>{"name": "api_version", "display_name": "API Version",
                "default_value": DefaultApiVersion, "allowed_values": (
                    <AllowedValueInfo>{"value": DefaultApiVersion, "display_name": "2024-11-30"},)},
        };
        foreach hash<auto> identity in (({"wire": "modelId", "name": "model_id", "ref": "models", "label": "Model ID"},
                {"wire": "classifierId", "name": "classifier_id", "ref": "classifiers", "label": "Classifier ID"},
                {"wire": "operationId", "name": "operation_id", "ref": "operations", "label": "Operation ID"})) {
            if (path.find("{" + identity.wire + "}") != -1) {
                fields{identity.wire} = <RestSchemaFieldOverlayInfo>{"name": identity.name,
                    "display_name": identity.label, "ref_data": identity.ref, "supports_custom_values": True};
            }
        }
        if (path.find("{resultId}") != -1) {
            fields.resultId = <RestSchemaFieldOverlayInfo>{"name": "result_id", "display_name": "Result ID",
                "desc": "Result identifier from the job location returned when submitting the document."};
        }
        bool submit = path =~ /:analyze$/;
        if (submit || id == "start-batch-analysis") {
            fields.stringIndexType = choices("String Index Type", {
                "textElements": "Text Elements", "unicodeCodePoint": "Unicode Code Points",
                "utf16CodeUnit": "UTF-16 Code Units"});
        }
        if ((submit && path.find("documentModels") != -1) || id == "start-batch-analysis") {
            fields.features = choices("Analysis Features", {"ocrHighResolution": "High Resolution OCR",
                "languages": "Languages", "barcodes": "Barcodes", "formulas": "Formulas",
                "keyValuePairs": "Key/Value Pairs", "styleFont": "Font Styles", "queryFields": "Query Fields"}, True);
            fields.outputContentFormat = choices("Content Format", {"text": "Plain Text", "markdown": "Markdown"});
            fields.output = choices("Additional Outputs", {"pdf": "Searchable PDF", "figures": "Figures"}, True);
        }
        if ((submit && path.find("documentClassifiers") != -1) || id == "compose-model") {
            fields.split = choices("Document Splitting", {"auto": "Automatic", "none": "None", "perPage": "Per Page"});
        }
        if (id == "build-model") {
            fields.buildMode = choices("Build Mode", {"template": "Template", "neural": "Neural"});
        }
        if (submit && id !~ /-stream$/) {
            fields.urlSource = <RestSchemaFieldOverlayInfo>{"display_name": "Document URL", "preselected": True,
                "required_groups": ("document_source",), "exclusive_with": ("base64Source",), "sensitive": True};
            fields.base64Source = <RestSchemaFieldOverlayInfo>{"display_name": "Base64 Document", "preselected": True,
                "required_groups": ("document_source",), "exclusive_with": ("urlSource",), "sensitive": True};
        }
        if (id =~ /-stream$/) {
            fields.body = <RestSchemaFieldOverlayInfo>{"display_name": "Document Bytes", "sensitive": True,
                "desc": "Original document bytes, uploaded without base64 encoding."};
        }
        if (id == "build-model" || id == "start-batch-analysis") {
            foreach string field in ("azureBlobSource", "azureBlobFileListSource") {
                fields{field} = <RestSchemaFieldOverlayInfo>{"sensitive": True,
                    "required_groups": ("storage_source",),
                    "exclusive_with": (field == "azureBlobSource" ? "azureBlobFileListSource" : "azureBlobSource",),
                    "fields": {"containerUrl": <RestSchemaFieldOverlayInfo>{"sensitive": True}}};
            }
        }
        if (id == "start-batch-analysis") {
            fields.resultContainerUrl = <RestSchemaFieldOverlayInfo>{"sensitive": True};
        }
        if (id == "build-classifier") {
            fields.docTypes = <RestSchemaFieldOverlayInfo>{"sensitive": True};
            fields.baseClassifierId = <RestSchemaFieldOverlayInfo>{"ref_data": "classifiers", "supports_custom_values": True};
        }
        if (id == "compose-model") {
            fields.classifierId = <RestSchemaFieldOverlayInfo>{"ref_data": "classifiers", "supports_custom_values": True};
        }
        if (id == "copy-model" || id == "copy-classifier") {
            fields.accessToken = <RestSchemaFieldOverlayInfo>{"sensitive": True};
        }
        bool accepted = method == "POST" && path !~ /:authorizeCopy$/;
        bool binary_output = id == "get-result-pdf" || id == "get-result-figure";
        string description = summary + ".";
        if (accepted) {
            description += "\n\nReturns HTTP status and response headers. Save `Operation-Location` "
                "and use the corresponding result or operation action to check completion. "
                "**Submission does not wait for completion.**";
        } else if (method == "DELETE") {
            description += " **This permanently removes the selected resource or result.**";
        } else if (id =~ /^list-/) {
            description += " Returns one page; `nextLink` identifies the next page when present.";
        } else if (binary_output) {
            description += " Request the matching `output` when starting analysis. "
                "The response includes binary `body`, HTTP status, and media headers.";
        }
        return <RestSchemaActionInfo>{"action": id, "method": method, "path": path, "operation_id": operation_id,
            "display_name": label, "short_desc": summary, "desc": description, "fields": fields,
            "groups": (path.find("documentClassifiers") != -1 ? "Classification" : "Document Intelligence",),
            "response_envelope": accepted || method == "DELETE" || binary_output,
            "response_fields": id =~ /^authorize-/
                ? {"accessToken": <RestSchemaFieldOverlayInfo>{"sensitive": True}} : NOTHING,
            "response_content_type": binary_output ? (id == "get-result-pdf" ? "application/pdf" : "image/png") : NOTHING};
    }

    private static hash<RestSchemaFieldOverlayInfo> choices(string label, hash<string, string> labels,
            bool list_values = False) {
        list<hash<AllowedValueInfo>> values = map <AllowedValueInfo>{"value": $1.key,
            "display_name": $1.value}, labels.pairIterator();
        return <RestSchemaFieldOverlayInfo>{"display_name": label,
            "allowed_values": list_values ? NOTHING : values, "element_allowed_values": list_values ? values : NOTHING};
    }
}
}
