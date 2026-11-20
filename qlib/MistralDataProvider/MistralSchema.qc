# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MistralDataProvider module: the vendored Mistral API description

/** MistralSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Private definitions for the MistralDataProvider module
namespace Priv {
#! The directory this module was loaded from
/** Captured in a constant initializer, which is evaluated while this file is parsed, so that it names the
    module's own directory rather than whatever script happens to be calling later.
*/
const MistralModuleDir = get_script_dir();
}

public namespace MistralDataProvider {
#! Records where the vendored Mistral API description came from and what it describes
/** The actions are generated from a schema committed to this module rather than fetched at run time,
    because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: download the current official document over the vendored
    copy beside the import specification, import it, check it against the manifest, and look at what changed.

    @see @ref MistralDataProvider::MistralSchema "MistralSchema"
*/
public hashdecl MistralSchemaProvenanceInfo {
    #! The URL of the official document; the import reads the copy of it vendored beside the import specification
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
    /** The selected operations use explicit versioned endpoint paths. */
    string api_version;
}

#! Loads and holds the vendored Mistral API description
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference, and is parsed once when the action set is first requested. Programs that only use the
    separate MistralRestClient connection module do not load the action schema.

    @since %MistralDataProvider 1.1
*/
public class MistralSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <MistralSchemaProvenanceInfo>{
            "source_url": "https://docs.mistral.ai/openapi.yaml",
            "source_sha256": "7652f1307b3bc6cb7a10526c28788a430246f641cb9785a9f1b7b0dd912cdfae",
            "source_size": 1548910,
            "imported": 2026-11-17,
            "source_paths": 251,
            "source_operations": 347,
            "source_schemas": 1106,
            "source_webhooks": 0,
            "api_version": "1.0.0",
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "mistral-openapi.json";
    }

    private {
        #! The parsed schema
        static *OpenApi3::OpenApi3Schema schema;

        #! The action set built from the schema and the manifest
        static *RestSchemaActions::RestSchemaActionSet action_set;
    }

    private:internal {
        #! Serializes construction of the schema and the action set
        static Mutex lck();
    }

    #! Returns the parsed vendored schema, parsing it on first use
    /** @return the shared schema
        @throw EMPTY-SCHEMA-FILE the vendored resource is empty
        @throw JSON-PARSE-ERROR the vendored JSON is invalid
    */
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock al(lck);
        return MistralSchema::getSchemaIntern();
    }

    #! Returns the action set for the generated actions, building it on first use
    /** @return the shared, validated action set
        @throw REST-SCHEMA-ACTION-ERROR the manifest and schema disagree
    */
    static RestSchemaActions::RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActions::RestSchemaActionSet(MistralSchema::getSchemaIntern(),
                MistralManifest::Manifest, MistralManifest::ReferenceData,
                NOTHING, NOTHING, RestSchemaActions::RestSchemaActionSet::LazyResponseTypes);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    /** @return the schema resource path beside this module */
    static string getSchemaPath() {
        return Priv::MistralModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3::OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = MistralSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Mistral API "
                "description %y)", $1.desc, path);
            OpenApi3::OpenApi3Schema doc = OpenApi3::OpenApi3Loader::fromFile(path);
            # the labels of an enumeration reached at several locations are given once, to its schema
            foreach hash<auto> entry in (MistralManifest::EnumDisplayNames.pairIterator()) {
                doc.getSchemaObjectAt(entry.key).setEnumDisplayNames(entry.value);
            }
            schema = doc;
        }
        return schema;
    }
}
}
