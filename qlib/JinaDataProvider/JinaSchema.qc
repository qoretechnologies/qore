# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaDataProvider module: vendored inference API schema

/** JinaSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Private definitions for the JinaDataProvider module
namespace Priv {
#! The directory this module was loaded from
/** Captured in a constant initializer, which is evaluated while this file is parsed, so that it names the
    module's own directory rather than whatever script happens to be calling later.
*/
const JinaModuleDir = get_script_dir();
}

public namespace JinaDataProvider {
#! Records where the vendored Jina API schema came from and what it describes
/** The inference actions are generated from a schema committed to this module rather than fetched at run
    time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: import a named revision, check it against the
    manifest, and look at what changed.

    @see @ref JinaDataProvider::JinaSchema "JinaSchema"
*/
public hashdecl JinaSchemaProvenanceInfo {
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

    #! The number of webhooks in the source document
    int source_webhooks;

    #! The API version the upstream document describes
    /** Recorded because the document is served by the API itself, so the two move together and a
        version change is the signal to re-import.
    */
    string api_version;
}

#! Loads and holds the vendored Jina API schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference, and is parsed once. Action registration shares the cached schema and action set across all providers.

    @since %JinaDataProvider 1.1
*/
public class JinaSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <JinaSchemaProvenanceInfo>{
            "source_url": "https://api.jina.ai/openapi.json",
            "source_sha256": "87677e535552b380947607577f5c806fca1d1a90f528a2d5778bc5d725d8fc7f",
            "source_size": 102860,
            "imported": 2026-09-07,
            "source_paths": 17,
            "source_operations": 19,
            "source_schemas": 57,
            "source_webhooks": 0,
            "api_version": "2026.07.27.1603",
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "jina-openapi.json";
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
    /** @return the cached schema
        @throw any schema loading or validation exception
    */
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock al(lck);
        return JinaSchema::getSchemaIntern();
    }

    #! Returns the action set for the inference actions, building it on first use
    /** @return the cached action set
        @throw any schema loading or manifest validation exception
    */
    static RestSchemaActions::RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActions::RestSchemaActionSet(
                JinaSchema::getSchemaIntern(),
                JinaManifest::Manifest,
                JinaManifest::ReferenceData,
                NOTHING, NOTHING, RestSchemaActions::RestSchemaActionSet::LazyResponseTypes);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    /** @return the installed schema path */
    static string getSchemaPath() {
        return Priv::JinaModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3::OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = JinaSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Jina API "
                "schema %y)", $1.desc, path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
