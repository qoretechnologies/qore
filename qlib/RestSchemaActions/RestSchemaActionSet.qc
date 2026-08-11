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

    #! Options taken from the action being filled in, keyed by the listing action's option name
    /** Turns a dropdown that can only list globally into one scoped by what the user has already chosen:
        the branches of the selected project, the pages of the selected space.  Each entry names an option
        of the listing action, and the value names the option of the **calling** action supplying it.

        Every named value is required: until the user has filled in the option it depends on, the listing
        cannot be made at all, so the dropdown is empty rather than listing the wrong thing.  Pair this with
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::depends_on "depends_on" on the field, so that the
        UI knows to re-read the dropdown when the value it is scoped by changes.

        Both sides are validated when the action set is built: the key must be an option of the listing
        action, and the value must be an option of every action whose overlay names this reference data.
    */
    *hash<string, string> options_from;

    #! A path of field names leading from the action's output to the list of records
    /** Not needed when the action's output is itself the list.
    */
    *softlist<string> records_path;

    #! The record field supplying the value
    /** A \c "." separated path reaches into a nested record: a Bitbucket workspace membership carries the
        identity as \c "user.uuid", not at the top level, and a dropdown that could only read top-level
        fields would have nothing to offer.
    */
    string value_field;

    #! The record field supplying the display name
    /** Falls back to @ref value_field when absent, or when a record does not carry it - many APIs allow the
        human-readable name of an object to be unset.  A \c "." separated path reaches into a nested record,
        as for @ref value_field.
    */
    *string display_field;

    #! A description of the reference data
    *string desc;
}

#! An application's set of schema-derived actions
/** Owns the manifest, resolves every entry against the schema, and generates the
    @ref DataProvider::DataProviderActionInfo "DataProviderActionInfo" hashes an application registers.

    All validation happens in the constructor, so a manifest that has drifted from its schema - an operation
    that no longer exists, an overlay entry that no longer resolves, a reference-data entry naming an action
    that was removed - fails when the module is loaded rather than when a user opens the action.
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

        #! Other sets in the same application whose reference data this one may use
        list<RestSchemaActionSet> peers = ();
    }

    #! Resolves and validates an application's manifest against a schema
    /** @param schema the schema the actions are derived from
        @param manifest the operations the application exports
        @param refdata reference data declarations, keyed by reference data name
        @param peers other sets in the same application whose reference data this one may use

        An application whose API is split across documents needs one set per document, because a set binds
        one schema and the request URI takes its prefix from that schema's base path.  Its dropdowns do not
        split the same way: a Xero project names a **contact**, and contacts are declared in the accounting
        document while projects are declared in the projects one.  Naming the peer sets lets a field in this
        set declare reference data another set produces, and the listing then runs against the schema that
        actually declares it.

        Peers are given at construction rather than added later so that every declaration is still checked
        once, in one place.  That means they have to be built in dependency order, which is what makes a
        cycle between two sets impossible to express.

        @throw REST-SCHEMA-ACTION-ERROR the manifest is inconsistent, or an entry does not resolve
    */
    constructor(AbstractRestSchemaValidator schema, list<hash<RestSchemaActionInfo>> manifest,
            *hash<string, hash<RestSchemaReferenceDataInfo>> refdata, *softlist<RestSchemaActionSet> peers) {
        self.schema = schema;
        self.manifest = manifest;
        self.refdata = refdata ?? {};
        self.peers = peers ?? ();

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

    #! Returns every reference data declaration reachable from this set, including its peers'
    hash<string, hash<RestSchemaReferenceDataInfo>> getAllReferenceDataDecls() {
        hash<string, hash<RestSchemaReferenceDataInfo>> rv = {};
        foreach RestSchemaActionSet peer in (peers) {
            rv += peer.getAllReferenceDataDecls();
        }
        # this set's own declarations win, so a peer cannot shadow one
        return rv + refdata;
    }

    #! Returns the reference data declaration of the given name, from this set or a peer
    /** @param name the reference data name

        @return the declaration, or @ref nothing if no set in the application declares it
    */
    *hash<RestSchemaReferenceDataInfo> getReferenceDataDecl(string name) {
        if (*hash<RestSchemaReferenceDataInfo> rv = refdata{name}) {
            return rv;
        }
        foreach RestSchemaActionSet peer in (peers) {
            if (*hash<RestSchemaReferenceDataInfo> rv = peer.getReferenceDataDecl(name)) {
                return rv;
            }
        }
    }

    #! Returns the set that produces the given reference data, from this set or a peer
    /** The listing has to run against the schema that declares it, because the request URI takes its prefix
        from that schema's base path.

        @param name the reference data name

        @return the set, or @ref nothing if no set in the application declares it
    */
    *RestSchemaActionSet getReferenceDataSet(string name) {
        if (exists refdata{name}) {
            return self;
        }
        foreach RestSchemaActionSet peer in (peers) {
            if (*RestSchemaActionSet rv = peer.getReferenceDataSet(name)) {
                return rv;
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
            foreach string opt in (keys rd.options_from) {
                if (!exists flat{opt}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("reference data %y forwards a value into option "
                        "%y of action %y, which does not declare it; options: %y", i.key, opt, rd.action,
                        sort(keys flat));
                }
                if (exists rd.options{opt}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("reference data %y both fixes option %y and "
                        "forwards a value into it; one of the two would silently win", i.key, opt);
                }
            }
        }

        # every ref_data named by an overlay must be declared, or the dropdown would silently stay empty
        foreach hash<auto> i in (opmap.pairIterator()) {
            *hash<string, AbstractDataField> fields = i.value.getRequestFields();
            foreach hash<auto> f in (fields.pairIterator()) {
                *hash<auto> attr = f.value.getAttributes();
                foreach string key in (("ref_data", "element_ref_data")) {
                    if (!attr{key}.val()) {
                        continue;
                    }
                    *hash<RestSchemaReferenceDataInfo> rd = getReferenceDataDecl(attr{key});
                    if (!rd) {
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y option %y declares %s %y, which is "
                            "not declared in the reference data of this action set or any of its peers; "
                            "declared reference data: %y", i.key, f.key, key, attr{key},
                            sort(keys getAllReferenceDataDecls()));
                    }
                    # a forwarded value has to come from somewhere: an action offering a dropdown scoped by
                    # a value it does not itself have would show an empty list and no reason for it
                    foreach string from in (rd.options_from.iterator()) {
                        if (!exists fields{from}) {
                            throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y option %y declares %s %y, "
                                "which is scoped by option %y; action %y does not declare that option, so the "
                                "dropdown could never be populated; options: %y", i.key, f.key, key,
                                attr{key}, from, i.key, sort(keys fields));
                        }
                    }
                }
            }
        }
    }
}
}
