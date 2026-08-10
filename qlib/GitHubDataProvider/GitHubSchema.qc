# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! Records where the vendored GitHub schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr.

    @see @ref GitHubDataProvider::GitHubSchema "GitHubSchema"
*/
public hashdecl GitHubSchemaProvenanceInfo {
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

    #! The REST API version the requests declare and this document describes
    /** GitHub versions its REST API by date, and \c GitHubRestClient sends this version with every
        request.  The two must move together: adopting a newer description without moving the pin would
        generate actions against a version the requests do not ask for.
    */
    string api_version;
}

#! Loads and holds the vendored GitHub schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.

    @section githubschema_size The Document Is 12.9 MB

    GitHub publishes one description covering its entire REST API — 808 paths — so parsing it on every
    data provider construction would be untenable.  That is what makes pruning load-bearing here rather
    than merely tidy: the committed artifact is a fraction of a percent of the document it came from.

    @section githubschema_version The Version Pin Moves With the Document

    \c api_version records the REST API version the requests declare, because
    @ref GitHubRestClient::GitHubRestClientBase "GitHubRestClientBase" sends it with every request.
    Adopting a newer description without moving the pin would generate actions against a version the
    requests never ask for.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class GitHubSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <GitHubSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/github/rest-api-description/main/descriptions/api.github.com/api.github.com.json",
            "source_sha256": "80850db290cde4eb487e0efb587cf27f305e77b6bef96933ed8a09b5169d5b1d",
            "source_size": 12920264,
            "imported": 2026-08-10,
            "source_paths": 808,
            "source_operations": 1220,
            "source_schemas": 969,
            "source_webhooks": 270,
            "api_version": GitHubRestClient::GitHubRestClientBase::ApiVersion,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "github-openapi.json";
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
        return GitHubSchema::getSchemaIntern();
    }

    #! Returns the action set for the GitHub application, building it on first use
    /** No wire codec is registered: GitHub writes every timestamp as an RFC 3339 string, which is what the
        schema declares, so there is no wire type to correct.
    */
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(GitHubSchema::getSchemaIntern(),
                GitHubManifest::Manifest, GitHubManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::GitHubModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = GitHubSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored GitHub schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
