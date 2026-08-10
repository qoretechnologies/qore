# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitLabDataProvider module
public namespace GitLabDataProvider {
#! Records where the vendored GitLab schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr: import a
    named revision, check it against the manifest, and look at what changed.

    @see @ref GitLabDataProvider::GitLabSchema "GitLabSchema"
*/
public hashdecl GitLabSchemaProvenanceInfo {
    #! The URL the schema was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The GitLab version the upstream document describes
    /** Recorded because GitLab's description is generated from the source of one particular release, so
        the document and the instance it describes move together; an instance older than this may not
        offer everything the actions declare.
    */
    string gitlab_version;

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

#! Loads and holds the vendored GitLab schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.  That is not only a size reduction: the full upstream document is 3.3 MB describing 1284
    paths and 1752 operations, and parsing it takes several seconds, which is not a cost worth paying on
    every data provider construction for an application exposing 68 of them.

    Only 70 of 865 component schemas survive pruning — a much sharper reduction than Paddle's or Stripe's,
    because GitLab's description inlines most response shapes rather than referencing shared entity types.

    @section gitlabschema_generated Why This Document

    GitLab **generates** this description from its own API source (\c lib/tasks/gitlab/openapi/v3.rake)
    and publishes it in the same repository as the code, so it cannot silently lag the API the way a
    hand-maintained description does.  It is also the same source the Swagger 2.0 document the superseded
    TypeScript application vendored was generated from, which is why every action ID carries over
    unchanged.

    @section gitlabschema_defect One Operation Is Not Generated From This Schema

    \c "POST /api/v4/projects/{id}/repository/commits" — creating a commit — is declared upstream with a
    \c multipart/form-data body whose only property is \c file, which is not the API GitLab documents or
    implements: the real request is a JSON body carrying \c branch, \c commit_message and an \c actions
    array.  15 of the document's 656 request bodies carry the same generated placeholder.

    An overlay must never be used for structural repair, and inventing the missing fields here would be
    exactly that, so this one action is **hand-written** instead — see
    @ref GitLabDataProvider::GitLabCreateCommitDataProvider "GitLabCreateCommitDataProvider".  It keeps its
    action ID, so nothing calling it can tell the difference.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class GitLabSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <GitLabSchemaProvenanceInfo>{
            "source_url": "https://gitlab.com/gitlab-org/gitlab/-/raw/master/doc/api/openapi/openapi_v3.yaml",
            "source_sha256": "1c16d4750bd37b52386a7ea58216398434152f2789e9405a3f60af3c41e63b1b",
            "source_size": 3490770,
            "gitlab_version": "19.3.0-pre",
            "imported": 2026-08-10,
            "source_paths": 1284,
            "source_operations": 1752,
            "source_schemas": 865,
            "source_webhooks": 0,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "gitlab-openapi.yaml";
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
        return GitLabSchema::getSchemaIntern();
    }

    #! Returns the action set for the GitLab application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(GitLabSchema::getSchemaIntern(), GitLabManifest::Manifest,
                GitLabManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::GitLabModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = GitLabSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored GitLab schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
