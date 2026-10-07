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
        of the listing action, and the value names the option of the <b>calling</b> action supplying it.

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

    #! Treat the value at records_path as one record, for identity endpoints
    bool single_record = False;

    #! Exact field values used to filter records locally, such as a model task
    *hash<auto> record_filter;

    #! Record filters supplied by calling action options, keyed by record field
    *hash<string, string> filter_from;

    #! A nested collection to expand from each matching record, such as a model's voices
    *softlist<string> children_path;

    #! The record field supplying the value
    /** A \c "." separated path reaches into a nested record: a Bitbucket workspace membership carries the
        identity as \c "user.uuid", not at the top level, and a dropdown that could only read top-level
        fields would have nothing to offer.
    */
    string value_field;

    #! Formats a resource name from record fields using literal {field} placeholders
    /** When set, replaces the value_field result. All placeholders must name scalar record fields.
        For example, projects/{projectId}/models/{id} preserves the resource's owning project.
    */
    *string value_template;

    #! The record field supplying the display name
    /** Falls back to @ref value_field when absent, or when a record does not carry it - many APIs allow the
        human-readable name of an object to be unset.  A \c "." separated path reaches into a nested record,
        as for @ref value_field.
    */
    *string display_field;

    #! Option sets to run the listing once for each of, in addition to @ref options
    /** Some APIs will not list a collection whole.  Klaviyo's campaign listing <b>requires</b> a filter on the
        message channel and accepts only \c equals there, so an account's campaigns are only reachable one
        channel at a time - and a dropdown offering half the account is worse than no dropdown at all.

        Each entry here is merged over @ref options and produces one call, and the results are concatenated
        and de-duplicated exactly as one call's are.  The application being replaced did this by hand, with
        two calls and a merge.

        A dropdown scoped by @ref options_from still resolves that first: a variant partitions the listing,
        it does not widen it.
    */
    *list<hash<auto>> option_sets;

    #! A reference-data listing whose values supply the partitions to enumerate
    /** For project-scoped API keys addressed globally by ID: enumerate projects, then list keys once
        per project. Set reference_option as well. Mutually exclusive with option_sets.
    */
    *string options_reference;

    #! The listing option receiving each value from options_reference
    *string reference_option;

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

        #! Host source declarations, keyed by host source name
        hash<string, hash<RestSchemaHostSourceInfo>> host_sources;
    }

    #! Resolves and validates an application's manifest against a schema
    /** @param schema the schema the actions are derived from
        @param manifest the operations the application exports
        @param refdata reference data declarations, keyed by reference data name
        @param peers other sets in the same application whose reference data this one may use
        @param host_sources host source declarations, keyed by host source name; declared by the set that
        exports the action resolving the host, and reachable from a peer exactly as reference data is

        An application whose API is split across documents needs one set per document, because a set binds
        one schema and the request URI takes its prefix from that schema's base path.  Its dropdowns do not
        split the same way: a Xero project names a <b>contact</b>, and contacts are declared in the accounting
        document while projects are declared in the projects one.  Naming the peer sets lets a field in this
        set declare reference data another set produces, and the listing then runs against the schema that
        actually declares it.

        Peers are given at construction rather than added later so that every declaration is still checked
        once, in one place.  That means they have to be built in dependency order, which is what makes a
        cycle between two sets impossible to express.

        @throw REST-SCHEMA-ACTION-ERROR the manifest is inconsistent, or an entry does not resolve
    */
    constructor(AbstractRestSchemaValidator schema, list<hash<RestSchemaActionInfo>> manifest,
            *hash<string, hash<RestSchemaReferenceDataInfo>> refdata, *softlist<RestSchemaActionSet> peers,
            *hash<string, hash<RestSchemaHostSourceInfo>> host_sources) {
        self.schema = schema;
        self.manifest = manifest;
        self.refdata = refdata ?? {};
        self.peers = peers ?? ();
        self.host_sources = host_sources ?? {};

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
            # an action with a host source gains an option from it, so the declaration has to be in hand
            # before the operation is built; peers are already set, so a data-plane set can reach the
            # declaration its control-plane peer makes
            *hash<RestSchemaHostSourceInfo> hs;
            if (info.host_from.val()) {
                hs = getHostSourceDecl(info.host_from);
                if (!hs) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y takes its host from host source %y, "
                        "which is not declared in this action set or any of its peers; declared host "
                        "sources: %y", info.action, info.host_from, sort(keys getAllHostSourceDecls()));
                }
            }
            opmap{info.action} = new RestSchemaActionOperation(schema, info, hs);
        }
        checkReferenceData();
        checkHostSources();
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

    #! Returns the host source declarations made by this set
    hash<string, hash<RestSchemaHostSourceInfo>> getHostSourceDecls() {
        return host_sources;
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
            "mutates_state": !RestSchemaReadOnlyHttpMethods{op.getMethod()},
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

    #! Returns every host source declaration reachable from this set, including its peers'
    hash<string, hash<RestSchemaHostSourceInfo>> getAllHostSourceDecls() {
        hash<string, hash<RestSchemaHostSourceInfo>> rv = {};
        foreach RestSchemaActionSet peer in (peers) {
            rv += peer.getAllHostSourceDecls();
        }
        # this set's own declarations win, so a peer cannot shadow one
        return rv + host_sources;
    }

    #! Returns the host source declaration of the given name, from this set or a peer
    /** @param name the host source name

        @return the declaration, or @ref nothing if no set in the application declares it
    */
    *hash<RestSchemaHostSourceInfo> getHostSourceDecl(string name) {
        if (*hash<RestSchemaHostSourceInfo> rv = host_sources{name}) {
            return rv;
        }
        foreach RestSchemaActionSet peer in (peers) {
            if (*hash<RestSchemaHostSourceInfo> rv = peer.getHostSourceDecl(name)) {
                return rv;
            }
        }
    }

    #! Returns the set that resolves the given host source, from this set or a peer
    /** An application whose data plane is described in a different document than its control plane declares
        the host source in the <b>control</b> set, because that is where the action resolving the host lives;
        the data-plane set names it as a peer.  The lookup has to run against the schema that declares it,
        for the same reason a reference-data listing does: the request URI takes its prefix from that
        schema's base path.

        @param name the host source name

        @return the set, or @ref nothing if no set in the application declares it
    */
    *RestSchemaActionSet getHostSourceSet(string name) {
        if (exists host_sources{name}) {
            return self;
        }
        foreach RestSchemaActionSet peer in (peers) {
            if (*RestSchemaActionSet rv = peer.getHostSourceSet(name)) {
                return rv;
            }
        }
    }

    #! Validates the host source declarations against the manifest and the overlays
    /** Everything a host source needs to work is checked here, so that a manifest that has drifted from its
        schema fails when the module is loaded rather than at the moment a request would be sent to the
        wrong host.

        @throw REST-SCHEMA-ACTION-ERROR a declaration is inconsistent, does not resolve, or omits the
        host suffixes that bound where the connection's credentials may be sent
    */
    private checkHostSources() {
        foreach hash<auto> i in (host_sources.pairIterator()) {
            hash<RestSchemaHostSourceInfo> hs = i.value;
            if (hs.name != i.key) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source is registered under key %y but names "
                    "itself %y", i.key, hs.name);
            }
            if (!exists opmap{hs.action}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y is resolved by action %y, which "
                    "the manifest does not export; exported actions: %y", i.key, hs.action, keys opmap);
            }
            if (!hs.host_field) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y declares no \"host_field\", so "
                    "there is nothing to read the host out of the lookup response with", i.key);
            }
            # not optional: the resolved host is where the connection's credentials are sent, and an
            # unbounded host source would make the connection URL stop bounding that
            if (!hs.allowed_host_suffixes) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y declares no "
                    "\"allowed_host_suffixes\"; a host taken from a response is a host the service chose, "
                    "and the request that follows carries the connection's credentials to it, so the "
                    "permitted hosts have to be stated", i.key);
            }
            if (hs.scheme != "http" && hs.scheme != "https") {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y declares scheme %y; only \"http\" "
                    "and \"https\" are supported", i.key, hs.scheme);
            }
            RestSchemaActionOperation lop = opmap{hs.action};
            # a lookup whose own host had to be resolved first could never be made
            if (lop.getInfo().host_from.val()) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y is resolved by action %y, which "
                    "itself takes its host from host source %y; the lookup could never be made", i.key,
                    hs.action, lop.getInfo().host_from);
            }
            hash<string, hash<RestSchemaFlatFieldInfo>> flat = lop.getFlatMap();
            string lookup_option = hs.lookup_option ?? hs.key_option;
            if (!exists flat{lookup_option}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y passes the lookup key to option %y "
                    "of action %y, which does not declare it; options: %y", i.key, lookup_option, hs.action,
                    sort(keys flat));
            }
            foreach string opt in (keys hs.options) {
                if (!exists flat{opt}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y passes option %y to action %y, "
                        "which does not declare it; options: %y", i.key, opt, hs.action, sort(keys flat));
                }
                if (opt == lookup_option) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("host source %y both fixes option %y and passes "
                        "the lookup key into it; one of the two would silently win", i.key, opt);
                }
            }
        }

        # the option the host is resolved from is synthesized when the operation is built, so every action
        # naming a host source has one; a manifest that dropped it with "ignore" would leave the host
        # unresolvable, and that is worth catching here rather than at the moment a request is made
        foreach hash<auto> i in (opmap.pairIterator()) {
            if (!i.value.getInfo().host_from.val()) {
                continue;
            }
            *string key_option = i.value.getHostKeyOption();
            if (!key_option.val()) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y takes its host from host source %y, but "
                    "publishes no option to resolve it from; an overlay that drops the key option leaves "
                    "the request with no host to go to", i.key, i.value.getInfo().host_from);
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
            if (rd.options_reference.val().toBool() != rd.reference_option.val().toBool()) {
                throw "REST-SCHEMA-ACTION-ERROR", "options_reference and reference_option must be set together";
            }
            if (rd.options_reference.val()) {
                if (rd.option_sets || !opmap{rd.action}.getFlatMap().hasKey(rd.reference_option)
                        || rd.options.hasKey(rd.reference_option) || rd.options_from.hasKey(rd.reference_option)) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("invalid reference partition option for %y", i.key);
                }
                hash<string, bool> seen = {i.key: True};
                *string next = rd.options_reference;
                while (next.val()) {
                    if (seen{next}) {
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("cyclic reference-data partitions at %y", next);
                    }
                    seen{next} = True;
                    *hash<RestSchemaReferenceDataInfo> source = getReferenceDataDecl(next);
                    if (!source) {
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("unknown reference-data partition %y", next);
                    }
                    next = source.options_reference;
                }
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
                    list<string> sources = map $1, rd.options_from.iterator();
                    sources += map $1, rd.filter_from.iterator();
                    foreach string from in (sources) {
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
