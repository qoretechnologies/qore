# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureOpenAiDataProvider module: the pinned Azure OpenAI v1 API description

/** AzureOpenAiSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Private definitions for the AzureOpenAiDataProvider module
namespace Priv {
#! The directory this module was loaded from
/** Captured in a constant initializer, which is evaluated while this file is parsed, so that it names the
    module's own directory rather than whatever script happens to be calling later.
*/
const AzureOpenAiModuleDir = get_script_dir();
}

public namespace AzureOpenAiDataProvider {
#! Records where the vendored Azure OpenAI v1 API description came from and what it describes
/** The actions are generated from a schema committed to this module rather than fetched at run time,
    because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: import a named revision, check it against the
    manifest, and look at what changed.

    @see @ref AzureOpenAiDataProvider::AzureOpenAiSchema "AzureOpenAiSchema"
*/
public hashdecl AzureOpenAiSchemaProvenanceInfo {
    #! The URL the schema was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The date the schema was imported
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of operations the upstream document declares
    int source_operations;

    #! The number of component schemas the upstream document declares
    int source_schemas;

    #! The number of webhooks the upstream document declares
    int source_webhooks;

    #! The API version the upstream document reports for itself
    /** The v1 data-plane API does not require an <tt>api-version</tt> query parameter. */
    string api_version;
}

#! Loads and holds the vendored Azure OpenAI v1 API description
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference, and is parsed once when the action set is first requested. Programs that only use the
    separate AzureOpenAiRestClient connection module do not load the action schema.
*/
public class AzureOpenAiSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <AzureOpenAiSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/Azure/azure-rest-api-specs/a6943a926f76b3a2f90371b3466157eddf760e25/specification/ai/data-plane/OpenAI.v1/azure-v1-preview-generated.json",
            "source_sha256": "8d7298e942f1bc5a6ce4c2462a762e2c548861425034dff71b38ded8447f2cde",
            "source_size": 1539952,
            "imported": 2026-09-12,
            "source_paths": 81,
            "source_operations": 118,
            "source_schemas": 862,
            "source_webhooks": 0,
            "api_version": "preview",
        };

        #! Provenance of the deployment-based audio API description
        const AudioProvenance = <AzureOpenAiSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/Azure/azure-rest-api-specs/a6943a926f76b3a2f90371b3466157eddf760e25/specification/cognitiveservices/data-plane/AzureOpenAI/inference/preview/2025-04-01-preview/inference.json",
            "source_sha256": "431bd065fda5fcd73f0e08eec37b3790a1dd266dcac2ebe1c0e5141a9fad4190",
            "source_size": 620264,
            "imported": 2026-09-12,
            "source_paths": 36,
            "source_operations": 52,
            "source_schemas": 342,
            "source_webhooks": 0,
            "api_version": "2025-04-01-preview",
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "azure-openai-v1.json";
    }

    private {
        static list<OpenApi3::OpenApi3Schema> schemas;
        static list<RestSchemaActions::RestSchemaActionSet> action_sets;
        static Mutex lck();
    }

    #! Returns the parsed v1 schema
    /** @return the shared v1 schema
        @throw EMPTY-SCHEMA-FILE a vendored resource is empty
        @throw REST-SCHEMA-ACTION-ERROR a manifest and schema disagree
    */
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock al(lck);
        initIntern();
        return schemas[0];
    }

    #! Returns the parsed deployment-based audio schema
    /** @return the shared audio schema
        @throw REST-SCHEMA-ACTION-ERROR a manifest and schema disagree
    */
    static OpenApi3::OpenApi3Schema getAudioSchema() {
        AutoLock al(lck);
        initIntern();
        return schemas[1];
    }

    #! Returns the primary v1 action set
    /** @return the shared v1 actions */
    static RestSchemaActions::RestSchemaActionSet getActionSet() {
        return getActionSets()[0];
    }

    #! Returns the v1 and deployment-based audio action sets
    /** @return both shared, validated action sets
        @throw REST-SCHEMA-ACTION-ERROR a manifest and schema disagree
    */
    static list<RestSchemaActions::RestSchemaActionSet> getActionSets() {
        AutoLock al(lck);
        initIntern();
        return action_sets;
    }

    #! Finds the action set owning a stable action ID
    /** @param action the action ID
        @return its action set, or nothing if it is unknown
    */
    static *RestSchemaActions::RestSchemaActionSet findActionSet(string action) {
        foreach RestSchemaActions::RestSchemaActionSet set in (getActionSets()) {
            if (set.getOperation(action)) {
                return set;
            }
        }
        return;
    }

    #! Returns an action contract from either API
    /** @param action the stable action ID
        @return its operation
        @throw AZURE-OPENAI-ACTION-ERROR the action is unknown
    */
    static RestSchemaActions::RestSchemaActionOperation getOperationEx(string action) {
        *RestSchemaActions::RestSchemaActionSet set = findActionSet(action);
        if (!set) {
            throw "AZURE-OPENAI-ACTION-ERROR", sprintf("unknown Azure OpenAI action %y", action);
        }
        return set.getOperationEx(action);
    }

    #! Returns the path to the primary v1 resource
    /** @return the v1 schema resource path beside this module */
    static string getSchemaPath() {
        return Priv::AzureOpenAiModuleDir + DirSep + SchemaFileName;
    }

    #! Constructs both partitions under the lock and publishes only fully validated action sets
    private static initIntern() {
        if (action_sets) {
            return;
        }
        list<OpenApi3::OpenApi3Schema> parsed = (
            OpenApi3::OpenApi3Loader::fromFile(getSchemaPath()),
            OpenApi3::OpenApi3Loader::fromFile(Priv::AzureOpenAiModuleDir + "/azure-openai-audio.json"),
        );
        RestSchemaActions::RestSchemaActionSet primary(parsed[0], AzureOpenAiManifest::Manifest,
            AzureOpenAiManifest::ReferenceData);
        RestSchemaActions::RestSchemaActionSet audio(parsed[1], AzureOpenAiAudioManifest::Manifest,
            NOTHING, (primary,));
        schemas = parsed;
        action_sets = (primary, audio);
    }
}
}
