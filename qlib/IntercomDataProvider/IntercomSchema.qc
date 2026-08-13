# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the IntercomDataProvider module
public namespace IntercomDataProvider {
#! Records where the vendored Intercom schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr.

    @see @ref IntercomDataProvider::IntercomSchema "IntercomSchema"
*/
public hashdecl IntercomSchemaProvenanceInfo {
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

    #! The Intercom API revision this description describes
    /** Not written by the importer.  It is the revision
        @ref IntercomRestClient::IntercomRestClientBase::ApiVersion "ApiVersion" sends with every request,
        and the two must move together: an action generated from this description and sent against another
        revision would be making a request the description does not describe.
    */
    string api_version;
}

#! Loads and holds the vendored Intercom schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference - 15 of the upstream document's 164 paths and 83 of its 338 component schemas, 227 KB of the
    upstream 1.3 MB.  That is not only a size reduction: parsing the full document is a cost not worth
    paying on every data provider construction for an application exporting nineteen operations.

    \c "POST /conversations/{conversation_id}/reply" is deliberately absent.  It is the one action this
    application writes by hand, so no operation selects that path and the pruner drops it; see
    @ref IntercomDataProvider::IntercomReplyConversationDataProvider "IntercomReplyConversationDataProvider".

    @section intercomschema_version The Revision Is the Pin

    Intercom publishes one description per dated API revision, so pinning \c descriptions/2.16 pins the API
    version as well.  The revision is not a property of this document that can be read out of it and
    changed independently: it decides which operations exist and what they accept, and it is sent on every
    request as \c Intercom-Version.  Adopting a newer revision is therefore a re-import of a different
    upstream file, run through \c tools/rest-schema-import.qr with
    \c qlib/IntercomDataProvider/intercom-import.yaml, and \c api_version below moves with it.

    @section intercomschema_repairs What Was Repaired at Import Time

    Three listings - \c "GET /articles", \c "GET /contacts" and \c "GET /help_center/collections" - answer
    Intercom's \c cursor_pages envelope, whose \c next object carries \c per_page and \c starting_after, and
    declare neither as a request parameter.  An overlay cannot add a parameter, so without a repair those
    actions could never ask for a second page.

    The omission is internal to the document rather than an API without paging: \c "GET /conversations"
    returns the *same* \c cursor_pages shape and does declare exactly those two parameters.  So the two
    parameters are added at import time, with that reason recorded in the committed artifact under
    \c x-qore-normalized.  \c "GET /tags", \c "GET /admins" and \c "GET /contacts/{contact_id}/tags" are
    left alone: they answer envelopes with no \c pages object at all, so they are genuinely unpaged.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class IntercomSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <IntercomSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/intercom/Intercom-OpenAPI/main/descriptions/2.16/api.intercom.io.yaml",
            "source_sha256": "57be9a3f321bfc3b67a073f5ed14d1920a9450f5eb11a43caed8dfcdda14a2dc",
            "source_size": 1319560,
            "imported": 2026-08-13,
            "source_paths": 164,
            "source_operations": 231,
            "source_schemas": 338,
            "source_webhooks": 0,
            "api_version": IntercomRestClient::IntercomRestClientBase::ApiVersion,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "intercom-openapi.yaml";
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
        return IntercomSchema::getSchemaIntern();
    }

    #! Returns the action set for the Intercom application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(IntercomSchema::getSchemaIntern(),
                IntercomManifest::Manifest, IntercomManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::IntercomModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = IntercomSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Intercom schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
