# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the StripeDataProvider module
public namespace StripeDataProvider {
#! Records where the vendored Stripe schema came from and what it describes
/** The application's actions are generated from a schema committed to this module rather than fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating it is a reviewed step: import a named revision, check it against the manifest,
    and look at what changed.

    @see @ref StripeDataProvider::StripeSchema "StripeSchema"
*/
public hashdecl StripeSchemaProvenanceInfo {
    #! The URL the schema was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The Stripe API version the schema describes
    /** The @ref striperestclientintro "StripeRestClient" module sends this version with every request, so
        the two must be changed together.
    */
    string api_version;

    #! The date the schema was imported
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of component schemas the upstream document declares
    int source_schemas;
}

#! Loads and holds the vendored Stripe schema
/** The schema is pruned to the exported operations and the transitive closure of everything they reference.
    That is not only a size reduction: the full upstream document declares 416 paths and 1440 component
    schemas and takes around 45 seconds to parse, which is not a tolerable cost to pay for an application
    exposing a few dozen operations.  1022 of the 1440 schemas survive pruning, because Stripe's types are
    heavily interconnected.

    The schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class StripeSchema {
    public {
        #! Where the vendored schema came from
        const Provenance = <StripeSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/stripe/openapi/master/openapi/spec3.json",
            "source_sha256": "3653ad45bbec54fcbe461c541c908355b715018bdf455a0e11b27bedb2cbdee5",
            "source_size": 7967776,
            "api_version": "2026-07-29.dahlia",
            "imported": 2026-08-08,
            "source_paths": 416,
            "source_schemas": 1440,
        };

        #! The name of the vendored schema file, which lives beside this module
        const SchemaFileName = "stripe-openapi3.json";
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
        return StripeSchema::getSchemaIntern();
    }

    #! Returns the action set for the Stripe application, building it on first use
    static RestSchemaActionSet getActionSet() {
        AutoLock al(lck);
        if (!action_set) {
            action_set = new RestSchemaActionSet(StripeSchema::getSchemaIntern(), StripeManifest::Manifest,
                StripeManifest::ReferenceData);
        }
        return action_set;
    }

    #! Returns the path to the vendored schema file
    static string getSchemaPath() {
        return Priv::StripeModuleDir + DirSep + SchemaFileName;
    }

    #! Returns the parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern() {
        if (!schema) {
            string path = StripeSchema::getSchemaPath();
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Stripe schema %y)", $1.desc, path);
            schema = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schema;
    }
}
}
