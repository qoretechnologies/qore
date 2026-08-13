# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the KlaviyoDataProvider module
public namespace KlaviyoDataProvider {
#! Records where the vendored Klaviyo schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr.

    @see @ref KlaviyoDataProvider::KlaviyoSchema "KlaviyoSchema"
*/
public hashdecl KlaviyoSchemaProvenanceInfo {
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

    #! The Klaviyo API revision this description describes
    /** Not written by the importer.  It is the revision
        @ref KlaviyoRestClient::KlaviyoRestClientBase::ApiRevision "ApiRevision" sends with every request,
        and the two must move together: an action generated from this description and sent against another
        revision would be making a request the description does not describe.
    */
    string api_version;
}

#! Loads and holds the vendored Klaviyo schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.  That is not only a size reduction: Klaviyo's combined description is 3.0 MB, 238 paths and
    1,295 component schemas, and parsing all of it is a cost not worth paying on every data provider
    construction for an application exporting a few dozen operations.

    @section klaviyoschema_combined One Document, Not Per-Category Slices

    Klaviyo publishes per-category slices - profiles, lists, campaigns and so on - **and** a combined
    \c openapi/stable.json carrying every path in one file.  The combined document is what is imported,
    because every path this application exports is in it, so there is one schema to load, one revision to
    check, and no cross-category reference to resolve by hand.

    @section klaviyoschema_version The Revision Is the Pin

    Klaviyo carries the API revision in \c info.version, and it is sent on every request as the \c revision
    header.  It is not a property of the document that can be read out of it and changed independently: it
    decides what an operation accepts and, for a deprecated revision, whether it answers at all.  Adopting a
    newer revision is therefore a re-import, run through \c tools/rest-schema-import.qr with
    \c qlib/KlaviyoDataProvider/klaviyo-import.yaml, and \c api_version below moves with it.

    A revision is stable for a year and deprecated for a second, then removed - so \c 2026-07-15 is stable
    until 2027-07-15 and gone on 2028-07-15.

    @section klaviyoschema_repairs Nothing Was Repaired at Import Time

    Unlike the Bitbucket and Intercom descriptions, Klaviyo's needs no repair for this application to work:
    every collection GET it exports declares \c "page[cursor]", and all but \c "GET /api/metrics" also
    declare \c "page[size]", so a generated action can always ask for the next page.  The import spec
    therefore has no \c normalize block, and the committed artifact carries no \c x-qore-normalized marker.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class KlaviyoSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <KlaviyoSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/klaviyo/openapi/main/openapi/stable.json",
            "source_sha256": "c7eae914d2d7b7c0955d90e545648a74a2e98ccda9d12773e0d26bf8373c9e12",
            "source_size": 3063700,
            "imported": 2026-08-13,
            "source_paths": 238,
            "source_operations": 345,
            "source_schemas": 1295,
            "source_webhooks": 0,
            "api_version": KlaviyoRestClient::KlaviyoRestClientBase::ApiRevision,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "klaviyo-openapi.json";
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
        return KlaviyoSchema::getSchemaIntern();
    }

    #! Returns the action set for the Klaviyo application, building it on first use
    /** The application's wire codecs are registered first: they live in a process-wide registry, and the
        manifest names them, so a set built before they exist would fail to resolve them.
    */
    static RestSchemaActionSet getActionSet() {
        KlaviyoCodecs::register();
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(KlaviyoSchema::getSchemaIntern(),
                KlaviyoManifest::Manifest, KlaviyoManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::KlaviyoModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = KlaviyoSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Klaviyo schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
