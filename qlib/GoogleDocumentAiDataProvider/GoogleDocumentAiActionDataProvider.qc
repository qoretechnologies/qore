# -*- mode: qore; indent-tabs-mode: nil -*-
#! Scoped schema actions, resource identities, and paginated dropdowns
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace GoogleDocumentAiDataProvider {
#! Executes a typed operation in the connection's project and location
public class GoogleDocumentAiActionDataProvider inherits RestSchemaActionDataProvider {
    private {
        string project;
        string location;
        HashDataType request_type;
    }

    #! Creates an immutable action instance
    /** @param actions owning action set
        @param operation native contract
        @param client authenticated client
        @param project_id resource project
        @param region resource location
    */
    constructor(RestSchemaActionSet actions, RestSchemaActionOperation operation,
            GoogleDocumentAiRestClientIo client, string project_id, string region)
            : RestSchemaActionDataProvider(actions, operation, client) {
        project = project_id;
        location = region;
        request_type = GoogleDocumentAiSchema::requestType(operation);
    }

    private *AbstractDataProviderType getRequestTypeImpl() { return request_type; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        GoogleDocumentAiRestClientIo client = cast<GoogleDocumentAiRestClientIo>(rest);
        req = GoogleDocumentAiResources::normalize(req, project, location, client);
        string base = GoogleDocumentAiResources::prefix(project, location);
        if (op.getAction() == "set-default-version") {
            req.defaultProcessorVersion = base + "/processors/" + req.processor_id + "/processorVersions/"
                + GoogleDocumentAiResources::id(req.defaultProcessorVersion,
                    base + "/processors/" + req.processor_id + "/processorVersions/", client);
        }
        if (op.getAction() == "update-schema" || op.getAction() == "update-schema-version") {
            string name = base + "/schemas/" + req.schema_id;
            if (op.getAction() == "update-schema-version") { name += "/schemaVersions/" + req.schema_version_id; }
            if (req.hasKey("name") && req.name != name) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "the body resource name must match the selected resource";
            }
            req.name = name;
        }
        if (op.getAction() == "beta-update-dataset" || op.getAction() == "beta-update-dataset-schema") {
            string name = base + "/processors/" + req.processor_id + "/dataset";
            if (op.getAction() == "beta-update-dataset-schema") { name += "/datasetSchema"; }
            if (req.hasKey("name") && req.name != name) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "the dataset name must match the selected processor";
            }
            req.name = name;
            if (req.hasKey("documentWarehouseConfig")) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "Document AI Warehouse dataset configuration is no longer supported";
            }
            if (req.hasKey("gcsManagedConfig") && req.hasKey("unmanagedDatasetConfig")) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "select one dataset storage configuration";
            }
        }
        if (op.getAction() =~ /^start-.*batch-processing$/) {
            GoogleDocumentAiResources::validateBatch(req);
        }
        if (op.getAction() == "list-operations" && !req.filter.val()) {
            throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "an operation Type filter is required";
        }
        # The public type cannot accept or override these connection-owned fields.
        hash<string, AbstractDataField> native = op.getRequestFields();
        if (native.project_id) { req.project_id = project; }
        if (native.location) { req.location = location; }
        return RestSchemaActionDataProvider::doRequestImpl(req, request_options);
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        return GoogleDocumentAiResources::references(cast<GoogleDocumentAiRestClientIo>(rest),
            project, location, type, action_opts);
    }
}

#! Handles Document AI resource names without reserved URI expansion or cross-project lookup
public class GoogleDocumentAiResources {
    #! Returns a validated project/location resource prefix
    /** @param project resource project ID or number
        @param region regional location ID
        @return a resource prefix without API version or leading slash
    */
    static string prefix(string project, string region) {
        if (project !~ /^[a-zA-Z0-9][a-zA-Z0-9._:-]*$/ || region !~ /^[a-z][a-z0-9-]*$/) {
            throw "GOOGLE-DOCUMENTAI-CONFIG-ERROR", "invalid project or location";
        }
        return "projects/" + project + "/locations/" + region;
    }

    #! Accepts an ID or a matching full resource name
    /** @param value caller's identifier
        @param prefix required scope and collection, ending in a slash
        @param client optional authenticated client for project ID/number equivalence
        @param timeout_ms optional remaining lookup deadline in milliseconds
        @return an unescaped single ID; the shared transport escapes it once
        @throw GOOGLE-DOCUMENTAI-REQUEST-ERROR invalid or differently scoped identity
    */
    static string id(auto value, string prefix, *GoogleDocumentAiRestClientIo client, *int timeout_ms) {
        if (value.typeCode() != NT_STRING) {
            throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "resource identifiers must be strings";
        }
        string result = value;
        if (result.find("/") >= 0) {
            if (result.substr(0, prefix.size()) != prefix && client && result =~ /^projects\/[0-9]+\//) {
                *list<string> parts = (prefix =~ x/^projects\/([^\/]+)\/locations\/([^\/]+)\//);
                if (parts && parts[0] !~ /^[0-9]+$/) {
                    # Only the authenticated location endpoint can establish an alias.
                    # Numeric input alone is never evidence of project membership.
                    string canonical = client.getCanonicalProject(parts[0], parts[1], timeout_ms);
                    prefix = "projects/" + canonical + prefix.substr(9 + parts[0].size());
                }
            }
            if (result.substr(0, prefix.size()) != prefix) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "resource name does not belong to the selected scope";
            }
            result = result.substr(prefix.size());
        }
        if (result !~ /^[a-zA-Z0-9][a-zA-Z0-9._~:-]*$/) {
            throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "resource ID must be a nonempty unescaped path segment";
        }
        return result;
    }

    #! Normalizes dependent resource identities in parent order
    /** @param request action fields
        @param project connection project
        @param region connection location
        @param client optional authenticated client for canonical resource names
        @return a copy containing only segment IDs
    */
    static hash<auto> normalize(hash<auto> request, string project, string region,
            *GoogleDocumentAiRestClientIo client) {
        string base = prefix(project, region);
        foreach hash<auto> field in (({"key": "processor_id", "prefix": base + "/processors/"},
                {"key": "processor_type", "prefix": base + "/processorTypes/"},
                {"key": "operation_id", "prefix": base + "/operations/"},
                {"key": "schema_id", "prefix": base + "/schemas/"})) {
            if (request.hasKey(field.key)) { request{field.key} = id(request{field.key}, field.prefix, client); }
        }
        if (request.hasKey("processor_version_id")) {
            request.processor_version_id = id(request.processor_version_id,
                base + "/processors/" + request.processor_id + "/processorVersions/", client);
        }
        if (request.hasKey("evaluation_id")) {
            request.evaluation_id = id(request.evaluation_id, base + "/processors/" + request.processor_id
                + "/processorVersions/" + request.processor_version_id + "/evaluations/", client);
        }
        if (request.hasKey("schema_version_id")) {
            request.schema_version_id = id(request.schema_version_id,
                base + "/schemas/" + request.schema_id + "/schemaVersions/", client);
        }
        return request;
    }

    #! Validates storage source alternatives before transport
    /** @param request batch request
        @throw GOOGLE-DOCUMENTAI-REQUEST-ERROR missing, empty, or conflicting source/output
    */
    static validateBatch(hash<auto> request) {
        auto input = request.inputDocuments;
        if (input.typeCode() != NT_HASH || int(input.hasKey("gcsPrefix")) + int(input.hasKey("gcsDocuments")) != 1) {
            throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "batch input requires exactly one GCS prefix or document list";
        }
        if (input.hasKey("gcsPrefix")) {
            validateGcs(input.gcsPrefix.gcsUriPrefix);
        } else {
            if (input.gcsDocuments.documents.typeCode() != NT_LIST || !input.gcsDocuments.documents.size()) {
                throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "the batch document list cannot be empty";
            }
            foreach auto document in (input.gcsDocuments.documents) { validateGcs(document.gcsUri); }
        }
        validateGcs(request.documentOutputConfig.gcsOutputConfig.gcsUri);
    }

    #! Checks a Google Cloud Storage URI without resolving or enumerating its objects
    /** @param uri storage URI
        @throw GOOGLE-DOCUMENTAI-REQUEST-ERROR invalid URI
    */
    static validateGcs(auto uri) {
        if (uri.typeCode() != NT_STRING || uri !~ /^gs:\/\/[^\s\/?#]+\/[^\r\n]*$/) {
            throw "GOOGLE-DOCUMENTAI-REQUEST-ERROR", "a gs://bucket/path storage URI is required";
        }
    }

    #! Lists resources in the current scope, with bounded pagination and duplicate removal
    /** @param client authenticated client
        @param project resource project
        @param region resource location
        @param kind declared reference name
        @param options parent action options
        @return distinct value/label records; nothing when a required parent is not selected
        @throw GOOGLE-DOCUMENTAI-REFERENCE-ERROR malformed or unbounded pagination
    */
    static *list<hash<AllowedValueInfo>> references(GoogleDocumentAiRestClientIo client, string project,
            string region, string kind, *hash<auto> options) {
        *hash<RestSchemaReferenceDataInfo> declaration = GoogleDocumentAiManifest::references(){kind};
        if (!declaration) { return; }
        hash<auto!> request = {"pageSize": 100};
        foreach hash<auto> item in (declaration.options_from.pairIterator()) {
            if (!options{item.value}.val()) { return; }
            request{item.key} = options{item.value};
        }
        RestSchemaActionSet set = GoogleDocumentAiSchema::getActionSets()[0];
        GoogleDocumentAiActionDataProvider lister(set, set.getOperationEx(declaration.action), client, project, region);
        hash<auto> parents = normalize(request, project, region, client);
        string resource_prefix = prefix(project, region);
        if (kind == "processor-versions" || kind == "evaluations") {
            resource_prefix += "/processors/" + parents.processor_id;
        }
        if (kind == "evaluations") { resource_prefix += "/processorVersions/" + parents.processor_version_id; }
        if (kind == "schema-versions") { resource_prefix += "/schemas/" + parents.schema_id; }
        resource_prefix += "/" + declaration.records_path[0] + "/";
        hash<string, bool> pages = {};
        hash<string, bool> ids = {};
        list<hash<AllowedValueInfo>> result = ();
        for (int page = 0; page < 100; ++page) {
            auto body = lister.doRequest(request);
            string collection = declaration.records_path[0];
            if (body.typeCode() != NT_HASH || (body.hasKey(collection) && body{collection}.typeCode() != NT_LIST)
                    || (body.hasKey("nextPageToken") && body.nextPageToken.typeCode() != NT_STRING)) {
                throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "malformed resource listing";
            }
            foreach auto record in (body{collection}) {
                if (record.typeCode() != NT_HASH || record{declaration.value_field}.typeCode() != NT_STRING
                        || !record{declaration.value_field}.val()) {
                    throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "resource listing contains an invalid identifier";
                }
                string value = record{declaration.value_field};
                if (kind != "processor-types" && value.find("/") < 0) {
                    throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "a listed resource belongs to another scope";
                }
                id(value, resource_prefix, client);
                if (ids{value}) { continue; }
                ids{value} = True;
                string label = record{declaration.display_field}.val() ? record{declaration.display_field} : value;
                push result, <AllowedValueInfo>{"value": value, "display_name": label};
                if (result.size() > 10000) {
                    throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "reference data exceeds the 10000-resource limit";
                }
            }
            if (!body.nextPageToken.val()) { return result; }
            if (pages{body.nextPageToken}) {
                throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "the service repeated a pagination token";
            }
            pages{body.nextPageToken} = True;
            request.pageToken = body.nextPageToken;
        }
        throw "GOOGLE-DOCUMENTAI-REFERENCE-ERROR", "reference data exceeds the 100-page limit";
    }
}
}
