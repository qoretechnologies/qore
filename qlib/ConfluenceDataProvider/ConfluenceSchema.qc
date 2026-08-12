# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConfluenceDataProvider module definition

/*  ConfluenceSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ConfluenceDataProvider module
public namespace ConfluenceDataProvider {
#! Records where the vendored Confluence schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr.

    @see @ref ConfluenceDataProvider::ConfluenceSchema "ConfluenceSchema"
*/
public hashdecl ConfluenceSchemaProvenanceInfo {
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

    #! The number of webhook event types the upstream document declares
    int source_webhooks;
}

#! Loads and holds the vendored Confluence schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.

    @section confluenceschema_site One Schema, Two Ways In

    Confluence's description declares its server as \c "https://{your-domain}/wiki/api/v2", so the schema's
    base path is \c /wiki/api/v2 and the host is the connection's business.  That prefix is the same
    whether the site is reached directly or through Atlassian's OAuth2 gateway, which is why one schema
    serves both; see @ref ConfluenceRestClient::ConfluenceRestClientBase "ConfluenceRestClientBase".

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class ConfluenceSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <ConfluenceSchemaProvenanceInfo>{
            "source_url": "https://developer.atlassian.com/cloud/confluence/openapi-v2.v3.json",
            "source_sha256": "c4bd1ed570f21cd6dee2360d9cda0d5aa09933233491c8286d60cc4fa16c5df5",
            "source_size": 611500,
            "imported": 2026-08-10,
            "source_paths": 151,
            "source_operations": 218,
            "source_schemas": 142,
            "source_webhooks": 0,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "confluence-openapi.json";
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
        return ConfluenceSchema::getSchemaIntern();
    }

    #! Returns the action set for the Confluence application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(ConfluenceSchema::getSchemaIntern(),
                ConfluenceManifest::Manifest, ConfluenceManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::ConfluenceModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = ConfluenceSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Confluence schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
