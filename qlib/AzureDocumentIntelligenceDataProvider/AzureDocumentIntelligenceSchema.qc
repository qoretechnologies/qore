# -*- mode: qore; indent-tabs-mode: nil -*-
#! Pinned Microsoft schema resources and lazy action sets
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

namespace Priv {
    # Read immutable module resources during initialization for sandbox compatibility.
    const AzureDocumentIntelligenceSchemaText = File::readTextFile(get_script_dir()
        + "/azure-document-intelligence-openapi.json");
    const AzureDocumentIntelligenceStreamsText = File::readTextFile(get_script_dir()
        + "/azure-document-intelligence-streams.json");
}

public namespace AzureDocumentIntelligenceDataProvider {
#! Shares validated schema objects without executing network or connection callbacks
/** tools/azure-document-intelligence-import.py defines normalization boundary version 1. Original
    Microsoft URLs, checksum, revision, and operation decisions are installed in the inventory resource.
    Schemas are parsed only after the module publishes its independent lightweight inventory.
*/
public class AzureDocumentIntelligenceSchema {
    private {
        static Mutex lck();
        static list<RestSchemaActionSet> sets;
    }

    #! Returns both complete action sets, constructing them once under the lock
    /** @return JSON and binary-request action sets
        @throw REST-SCHEMA-ACTION-ERROR a reviewed operation no longer matches its schema
    */
    static list<RestSchemaActionSet> getActionSets() {
        AutoLock al(lck);
        if (!sets) {
            foreach string text in (Priv::AzureDocumentIntelligenceSchemaText, Priv::AzureDocumentIntelligenceStreamsText) {
                hash<auto> provenance = parse_json(text)."x-qore-source";
                if (provenance.boundary_version != 1 || provenance.api_version != DefaultApiVersion
                        || provenance.source_sha256 != "3f750834da9e3acc162b781711bf0eda02ab2bd8a30c2b4c6a62c6910e7f1bd0") {
                    throw "AZURE-DOCUMENT-INTELLIGENCE-SCHEMA-ERROR", "unreviewed schema normalization boundary";
                }
            }
            OpenApi3::OpenApi3Schema main_schema = OpenApi3::OpenApi3Loader::fromString(
                Priv::AzureDocumentIntelligenceSchemaText, True);
            OpenApi3::OpenApi3Schema stream_schema = OpenApi3::OpenApi3Loader::fromString(
                Priv::AzureDocumentIntelligenceStreamsText, True);
            RestSchemaActionSet main(main_schema, AzureDocumentIntelligenceManifest::Main,
                AzureDocumentIntelligenceManifest::ReferenceData);
            RestSchemaActionSet streams(stream_schema, AzureDocumentIntelligenceManifest::Streams, NOTHING, (main,));
            sets = (main, streams);
        }
        return sets;
    }

    #! Returns the primary schema containing all official structured types
    /** @return the parsed Microsoft schema */
    static OpenApi3::OpenApi3Schema getSchema() { return getActionSets()[0].getSchema(); }

    #! Returns the set that owns a reviewed action
    /** @param name action identifier
        @return the owning set or nothing for an unknown action
    */
    static *RestSchemaActionSet findActionSet(string name) {
        foreach RestSchemaActionSet set in (getActionSets()) {
            if (set.getOperation(name)) { return set; }
        }
        return;
    }
}
}
