# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PineconeDataProvider module: the pinned Pinecone API descriptions

/** PineconeSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Private definitions for the PineconeDataProvider module
namespace Priv {
#! The directory this module was loaded from
/** Captured in a constant initializer, which is evaluated while this file is parsed, so that it names the
    module's own directory rather than whatever script happens to be calling later.
*/
const PineconeModuleDir = get_script_dir();
}

public namespace PineconeDataProvider {
#! Records where a vendored Pinecone API description came from and what it describes
/** The actions are generated from descriptions committed to this module rather than fetched at run time,
    because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating one is a reviewed step: import a named revision, check it against the
    manifest, and look at what changed.

    @see @ref PineconeDataProvider::PineconeSchema "PineconeSchema"
*/
public hashdecl PineconeSchemaProvenanceInfo {
    #! The URL the description was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The date the description was imported
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of operations the upstream document declares
    int source_operations;

    #! The number of component schemas the upstream document declares
    int source_schemas;

    #! The number of webhooks the upstream document declares
    int source_webhooks;
}

#! Loads and holds the vendored Pinecone API descriptions
/** Pinecone publishes one description per API rather than one per product, and this application uses
    four of them across two applications.  A @ref RestSchemaActions::RestSchemaActionSet "RestSchemaActionSet" binds one schema,
    because a request's URI prefix comes from that schema's base path, so each document becomes its own
    set.

    @section pineconeschema_order Build Order

    The sets are built in dependency order, and a cycle therefore cannot be expressed:

    |!Set|!Peers|!Why
    |admin|none|separate organization credentials and application
    |inference|none|its model listing depends on nothing
    |db_control|inference|the integrated-embedding index action offers a model dropdown
    |db_data|db_control, inference|the index dropdown and the host source come from the control plane; the reranking model from inference

    The control-plane set also declares the @ref PineconeDataProvider::PineconeManifest::HostSources
    "host source" that routes every data-plane request, because it exports the lookup that resolves it.

    @section pineconeschema_version All Four Move Together

    Pinecone versions all of its APIs with one dated version, so the four documents are imported at the
    same version and updated together;
    @ref PineconeRestClient::PineconeRestClientBase::ApiVersion "PineconeRestClientBase::ApiVersion" is
    the version they describe and the version the client sends, and this module's tests assert that the
    two agree.  Re-import all four when it moves, not one.

    Each document is parsed once, on first use, rather than when the module loads, so that a program that
    only holds the connection never pays for it.

    @since %PineconeDataProvider 1.0
*/
public class PineconeSchema {
    public {
        #! Where the vendored control-plane description came from
        const ControlProvenance = <PineconeSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/pinecone-io/pinecone-api/70a7509d34958cde7154efa3b76372bd5dacce8e/2026-07/db_control_2026-07.oas.yaml",
            "source_sha256": "19dd02de757e651d881bb69962b0bf83fe995b842de7a49c3629cc3c20b19107",
            "source_size": 193462,
            "imported": 2026-09-17,
            "source_paths": 14,
            "source_operations": 24,
            "source_schemas": 64,
            "source_webhooks": 0,
        };

        #! Where the vendored data-plane description came from
        const DataProvenance = <PineconeSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/pinecone-io/pinecone-api/70a7509d34958cde7154efa3b76372bd5dacce8e/2026-07/db_data_2026-07.oas.yaml",
            "source_sha256": "de555d09663b998350701196302f86d80119ba37e39de3eda889ee406d01dcc1",
            "source_size": 132787,
            "imported": 2026-09-17,
            "source_paths": 20,
            "source_operations": 24,
            "source_schemas": 68,
            "source_webhooks": 0,
        };

        #! Where the vendored inference description came from
        const InferenceProvenance = <PineconeSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/pinecone-io/pinecone-api/70a7509d34958cde7154efa3b76372bd5dacce8e/2026-07/inference_2026-07.oas.yaml",
            "source_sha256": "1e6958c4f554e521d1551bac01029e5902b02533d4663ebe09a279d04a0e3e59",
            "source_size": 31145,
            "imported": 2026-09-17,
            "source_paths": 4,
            "source_operations": 4,
            "source_schemas": 16,
            "source_webhooks": 0,
        };

        #! Provenance of the separate OAuth2 Admin API
        const AdminProvenance = <PineconeSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/pinecone-io/pinecone-api/70a7509d34958cde7154efa3b76372bd5dacce8e/2026-07/admin_2026-07.oas.yaml",
            "source_sha256": "4b0860571ffb538ae58205ec97f28d76ae8d020af0cce8ac018469e1bc3291a2",
            "source_size": 140279,
            "imported": 2026-09-17,
            "source_paths": 16,
            "source_operations": 32,
            "source_schemas": 27,
            "source_webhooks": 0,
        };

        #! Vendored organization administration schema
        const AdminSchemaFileName = "pinecone-admin.yaml";

        #! The name of the vendored control-plane description, which lives beside this module
        const ControlSchemaFileName = "pinecone-db-control.yaml";

        #! The name of the vendored data-plane description, which lives beside this module
        const DataSchemaFileName = "pinecone-db-data.yaml";

        #! The name of the vendored inference description, which lives beside this module
        const InferenceSchemaFileName = "pinecone-inference.yaml";
    }

    private {
        #! The parsed schemas, keyed by file name
        static hash<string, OpenApi3::OpenApi3Schema> schemas = {};

        #! The action set built from the control-plane description and its manifest
        static *RestSchemaActions::RestSchemaActionSet control_set;

        #! Separate Admin action set, using service-account authentication
        static *RestSchemaActions::RestSchemaActionSet admin_set;

        #! The action set built from the data-plane description and its manifest
        static *RestSchemaActions::RestSchemaActionSet data_set;

        #! The action set built from the inference description and its manifest
        static *RestSchemaActions::RestSchemaActionSet inference_set;

        #! Serializes construction of the schemas and the action sets
        static Mutex lck();
    }

    #! Returns the parsed vendored control-plane description, parsing it on first use
    static OpenApi3::OpenApi3Schema getControlSchema() {
        AutoLock al(lck);
        return PineconeSchema::getSchemaIntern(ControlSchemaFileName);
    }

    #! Returns the parsed vendored data-plane description, parsing it on first use
    static OpenApi3::OpenApi3Schema getDataSchema() {
        AutoLock al(lck);
        return PineconeSchema::getSchemaIntern(DataSchemaFileName);
    }

    #! Returns the parsed vendored inference description, parsing it on first use
    static OpenApi3::OpenApi3Schema getInferenceSchema() {
        AutoLock al(lck);
        return PineconeSchema::getSchemaIntern(InferenceSchemaFileName);
    }

    #! Returns the pinned Admin schema
    /** @return the parsed Admin schema
        @throw REST-SCHEMA-ERROR the vendored schema cannot be parsed
    */
    static OpenApi3::OpenApi3Schema getAdminSchema() {
        AutoLock al(lck);
        return PineconeSchema::getSchemaIntern(AdminSchemaFileName);
    }

    #! Returns the independent Admin action set
    /** @return all 32 Admin actions and their reference data
        @throw REST-SCHEMA-ACTION-ERROR the manifest does not match the schema
    */
    static RestSchemaActions::RestSchemaActionSet getAdminActionSet() {
        AutoLock al(lck);
        if (!admin_set) {
            admin_set = new RestSchemaActions::RestSchemaActionSet(
                PineconeSchema::getSchemaIntern(AdminSchemaFileName), PineconeAdminManifest::Manifest,
                PineconeAdminManifest::ReferenceData);
        }
        return admin_set;
    }

    #! Returns the inference action set, building it on first use
    static RestSchemaActions::RestSchemaActionSet getInferenceActionSet() {
        AutoLock al(lck);
        return PineconeSchema::getInferenceActionSetIntern();
    }

    #! Returns the control-plane action set, building it on first use
    /** Built with the inference set as a peer, so that the integrated-embedding index action can offer
        the embedding models the inference API lists.
    */
    static RestSchemaActions::RestSchemaActionSet getControlActionSet() {
        AutoLock al(lck);
        return PineconeSchema::getControlActionSetIntern();
    }

    #! Returns the data-plane action set, building it on first use
    /** Built with both other sets as peers: the index dropdown and the host source that routes every one
        of its requests are declared by the control-plane set, and the reranking model dropdown by the
        inference set.
    */
    static RestSchemaActions::RestSchemaActionSet getDataActionSet() {
        AutoLock al(lck);
        if (!data_set) {
            data_set = new RestSchemaActions::RestSchemaActionSet(
                PineconeSchema::getSchemaIntern(DataSchemaFileName), PineconeManifest::DataManifest,
                PineconeManifest::DataReferenceData,
                (PineconeSchema::getControlActionSetIntern(),
                    PineconeSchema::getInferenceActionSetIntern()));
        }
        return data_set;
    }

    #! Returns every action set the application exports, in registration order
    static list<RestSchemaActions::RestSchemaActionSet> getActionSets() {
        return (
            PineconeSchema::getControlActionSet(),
            PineconeSchema::getDataActionSet(),
            PineconeSchema::getInferenceActionSet(),
        );
    }

    #! Returns the path to a vendored description file
    static string getSchemaPath(string file_name) {
        return Priv::PineconeModuleDir + DirSep + file_name;
    }

    #! Returns the inference action set; the lock must be held
    private static RestSchemaActions::RestSchemaActionSet getInferenceActionSetIntern() {
        if (!inference_set) {
            inference_set = new RestSchemaActions::RestSchemaActionSet(
                PineconeSchema::getSchemaIntern(InferenceSchemaFileName),
                PineconeManifest::InferenceManifest, PineconeManifest::InferenceReferenceData);
        }
        return inference_set;
    }

    #! Returns the control-plane action set; the lock must be held
    private static RestSchemaActions::RestSchemaActionSet getControlActionSetIntern() {
        if (!control_set) {
            control_set = new RestSchemaActions::RestSchemaActionSet(
                PineconeSchema::getSchemaIntern(ControlSchemaFileName),
                PineconeManifest::ControlManifest, PineconeManifest::ControlReferenceData,
                PineconeSchema::getInferenceActionSetIntern(), PineconeManifest::HostSources);
        }
        return control_set;
    }

    #! Returns a parsed description; the lock must be held
    private static OpenApi3::OpenApi3Schema getSchemaIntern(string file_name) {
        if (!schemas{file_name}) {
            string path = PineconeSchema::getSchemaPath(file_name);
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Pinecone API description "
                "%y)", $1.desc, path);
            schemas{file_name} = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schemas{file_name};
    }
}
}
