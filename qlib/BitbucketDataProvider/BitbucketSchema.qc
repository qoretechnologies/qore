# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BitbucketDataProvider module
public namespace BitbucketDataProvider {
#! Records where the vendored Bitbucket schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step run through \c tools/rest-schema-import.qr.

    @see @ref BitbucketDataProvider::BitbucketSchema "BitbucketSchema"
*/
public hashdecl BitbucketSchemaProvenanceInfo {
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

#! Loads and holds the vendored Bitbucket schema
/** The schema is pruned to the exported operations and the transitive closure of everything they
    reference.

    @section bitbucketschema_composition Everything Is a Composition

    Bitbucket's description builds every entity with \c allOf on a shared \c object base that carries the
    \c type discriminator, so a request or response type is only visible after composition is resolved.
    That is why the pruned document keeps more component schemas than the operation count suggests: an
    entity reaches its base, its links, its \c user and its \c repository, and those reach further.

    @section bitbucketschema_nested Nested Request Shapes

    A pull request names its branches as \c "source.branch.name", and its reviewers as a list of objects
    carrying a \c uuid.  Presenting those as they are declared would make a user fill in three levels of
    nesting to name a branch, so the manifest presents them as plain values and converts them with wire
    codecs; see @ref BitbucketDataProvider::BitbucketManifest "BitbucketManifest".

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class BitbucketSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <BitbucketSchemaProvenanceInfo>{
            "source_url": "https://developer.atlassian.com/cloud/bitbucket/swagger.v3.json",
            "source_sha256": "8c7911c0a9109be263a3528b0307f1f3418bf0fabac26bdd50d7f3841af94f89",
            "source_size": 1485729,
            "imported": 2026-08-10,
            "source_paths": 193,
            "source_operations": 331,
            "source_schemas": 211,
            "source_webhooks": 0,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "bitbucket-openapi.json";
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
        return BitbucketSchema::getSchemaIntern();
    }

    #! Returns the action set for the Bitbucket application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            # the manifest names codecs, and a codec is resolved from a process-wide registry when the
            # set is built, so they have to be there first
            BitbucketCodecs::register();
            action_set = new RestSchemaActionSet(BitbucketSchema::getSchemaIntern(),
                BitbucketManifest::Manifest, BitbucketManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::BitbucketModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = BitbucketSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Bitbucket schema %y)", $1.desc,
                path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
