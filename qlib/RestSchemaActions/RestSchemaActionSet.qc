# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaActionSet.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! Describes a dropdown populated by calling another action in the same set
/** An identifier presented as a free-text field is one of the clearest signs of an action generated straight
    from a schema.  A schema knows that \c customer is a string; it does not know that the string is the ID of
    an object the same API can list.

    Declaring that relationship here turns the field into a dropdown without any application code: the
    reference data is produced by executing another action from the same manifest and mapping its records.

    @see @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ref_data "RestSchemaFieldOverlayInfo::ref_data"
*/
public hashdecl RestSchemaReferenceDataInfo {
    #! The reference data name, as named by a field overlay's \c ref_data or \c element_ref_data
    string name;

    #! The action in this set that lists the values
    string action;

    #! Fixed options passed to that action
    /** Typically a page size, so that populating a dropdown does not walk the whole collection.
    */
    *hash<auto> options;

    #! A path of field names leading from the action's output to the list of records
    /** Not needed when the action's output is itself the list.
    */
    *softlist<string> records_path;

    #! The record field supplying the value
    string value_field;

    #! The record field supplying the display name
    /** Falls back to @ref value_field when absent, or when a record does not carry it — many APIs allow the
        human-readable name of an object to be unset.
    */
    *string display_field;

    #! A description of the reference data
    *string desc;
}

#! An application's set of schema-derived actions
/** Owns the manifest, resolves every entry against the schema, and generates the
    @ref DataProvider::DataProviderActionInfo "DataProviderActionInfo" hashes an application registers.

    All validation happens in the constructor, so a manifest that has drifted from its schema — an operation
    that no longer exists, an overlay entry that no longer resolves, a reference-data entry naming an action
    that was removed — fails when the module is loaded rather than when a user opens the action.
*/
public class RestSchemaActionSet {
    private {
        #! The schema the actions are derived from
        AbstractRestSchemaValidator schema;

        #! The manifest, in declaration order
        list<hash<RestSchemaActionInfo>> manifest;

        #! Resolved operations, keyed by action ID
        hash<string, RestSchemaActionOperation> opmap = {};

        #! Reference data declarations, keyed by reference data name
        hash<string, hash<RestSchemaReferenceDataInfo>> refdata;
    }

    #! Resolves and validates an application's manifest against a schema
    /** @param schema the schema the actions are derived from
        @param manifest the operations the application exports
        @param refdata reference data declarations, keyed by reference data name

        @throw REST-SCHEMA-ACTION-ERROR the manifest is inconsistent, or an entry does not resolve
    */
    constructor(AbstractRestSchemaValidator schema, list<hash<RestSchemaActionInfo>> manifest,
            *hash<string, hash<RestSchemaReferenceDataInfo>> refdata) {
        self.schema = schema;
        self.manifest = manifest;
        self.refdata = refdata ?? {};

        hash<string, string> seen_ops = {};
        # manifest entries by action ID, for resolving "variant_of" against what came before
        hash<string, hash<RestSchemaActionInfo>> variant_base = {};
        foreach hash<RestSchemaActionInfo> info in (manifest) {
            if (exists opmap{info.action}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y is declared more than once in the manifest",
                    info.action);
            }
            string opkey = info.method.upr() + " " + info.path;
            if (exists seen_ops{opkey} && !info.variant_of.val()) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("operation \"%s\" is exported twice: as action %y and "
                    "as action %y; if the second is a deliberately narrowed variant of the first, say so with "
                    "\"variant_of\"", opkey, seen_ops{opkey}, info.action);
            }
            if (info.variant_of.val()) {
                *hash<RestSchemaActionInfo> base = variant_base{info.variant_of};
                if (!base) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y declares itself a variant of action %y, "
                        "which is not declared before it in the manifest", info.action, info.variant_of);
                }
                if (base.method.upr() != info.method.upr() || base.path != info.path) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y declares itself a variant of action %y, "
                        "but exports \"%s\" where %y exports \"%s %s\"; a variant must narrow the same "
                        "operation", info.action, info.variant_of, opkey, info.variant_of,
                        base.method.upr(), base.path);
                }
            }
            variant_base{info.action} = info;
            if (!exists seen_ops{opkey}) {
                seen_ops{opkey} = info.action;
            }
            opmap{info.action} = new RestSchemaActionOperation(schema, info);
        }
        checkReferenceData();
    }

    #! Returns the schema the actions are derived from
    AbstractRestSchemaValidator getSchema() {
        return schema;
    }

    #! Returns the action IDs in manifest order
    list<string> getActionNames() {
        return keys opmap;
    }

    #! Returns the operation for the given action ID, if any
    *RestSchemaActionOperation getOperation(string action) {
        return opmap{action};
    }

    #! Returns the operation for the given action ID
    /** @throw REST-SCHEMA-ACTION-ERROR the action is unknown
    */
    RestSchemaActionOperation getOperationEx(string action) {
        *RestSchemaActionOperation rv = opmap{action};
        if (!rv) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y is unknown; known actions: %y", action,
                keys opmap);
        }
        return rv;
    }

    #! Returns the reference data declarations
    hash<string, hash<RestSchemaReferenceDataInfo>> getReferenceDataDecls() {
        return refdata;
    }

    #! Returns the action definitions to register for the given application
    /** @param app the application name
        @param common values merged into every generated action definition

        @return the action definitions, in manifest order
    */
    list<hash<DataProviderActionInfo>> getActionInfoList(string app, *hash<auto> common) {
        list<hash<DataProviderActionInfo>> rv = ();
        foreach hash<RestSchemaActionInfo> info in (manifest) {
            rv += getActionInfo(app, info.action, common);
        }
        return rv;
    }

    #! Returns the action definition to register for a single action
    /** @param app the application name
        @param action the action ID
        @param common values merged into the generated action definition

        @return the action definition
    */
    hash<DataProviderActionInfo> getActionInfo(string app, string action, *hash<auto> common) {
        RestSchemaActionOperation op = getOperationEx(action);
        hash<RestSchemaActionInfo> info = op.getInfo();
        on_error rethrow $1.err, sprintf("%s (while generating action %y for app %y)", $1.desc, action, app);

        hash<DataProviderActionInfo> rv = <DataProviderActionInfo>{
            "app": app,
            "action": action,
            "path": "/" + action,
            "display_name": info.display_name,
            "short_desc": info.short_desc,
            "desc": info.desc,
            "action_code": DPAT_API,
        };
        if (info.groups) {
            rv.groups = info.groups;
        }
        if (info.metadata) {
            rv.metadata = info.metadata;
        }
        if (*hash<string, AbstractDataField> fields = op.getRequestFields()) {
            rv.options = DataProviderActionCatalog::getActionOptionFromFields(fields);
        }
        if (*AbstractDataProviderType type = op.getResponseType()) {
            rv.output_type = type;
        }
        if (common) {
            rv += common;
        }
        return rv;
    }

    #! Registers every action in the set for the given application
    /** @param app the application name
        @param common values merged into every generated action definition
    */
    registerActions(string app, *hash<auto> common) {
        map DataProviderActionCatalog::registerAction($1), getActionInfoList(app, common);
    }

    #! Verifies that every action in the set resolves to a data provider under the given root
    /** An action definition names a data provider path; if that path does not resolve, the action exists in
        the catalog but cannot be executed.  Applications should call this from their test suites.

        @param root the application's root data provider

        @throw REST-SCHEMA-ACTION-ERROR an action path does not resolve, or resolves to a provider that does
        not support the request API
    */
    validateActionPaths(AbstractDataProvider root) {
        foreach string action in (keys opmap) {
            *AbstractDataProvider child = root.getChildProvider(action);
            if (!child) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y declares data provider path \"/%s\", which "
                    "does not resolve under provider %y; children: %y", action, action, root.getName(),
                    root.getChildProviderNames());
            }
            if (!child.getInfo().supports_request) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y resolves to provider %y, which does not "
                    "support the request API", action, child.getName());
            }
        }
    }

    #! Validates the reference data declarations against the manifest and the overlays
    private checkReferenceData() {
        foreach hash<auto> i in (refdata.pairIterator()) {
            hash<RestSchemaReferenceDataInfo> rd = i.value;
            if (rd.name != i.key) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("reference data is registered under key %y but names "
                    "itself %y", i.key, rd.name);
            }
            if (!exists opmap{rd.action}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("reference data %y is produced by action %y, which "
                    "the manifest does not export; exported actions: %y", i.key, rd.action, keys opmap);
            }
            # a fixed option that the listing action does not accept would fail only when a user opened a
            # dropdown, so check it now
            RestSchemaActionOperation lop = opmap{rd.action};
            hash<string, hash<RestSchemaFlatFieldInfo>> flat = lop.getFlatMap();
            foreach string opt in (keys rd.options) {
                if (!exists flat{opt}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("reference data %y passes option %y to action %y, "
                        "which does not declare it; options: %y", i.key, opt, rd.action, sort(keys flat));
                }
            }
        }

        # every ref_data named by an overlay must be declared, or the dropdown would silently stay empty
        foreach hash<auto> i in (opmap.pairIterator()) {
            *hash<string, AbstractDataField> fields = i.value.getRequestFields();
            foreach hash<auto> f in (fields.pairIterator()) {
                *hash<auto> attr = f.value.getAttributes();
                foreach string key in (("ref_data", "element_ref_data")) {
                    if (attr{key}.val() && !exists refdata{attr{key}}) {
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y option %y declares %s %y, which is "
                            "not declared in the reference data of this action set; declared reference data: "
                            "%y", i.key, f.key, key, attr{key}, sort(keys refdata));
                    }
                }
            }
        }
    }
}
}
