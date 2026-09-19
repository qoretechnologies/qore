# -*- mode: qore; indent-tabs-mode: nil -*-
#! Pinned Discovery contracts and independent lightweight action inventory
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

namespace Priv {
    # Immutable installed resources are read during module initialization, before a sandbox is entered.
    const GoogleDocumentAiManifest = parse_json(File::readTextFile(get_script_dir()
        + "/google-document-ai-manifest.json"));
    const GoogleDocumentAiStable = File::readTextFile(get_script_dir() + "/google-document-ai-openapi-v1.json");
    const GoogleDocumentAiBeta = File::readTextFile(get_script_dir() + "/google-document-ai-openapi-v1beta3.json");
}

public namespace GoogleDocumentAiDataProvider {
#! Publishes reviewed identities without invoking schema, action, or connection code
public class GoogleDocumentAiManifest {
    #! Returns the complete expected application and action inventory
    /** @return application, reviewed stable/beta actions, helpers, and the shared generic action */
    static list<hash<DataProviderDiscoveryExpectedIdentityInfo>> inventory() {
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> result = (
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName},
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": "process-document"},
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": "wait-for-operation"},
            <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": "make-api-call"},
        );
        foreach string version in ("v1", "v1beta3") {
            foreach hash<auto> entry in (Priv::GoogleDocumentAiManifest{version}) {
                if (!entry.metadata.internal) {
                    push result, <DataProviderDiscoveryExpectedIdentityInfo>{"app": AppName, "action": entry.action};
                }
            }
        }
        return result;
    }

    #! Returns the reference declarations checked by the shared action set
    /** @return resource dropdown contracts; actual execution uses bounded, scoped pagination */
    static hash<string, hash<RestSchemaReferenceDataInfo>> references() {
        return {
            "processors": <RestSchemaReferenceDataInfo>{"name": "processors", "action": "list-processors", "records_path": ("processors",),
                "value_field": "name", "display_field": "displayName"},
            "processor-types": <RestSchemaReferenceDataInfo>{"name": "processor-types", "action": "list-processor-types",
                "records_path": ("processorTypes",), "value_field": "type", "display_field": "type"},
            "processor-versions": <RestSchemaReferenceDataInfo>{"name": "processor-versions", "action": "list-processor-versions",
                "records_path": ("processorVersions",), "value_field": "name", "display_field": "displayName",
                "options_from": {"processor_id": "processor_id"}},
            "evaluations": <RestSchemaReferenceDataInfo>{"name": "evaluations", "action": "list-evaluations", "records_path": ("evaluations",),
                "value_field": "name", "options_from": {"processor_id": "processor_id",
                    "processor_version_id": "processor_version_id"}},
            "schemas": <RestSchemaReferenceDataInfo>{"name": "schemas", "action": "list-schemas", "records_path": ("schemas",),
                "value_field": "name", "display_field": "displayName"},
            "schema-versions": <RestSchemaReferenceDataInfo>{"name": "schema-versions", "action": "list-schema-versions",
                "records_path": ("schemaVersions",), "value_field": "name", "display_field": "displayName",
                "options_from": {"schema_id": "schema_id"}},
        };
    }
}

#! Loads Qore-normalized OpenAPI contracts from the pinned Google Discovery snapshots
/** tools/google-document-ai-import.py records every source method and decision. Google does not publish
    these generated OpenAPI documents. The shared OpenApi3 and RestSchemaActions modules own type handling.
*/
public class GoogleDocumentAiSchema {
    private {
        static Mutex lck();
        static list<RestSchemaActionSet> sets;
    }

    #! Returns stable and explicitly beta action sets
    /** @return the two action sets
        @throw GOOGLE-DOCUMENTAI-SCHEMA-ERROR an unreviewed resource is loaded
    */
    static list<RestSchemaActionSet> getActionSets() {
        AutoLock al(lck);
        if (!sets) {
            list<RestSchemaActionSet> built = ();
            foreach hash<auto> item in (({"version": "v1", "text": Priv::GoogleDocumentAiStable,
                    "sha256": "7e18521900f94b8719c4ad366e10707b4a1f8a019d5aac156fa62fd7c6963b03"},
                    {"version": "v1beta3", "text": Priv::GoogleDocumentAiBeta,
                    "sha256": "89699fd011959f24db9c2f816ccbe51d5986fe1b62304f81052fb013a05403c8"})) {
                hash<auto> provenance = parse_json(item.text)."x-qore-google-discovery";
                if (provenance.boundary_version != 1 || provenance.revision != "20260915"
                        || provenance.source_sha256 != item.sha256) {
                    throw "GOOGLE-DOCUMENTAI-SCHEMA-ERROR", "unreviewed Discovery normalization boundary";
                }
                OpenApi3::OpenApi3Schema schema = OpenApi3::OpenApi3Loader::fromString(item.text, True);
                list<hash<RestSchemaActionInfo>> manifest = Priv::GoogleDocumentAiManifest{item.version};
                push built, new RestSchemaActionSet(schema, manifest,
                    item.version == "v1" ? GoogleDocumentAiManifest::references() : NOTHING, built);
            }
            sets = built;
        }
        return sets;
    }

    #! Returns the stable schema, including complete document and operation types
    /** @return the stable schema */
    static OpenApi3::OpenApi3Schema getSchema() { return getActionSets()[0].getSchema(); }

    #! Returns the owning action set
    /** @param action reviewed action identifier
        @return the owning set, or nothing when unknown
    */
    static *RestSchemaActionSet findActionSet(string action) {
        foreach RestSchemaActionSet set in (getActionSets()) {
            if (set.getOperation(action)) { return set; }
        }
    }

    #! Returns a request type with connection-owned scope removed
    /** @param op native operation
        @return the public request contract
    */
    static HashDataType requestType(RestSchemaActionOperation op) {
        HashDataType type = cast<HashDataType>(op.getRequestType());
        hash<string, AbstractDataField> fields = type.getFields() - ("project_id", "location");
        if (op.getMethod().upr() == "PATCH") { fields -= "name"; }
        return type.copyWithFields(fields);
    }
}
}
