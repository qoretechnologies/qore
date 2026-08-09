# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! Records where the vendored Paddle schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: import a named revision, check it against the manifest,
    and look at what changed.

    @see @ref PaddleDataProvider::PaddleSchema "PaddleSchema"
*/
public hashdecl PaddleSchemaProvenanceInfo {
    #! The URL the schema was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The date the upstream document was last changed
    date source_updated;

    #! The date the schema was imported
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of operations the upstream document declares
    int source_operations;

    #! The number of component schemas the upstream document declares
    int source_schemas;

    #! The number of webhook event types the upstream document declares
    int source_webhooks;
}

#! Loads and holds the vendored Paddle schema
/** The schema is pruned to the exported operations and the transitive closure of everything they reference.
    That is not only a size reduction: the full upstream document is 7.4 MB and takes around a second to
    parse, which is not a cost worth paying on every data provider construction for an application exposing
    a couple of dozen operations.  237 of 519 component schemas survive pruning, because Paddle's entity
    types reference each other heavily.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class PaddleSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <PaddleSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/PaddleHQ/paddle-openapi/main/v1/openapi.yaml",
            "source_sha256": "e1ac347eb3c5ff208fdbd6af9c4b3f7e9e4e9212be881468ee4f97792058734b",
            "source_size": 7403758,
            "source_updated": 2026-07-09,
            "imported": 2026-08-09,
            "source_paths": 70,
            "source_operations": 99,
            "source_schemas": 519,
            "source_webhooks": 56,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "paddle-openapi.yaml";
    }

    private {
        #! The parsed schema
        static *OpenApi3Schema schema;

        #! The action set built from the schema and the manifest
        static *RestSchemaActionSet action_set;

        #! Serializes construction of the schema and the action set
        static Mutex lck();
    }

    #! Returns the parsed vendored schema, parsing it on first use
    static OpenApi3Schema getSchema() {
        AutoLock al(lck);
        return PaddleSchema::getSchemaIntern();
    }

    #! Returns the action set for the Paddle application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(PaddleSchema::getSchemaIntern(), PaddleManifest::Manifest,
                PaddleManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::PaddleModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = PaddleSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Paddle schema %y)", $1.desc, path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
