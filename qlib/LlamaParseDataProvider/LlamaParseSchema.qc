# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LlamaParseDataProvider module: the pinned LlamaIndex Cloud API description

/** LlamaParseSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Private definitions for the LlamaParseDataProvider module
namespace Priv {
#! The directory this module was loaded from
/** Captured in a constant initializer, which is evaluated while this file is parsed, so that it names the
    module's own directory rather than whatever script happens to be calling later.
*/
const LlamaParseModuleDir = get_script_dir();
}

public namespace LlamaParseDataProvider {
#! Records where the vendored LlamaIndex Cloud API description came from and what it describes
/** The actions are generated from a schema committed to this module rather than fetched at run time,
    because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: import a named revision, check it against the
    manifest, and look at what changed.

    @see @ref LlamaParseDataProvider::LlamaParseSchema "LlamaParseSchema"
*/
public hashdecl LlamaParseSchemaProvenanceInfo {
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

    #! The version the upstream document reports for itself
    /** LlamaIndex versions its API in the path - \c /api/v1 and \c /api/v2 are both served, and this
        application uses \c v2 wherever the vendor publishes one - so the document's own version string
        is recorded for the record rather than sent anywhere.
    */
    string document_version;
}

#! Loads and holds the vendored LlamaIndex Cloud API description
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.  It is parsed once, when the module loads and registers its actions, which are built from it.

    @since %LlamaParseDataProvider 1.1
*/
public class LlamaParseSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <LlamaParseSchemaProvenanceInfo>{
            "source_url": "https://api.cloud.llamaindex.ai/api/openapi.json",
            "source_sha256": "8f4828c36c9ef45a66021af9417774882833b6abaa81966585937234448f2f3b",
            "source_size": 686329,
            "imported": 2026-09-06,
            "source_paths": 145,
            "source_operations": 201,
            "source_schemas": 406,
            "source_webhooks": 0,
            "document_version": "0.1.0",
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "llamaparse-openapi.json";
    }

    private {
        #! The parsed schema
        static *OpenApi3::OpenApi3Schema schema;

        #! The action set built from the schema and the manifest
        static *RestSchemaActions::RestSchemaActionSet action_set;

        #! Serializes construction of the schema and the action set
        static Mutex lck();
    }

    #! Returns the parsed vendored schema, parsing it on first use
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock al(lck);
        return LlamaParseSchema::getSchemaIntern();
    }

    #! Returns the action set for the generated actions, building it on first use
    static RestSchemaActions::RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActions::RestSchemaActionSet(
                LlamaParseSchema::getSchemaIntern(),
                LlamaParseManifest::Manifest,
                LlamaParseManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::LlamaParseModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3::OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = LlamaParseSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored LlamaIndex Cloud API "
                "description %y)", $1.desc, path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
