# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroSchema.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the XeroDataProvider module
public namespace XeroDataProvider {
#! Records where one vendored Xero schema came from and what it describes
/** The application's actions are generated from schemas committed to this module rather than fetched at run
    time, because an upstream change must not be able to alter the contract of an action that existing
    workflows call.  Updating them is a reviewed step run through \c tools/rest-schema-import.qr.

    Xero splits its API across documents served from different path prefixes, so there is one of these per
    document rather than one per application.

    @see @ref XeroDataProvider::XeroSchema "XeroSchema"
*/
public hashdecl XeroSchemaProvenanceInfo {
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

#! Loads and holds the vendored Xero schemas
/** @section xeroschema_documents One Application, Two Documents

    Xero does not publish one description of its API.  It publishes one per product, each with its own
    \c servers entry, and this application exports operations from two of them:

    |!Document|!Base path|!What it covers
    |\c xero-accounting.yaml|\c /api.xro/2.0|contacts, invoices, items, payments, bank transactions, quotes, credit notes, purchase orders
    |\c xero-projects.yaml|\c /projects.xro/2.0|projects, project tasks and project users

    Each becomes its own @ref RestSchemaActions::RestSchemaActionSet "RestSchemaActionSet", which is what
    makes the routing correct without any special case: a request's URI prefix comes from the base path of
    the schema its action was generated from, so an accounting action reaches \c /api.xro/2.0 and a projects
    action \c /projects.xro/2.0.

    What does **not** split cleanly is the dropdowns.  A project names a **contact**, and contacts are
    declared in the accounting document - so the projects set is built with the accounting set as a peer,
    and a projects action can offer a dropdown the accounting set produces.  The listing then runs against
    the accounting schema, which is the only way its URI prefix can be right.

    The sets are built in dependency order, accounting first, which is also what makes a cycle between two
    documents impossible to declare.

    @section xeroschema_payroll Payroll Is Not One of Them

    Xero's payroll API is three documents, one per region, and Australia's is a different **API version**
    (\c payroll.xro/1.0) from New Zealand's and the United Kingdom's (\c payroll.xro/2.0).  They do not
    describe the same operations: creating an employee takes an array in Australia and a single object in
    the other two, and listing them returns a different envelope.

    An action registered in the application catalog has exactly one option shape, so no generated action can
    stand for all three.  The two employee actions are therefore **hand-written**, take the region from the
    connection, and normalise what comes back; see
    @ref XeroDataProvider::XeroEmployeesDataProvider "XeroEmployeesDataProvider".

    @section xeroschema_parse Parsed On First Use

    Each schema is parsed once, on first use, rather than when the module loads, so that a program that only
    needs the connection does not pay for it.
*/
public class XeroSchema {
    public {
        #! Where the vendored accounting schema came from
        const AccountingProvenance = <XeroSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/XeroAPI/Xero-OpenAPI/master/xero_accounting.yaml",
            "source_sha256": "5cf7f780d3c8f619424c3ed72426e9fa3de2ad3a94f9d9ab0800fae0457fc4d9",
            "source_size": 929720,
            "imported": 2026-08-10,
            "source_paths": 138,
            "source_operations": 235,
            "source_schemas": 140,
            "source_webhooks": 0,
        };

        #! Where the vendored projects schema came from
        const ProjectsProvenance = <XeroSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/XeroAPI/Xero-OpenAPI/master/xero-projects.yaml",
            "source_sha256": "3870863245e208360fd169f9799c75ac6b22d6ebd0227d05dcf03648e101b7eb",
            "source_size": 50878,
            "imported": 2026-08-10,
            "source_paths": 7,
            "source_operations": 16,
            "source_schemas": 18,
            "source_webhooks": 0,
        };

        #! The name of the vendored accounting schema file, which lives beside this module
        const AccountingSchemaFileName = "xero-accounting.yaml";

        #! The name of the vendored projects schema file, which lives beside this module
        const ProjectsSchemaFileName = "xero-projects.yaml";
    }

    private {
        #! The parsed schemas, keyed by file name
        static hash<string, OpenApi3Schema> schemas = {};

        #! The action set built from the accounting schema and its manifest
        static *RestSchemaActionSet accounting_set;

        #! The action set built from the projects schema and its manifest
        static *RestSchemaActionSet projects_set;

        #! Serializes construction of the schemas and the action sets
        static Mutex lck();
    }

    #! Returns the parsed vendored accounting schema, parsing it on first use
    static OpenApi3Schema getAccountingSchema() {
        AutoLock al(lck);
        return XeroSchema::getSchemaIntern(AccountingSchemaFileName);
    }

    #! Returns the parsed vendored projects schema, parsing it on first use
    static OpenApi3Schema getProjectsSchema() {
        AutoLock al(lck);
        return XeroSchema::getSchemaIntern(ProjectsSchemaFileName);
    }

    #! Returns the accounting action set, building it on first use
    static RestSchemaActionSet getAccountingActionSet() {
        AutoLock al(lck);
        return XeroSchema::getAccountingActionSetIntern();
    }

    #! Returns the projects action set, building it on first use
    /** Built with the accounting set as a peer, so that a project's contact dropdown can be produced by the
        accounting listing that actually declares contacts.
    */
    static RestSchemaActionSet getProjectsActionSet() {
        AutoLock al(lck);
        if (!projects_set) {
            projects_set = new RestSchemaActionSet(XeroSchema::getSchemaIntern(ProjectsSchemaFileName),
                XeroManifest::ProjectsManifest, XeroManifest::ProjectsReferenceData,
                XeroSchema::getAccountingActionSetIntern());
        }
        return projects_set;
    }

    #! Returns every action set the application exports, in registration order
    static list<RestSchemaActionSet> getActionSets() {
        return (XeroSchema::getAccountingActionSet(), XeroSchema::getProjectsActionSet());
    }

    #! Returns the path to a vendored schema file
    static string getSchemaPath(string file_name) {
        return Priv::XeroModuleDir + DirSep + file_name;
    }

    #! Returns the accounting action set; the lock must be held
    private static RestSchemaActionSet getAccountingActionSetIntern() {
        if (!accounting_set) {
            # the manifest names a codec, and a codec is resolved from a process-wide registry when the set
            # is built, so it has to be there first
            XeroCodecs::register();
            accounting_set = new RestSchemaActionSet(XeroSchema::getSchemaIntern(AccountingSchemaFileName),
                XeroManifest::AccountingManifest, XeroManifest::AccountingReferenceData);
        }
        return accounting_set;
    }

    #! Returns a parsed schema; the lock must be held
    private static OpenApi3Schema getSchemaIntern(string file_name) {
        if (!schemas{file_name}) {
            string path = XeroSchema::getSchemaPath(file_name);
            on_error rethrow $1.err, sprintf("%s (while loading the vendored Xero schema %y)", $1.desc, path);
            schemas{file_name} = OpenApi3::OpenApi3Loader::fromFile(path);
        }
        return schemas{file_name};
    }
}
}
