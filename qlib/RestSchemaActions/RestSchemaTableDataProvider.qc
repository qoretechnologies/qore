# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaTableDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! A record-based table backed by actions the application's manifest already exports
/** A record table is not a second implementation of the API.  Its collection, its single-record read and its
    writes are **actions from the manifest**, executed through
    @ref RestSchemaActions::RestSchemaActionDataProvider "RestSchemaActionDataProvider", so the paths, the
    request contracts, the percent-encoding, the wire codecs and the published response shapes are the
    schema's and are described in exactly one place.

    What a table adds is what a schema cannot say:
    @ref RestSchemaActions::RestSchemaRecordTableInfo "RestSchemaRecordTableInfo" names the collection behind
    the table, the field carrying a record's identity, how the listing pages, and which predicates the API can
    evaluate for itself.

    @section restschematable_search How a Search Is Answered

    Every table declares
    @ref DataProvider::DataProviderBaseInfo::supports_search_expressions "supports_search_expressions", so a
    search arrives as an expression tree whatever the caller wrote, and is answered in three steps:

    -# @ref RestSchemaActions::RestSchemaRecordFilter "RestSchemaRecordFilter" splits the tree into the part
       the API can evaluate - query arguments, or a rendered filter expression where the API has a query
       language - and the part left over.
    -# The API's part is sent with every page of the listing.
    -# The part left over is evaluated against each record as it arrives, by
       @ref DataProvider::DefaultRecordIterator "DefaultRecordIterator", over a lazy
       @ref RestSchemaActions::RestSchemaRecordPageIterator "RestSchemaRecordPageIterator".

    The order of the last two is the whole contract.  A \c limit counts **matching** records, so it has to be
    applied after the filter and not before it; because the page iterator is lazy, applying it afterwards
    reads exactly as many pages as the search needs and no more.  Truncating the fetch at the limit first and
    filtering afterwards is the way to get this wrong, and it under-returns silently: asked for 10 closed
    issues out of a repository holding 10 closed among 100, it answers with one.

    @section restschematable_native What "Native Search" Means Here

    @ref DataProvider::DataProviderBaseInfo::supports_native_search "supports_native_search" is declared only by a
    table whose API takes a filter **expression** - Xero's \c where, Bitbucket's BBQL - and can therefore
    evaluate a whole tree.  A table that can only narrow a listing with a handful of query arguments declares
    it @ref False, however many arguments it has, because a caller reading that flag is asking whether the
    search happens on the server and for such a table the answer is "partly, and which part depends on what
    you asked".  @ref DataProvider::DataProviderExpressionBaseInfo::supports_pushdown "supports_pushdown" is where
    that per-operator detail is published.

    @section restschematable_write Updating and Deleting

    A REST API updates and deletes one record at a time, by identity, so
    @ref DataProvider::AbstractDataProvider::updateRecords() "updateRecords()" and
    @ref DataProvider::AbstractDataProvider::deleteRecords() "deleteRecords()" first work out which records
    the predicate matches and then act on each of them.

    Where the predicate is exactly an equality on the table's identity field, that search is skipped and the
    write is issued directly, because the caller has already said which record it means.  Otherwise the
    matching records are **collected before the first write is issued**, rather than being written as the
    collection is walked: a paged collection that is being modified while it is read returns a different set
    of pages, and records would be skipped or repeated.
*/
public class RestSchemaTableDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Search options every table accepts, beyond the collection's own
        const TableSearchOptions = AbstractDataProvider::GenericRecordSearchOptions{
            ("columns", "limit", "offset", "max_records", "requires_result")
        };
    }

    private {
        #! The action set the table's actions belong to
        RestSchemaActionSet action_set;

        #! The table declaration
        hash<RestSchemaRecordTableInfo> table;

        #! The REST client used for API calls
        *AbstractRestClient rest;

        #! Shared, synchronized host router for all requests under this root
        *RestSchemaHostRouter router;

        #! The predicate splitter
        RestSchemaRecordFilter filter;

        #! The record type, built on first use
        *hash<string, AbstractDataField> record_type;

        #! Guards building the record type
        Mutex record_type_lock();
    }

    #! Creates the object from the arguments
    /** @param action_set the action set the table's actions belong to
        @param table the table declaration
        @param rest the REST client to use for API calls
        @param router the host router shared with the application root

        @throw REST-SCHEMA-RECORD-ERROR the table names an action the manifest does not export, an option an
        action does not declare, or a projection without the conversions it needs
    */
    constructor(RestSchemaActionSet action_set, hash<RestSchemaRecordTableInfo> table,
            *AbstractRestClient rest, *RestSchemaHostRouter router) {
        self.action_set = action_set;
        self.table = table;
        self.rest = rest;
        self.router = router ?? (rest ? new RestSchemaHostRouter(rest) : NOTHING);
        filter = new RestSchemaRecordFilter(table);
        RestSchemaTableDataProvider::check(action_set, table);
    }

    #! Validates a table declaration against the action set it is built on
    /** Called from the constructor, and from an application's test suite through
        @ref RestSchemaActions::RestSchemaTablesDataProvider "RestSchemaTablesDataProvider", so that a table
        naming an action that has been withdrawn from the manifest fails at once rather than when somebody
        opens it.

        @param action_set the action set the table's actions belong to
        @param table the table declaration

        @throw REST-SCHEMA-RECORD-ERROR the declaration is inconsistent with the manifest
    */
    static check(RestSchemaActionSet action_set, hash<RestSchemaRecordTableInfo> table) {
        on_error rethrow $1.err, sprintf("%s (while checking record table %y)", $1.desc, table.name);
        if (table.query_listing) {
            if (!table.query) {
                throw "REST-SCHEMA-RECORD-ERROR", "query_listing requires a query declaration";
            }
            hash<RestSchemaRecordTableInfo> alternate = cast<hash<RestSchemaRecordTableInfo>>(
                (table - "query_listing") + table.query_listing);
            RestSchemaTableDataProvider::check(action_set, alternate);
            # The query option belongs to the alternate listing, not the unfiltered ID listing.
            table -= "query";
        }
        if (table.hydrate_list && (!table.get_action.val()
                || !inlist(table.get_action, table.id_list_actions))) {
            throw "REST-SCHEMA-RECORD-ERROR", "hydrate_list requires a get action accepting a list of IDs";
        }
        hash<string, hash<RestSchemaFlatFieldInfo>> list_flat =
            action_set.getOperationEx(table.list_action).getFlatMap();
        foreach hash<auto> i in (("get_action": table.get_action, "create_action": table.create_action,
                "update_action": table.update_action, "delete_action": table.delete_action).pairIterator()) {
            if (i.value.val()) {
                action_set.getOperationEx(i.value);
            }
        }
        if (table.create_scope_options) {
            if (!table.create_action.val()) {
                throw "REST-SCHEMA-RECORD-ERROR", "create_scope_options requires a create action";
            }
            hash<string, hash<RestSchemaFlatFieldInfo>> flat =
                action_set.getOperationEx(table.create_action).getFlatMap();
            foreach string name in (table.create_scope_options) {
                if (!flat.hasKey(name)) {
                    throw "REST-SCHEMA-RECORD-ERROR", sprintf("create scope %y is not an option of %y",
                        name, table.create_action);
                }
            }
        }
        foreach hash<auto> entry in (table.action_id_options.pairIterator()) {
            if (!inlist(entry.key, (table.get_action, table.update_action, table.delete_action))
                    || !action_set.getOperationEx(entry.key).getFlatMap().hasKey(entry.value)) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("invalid identity option %y for action %y",
                    entry.value, entry.key);
            }
        }
        foreach string action in (table.id_list_actions) {
            if (!inlist(action, (table.get_action, table.update_action, table.delete_action))
                    || RestSchemaTableDataProvider::getIdParts(table).size() != 1) {
                throw "REST-SCHEMA-RECORD-ERROR", "list identities require a single identity and a record action";
            }
        }
        foreach hash<auto> entry in (table.write_list_options.pairIterator()) {
            if (!inlist(entry.key, (table.create_action, table.update_action))
                    || !action_set.getOperationEx(entry.key).getFlatMap().hasKey(entry.value)) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("invalid write list option %y for action %y",
                    entry.value, entry.key);
            }
        }
        if (table.check_write_identity && !table.get_action.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", "check_write_identity requires a get action";
        }
        # every option the table sends to the listing has to be one the listing declares, or it would be
        # dropped silently and the search would answer with the unfiltered collection
        hash<string, bool> checked = {};
        foreach hash<RestSchemaRecordFilterInfo> f in (table.filters) {
            RestSchemaTableDataProvider::checkOption(table, list_flat, f.option,
                sprintf("filter on %y %y", f.field, f.operator));
            string key = RestSchemaRecordFilter::ruleKey(f.field, f.operator);
            if (checked{key}) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("two filters are declared for field %y operator "
                    "%y; one of them would silently never be used", f.field, f.operator);
            }
            checked{key} = True;
        }
        if (exists table.query) {
            RestSchemaTableDataProvider::checkOption(table, list_flat, table.query.option,
                "the filter expression");
            # the built-in rendering has no way to spell a value without this; a syntax that renders the
            # whole expression itself spells its own values and does not need it
            if (!exists table.query.syntax.render_value && !exists table.query.syntax.render_query) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("the %y query language declares neither "
                    "\"render_value\" nor \"render_query\", so no predicate could be rendered into it",
                    table.query.syntax.name);
            }
            # "field_ops" is the whole statement of what the API filters on where it is declared, so a
            # table declaring both would have one of them silently ignored
            if (exists table.query.fields && exists table.query.field_ops) {
                throw "REST-SCHEMA-RECORD-ERROR", "a table's query declares both \"fields\" and "
                    "\"field_ops\"; \"field_ops\" states which operators each field accepts and replaces "
                    "\"fields\" entirely, so declaring both would leave one of them unused";
            }
        }
        foreach string opt in (table.scope_options) {
            RestSchemaTableDataProvider::checkOption(table, list_flat, opt, "a scope option");
        }
        foreach string opt in (table.search_options) {
            RestSchemaTableDataProvider::checkOption(table, list_flat, opt, "a search option");
        }
        hash<RestSchemaRecordPagingInfo> p = table.paging;
        if (!exists RestSchemaRecordPagingModes{p.mode}) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("paging mode %y is unknown; known modes: %y", p.mode,
                sort(keys RestSchemaRecordPagingModes));
        }
        if (p.mode != RSRP_NONE) {
            if (!p.page_option.val()) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("paging mode %y needs a \"page_option\", or a "
                    "search would never read past the first page", p.mode);
            }
            RestSchemaTableDataProvider::checkOption(table, list_flat, p.page_option, "the paging option");
        }
        if (p.size_option.val()) {
            RestSchemaTableDataProvider::checkOption(table, list_flat, p.size_option, "the page size option");
        }
        if (p.mode == RSRP_CURSOR && !exists p.next_path) {
            throw "REST-SCHEMA-RECORD-ERROR", "cursor paging needs a \"next_path\" saying where the response "
                "reports the cursor of the next page";
        }
        # a write field mapping naming an option no write action declares would be dropped exactly as the
        # unmapped field would have been, which is the defect it exists to prevent
        if (exists table.write_field_map) {
            hash<string, bool> write_opts = {};
            foreach string a in ((table.create_action, table.update_action)) {
                if (a.val()) {
                    map write_opts{$1} = True, keys action_set.getOperationEx(a).getFlatMap();
                }
            }
            foreach hash<auto> i in (table.write_field_map.pairIterator()) {
                if (!write_opts{i.value}) {
                    throw "REST-SCHEMA-RECORD-ERROR", sprintf("the write field mapping sends record field "
                        "%y as option %y, which neither the create nor the update action declares; known "
                        "write options: %y", i.key, i.value, sort(keys write_opts));
                }
            }
        }
        if (exists table.to_record != exists table.to_wire) {
            throw "REST-SCHEMA-RECORD-ERROR", "a table converting its records needs both \"to_record\" and "
                "\"to_wire\"; with only one of them its records could be read but not written back, or "
                "written but not read";
        }
    }

    #! Checks that the table has a record type
    /** Not part of @ref check(), because deriving a record type parses the listing's response and a
        declaration check should not.  It is called the first time the type is needed, and an application's
        test suite reaches it by asking each table for its record type.

        A collection whose element is a union - Paddle describes a report as one of seven models - has no
        single set of fields to derive, and a table over it would publish \c has_record with no columns.
        Naming the columns explicitly is the answer, and being told so is better than an empty table.

        @throw REST-SCHEMA-RECORD-ERROR no record type could be derived and none was declared
    */
    private *hash<string, AbstractDataField> checkRecordType(*hash<string, AbstractDataField> rt) {
        if (rt) {
            return rt;
        }
        throw "REST-SCHEMA-RECORD-ERROR", sprintf("no record type could be derived for table %y from the "
            "response of listing action %y, and the table declares none; a collection whose element type is "
            "a union or an open hash has no fields to derive, and such a table has to name its columns with "
            "\"record_type\"", table.name, table.list_action);
    }

    #! Checks that every path a table reads or writes a record through resolves against the response
    /** A path that does not resolve fails **silently**: a single read whose record path names a wrapper
        the action already unwrapped reads one record as nothing at all, and no test that only searches
        will ever notice.  That is what Paddle's tables did - the manifest unwraps \c data on a single
        read, so unwrapping it again in the table left \c readOne() and \c searchSingleRecord() returning
        nothing - and it is the same class of error as a filter naming a field the record does not have.

        Only a response that describes its fields can be checked; a union or an open hash describes none,
        and an unverifiable path is left alone rather than reported on a guess.

        Called from @ref RestSchemaActions::RestSchemaTablesDataProvider::checkTables() "checkTables()"
        rather than from @ref check(), because it parses the actions' responses.

        @param action_set the action set the table's actions belong to
        @param table the table declaration

        @throw REST-SCHEMA-RECORD-ERROR a declared path does not resolve against the response it reads
    */
    static checkRecordPaths(RestSchemaActionSet action_set, hash<RestSchemaRecordTableInfo> table) {
        if (table.query_listing) {
            RestSchemaTableDataProvider::checkRecordPaths(action_set,
                cast<hash<RestSchemaRecordTableInfo>>((table - "query_listing") + table.query_listing));
        }
        list<string> bad = ();
        RestSchemaTableDataProvider::checkPath(action_set, table.list_action, table.records_path,
            "records_path", \bad);
        if (table.get_action.val()) {
            RestSchemaTableDataProvider::checkPath(action_set, table.get_action, table.record_path,
                "record_path", \bad);
        }
        foreach *string action in ((table.create_action, table.update_action)) {
            if (action.val()) {
                RestSchemaTableDataProvider::checkPath(action_set, action, table.write_record_path,
                    "write_record_path", \bad);
            }
        }
        if (bad) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y: %y; the action publishes the response it "
                "unwraps, so a path naming a wrapper it has already removed reads nothing at all",
                table.name, bad);
        }
    }

    #! Adds a message to \a bad for each element of a path the action's response does not declare
    private static checkPath(RestSchemaActionSet action_set, string action, *softlist<string> path,
            string what, reference<list<string>> bad) {
        if (!path) {
            return;
        }
        *AbstractDataProviderType type = action_set.getOperationEx(action).getResponseType();
        list<string> seen = ();
        foreach string name in (path) {
            *hash<string, AbstractDataField> fields = RestSchemaTableDataProvider::getTypeFields(type);
            if (!fields) {
                # a union or an open hash declares no fields, so there is nothing to check against
                return;
            }
            *AbstractDataField field = fields{name};
            if (!field) {
                bad += sprintf("%s %y names %y, which the response of action %y does not declare%s; "
                    "it declares %y", what, path, name, action,
                    seen ? sprintf(" under %y", seen) : "", keys fields);
                return;
            }
            seen += name;
            type = field.getType();
        }
    }

    #! Returns the fields a type describes, reading through a collection to the type of its elements
    /** The element type is preferred and the type's own fields are the fallback, because a description does
        not always say that a collection is a collection: GitLab declares the 200 response of every listing
        it publishes as a **single** entity rather than an array of them, though the body it returns is an
        array.  Reading the entity's fields is right in both cases - it is the record either way - and it is
        the only thing that works for the second.

        A hash type reports its **value** type as its element type, which describes no fields, so the
        fallback is what answers for one of those.

        @param type the type to read

        @return the fields the type describes, or @ref NOTHING when it describes none - a union, an open
        hash or a scalar
    */
    private static *hash<string, AbstractDataField> getTypeFields(*AbstractDataProviderType type) {
        if (!type) {
            return;
        }
        if (*AbstractDataProviderType etype = type.getElementType()) {
            if (*hash<string, AbstractDataField> efields = etype.getFields()) {
                return efields;
            }
        }
        return type.getFields();
    }

    #! Checks that the listing action declares an option the table sends it
    private static checkOption(hash<RestSchemaRecordTableInfo> table,
            hash<string, hash<RestSchemaFlatFieldInfo>> flat, string option, string what) {
        if (!exists flat{option}) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("%s names option %y, which listing action %y does not "
                "declare; options: %y", what, option, table.list_action, sort(keys flat));
        }
    }

    #! Returns the table name
    string getName() {
        return table.name;
    }

    #! Returns the table display name
    string getDisplayName() {
        return table.display_name;
    }

    #! Returns the table short description
    *string getShortDesc() {
        return table.short_desc;
    }

    #! Returns the table description
    *string getDesc() {
        return table.desc;
    }

    #! Returns the table declaration
    hash<RestSchemaRecordTableInfo> getTableInfo() {
        return table;
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        if (exists table.record_type) {
            return addExtraColumns(table.record_type);
        }
        AutoLock al(record_type_lock);
        if (!exists record_type) {
            record_type = checkRecordType(addExtraColumns(deriveRecordType()));
        }
        return record_type;
    }

    #! Returns the record fields accepted by the create action
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> create_options) {
        return getWriteRecordType(table.create_action, False);
    }

    #! Returns the fields accepted by a partial update, in the table's own spelling
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return getWriteRecordType(table.update_action, True);
    }

    #! Derives write fields from the request rather than publishing server-assigned response fields
    private *hash<string, AbstractDataField> getWriteRecordType(*string action, bool update) {
        if (!action.val()) {
            return;
        }
        *hash<string, AbstractDataField> fields;
        if (table.to_wire) {
            fields = getRecordTypeImpl(NOTHING);
        } else {
            fields = action_set.getOperationEx(action).getRequestFields();
            if (*string wrapper = getWriteListOption(action)) {
                fields = RestSchemaTableDataProvider::getTypeFields(fields{wrapper}.getType());
            } else {
                fields -= table.scope_options;
                fields -= table.create_scope_options;
                foreach hash<auto> mapping in (table.write_field_map.pairIterator()) {
                    if (fields{mapping.value}) {
                        AbstractDataField field = fields{mapping.value};
                        fields -= mapping.value;
                        fields{mapping.key} = field.getRenamedField(mapping.key);
                    }
                }
                if (update) {
                    fields -= keys RestSchemaTableDataProvider::getIdParts(table);
                    fields -= table.action_id_options{action};
                }
            }
        }
        fields -= table.read_only_fields;
        if (update) {
            fields -= table.id_field;
            fields = map {$1.key: $1.value.getOrNothingType()}, fields.pairIterator();
        }
        return fields;
    }

    #! Adds the columns the table declares on top of the ones the schema describes
    /** @see @ref RestSchemaActions::RestSchemaRecordTableInfo::extra_columns "extra_columns"

        Extra columns never rescue a table whose type could not be derived at all: a table publishing one
        hand-declared column and none of the record's own is worse than one that said it could not be built.
    */
    private *hash<string, AbstractDataField> addExtraColumns(*hash<string, AbstractDataField> rt) {
        if (!rt || !table.extra_columns) {
            return rt;
        }
        map rt{$1.key} = $1.value, table.extra_columns.pairIterator();
        return rt;
    }

    #! Returns the record type derived from the actions that read the collection
    /** The schema's own description of the record is what the table publishes, so a table that has not
        declared a projection of its own cannot drift from the contract its actions use.

        **A listing returns a projection; reading one record returns the record.**  A vendor describes both,
        and the two rarely agree: GitLab describes a project with 24 fields in its listing and 161 in its
        single read.  The richer of the two is therefore the one published, because an under-declared column
        is not cosmetic - the framework rejects a search on a column the table does not declare before the
        table is reached, so those columns cannot be searched at all.

        A column only the other action declares is still a column, and is added rather than lost.  Where
        neither yields anything - Stripe describes a single customer read as \c anyOf a customer and a
        deleted customer, a union with no fields of its own - the one that did wins, and where neither did
        the table is told to name its columns.
    */
    private *hash<string, AbstractDataField> deriveRecordType() {
        *hash<string, AbstractDataField> from_list = getResponseFields(table.list_action, table.records_path);
        *hash<string, AbstractDataField> from_get = table.get_action.val()
            ? getResponseFields(table.get_action, table.record_path)
            : NOTHING;
        bool get_wins = from_get.size() > from_list.size();
        *hash<string, AbstractDataField> rv = get_wins ? from_get : from_list;
        if (!rv) {
            return;
        }
        # the fields of the other one, in the order the richer one does not already have them
        *hash<string, AbstractDataField> other = get_wins ? from_list : from_get;
        map rv{$1.key} = $1.value, (other ?? {}).pairIterator(), !rv.hasKey($1.key);
        return rv;
    }

    #! Returns the record fields an action's response declares at the given path
    /** @param action the action whose success response describes the record
        @param path the path of field names leading from the response to the record or the records

        @return the record's fields, or @ref NOTHING when the response does not describe them
    */
    private *hash<string, AbstractDataField> getResponseFields(string action, *softlist<string> path) {
        *AbstractDataProviderType type = action_set.getOperationEx(action).getResponseType();
        foreach string name in (path) {
            *hash<string, AbstractDataField> fields = RestSchemaTableDataProvider::getTypeFields(type);
            *AbstractDataField field = fields{name};
            if (!field) {
                # the response does not declare the path the table reads its records from; there is nothing
                # to derive here, and the other action - or the table - has to supply the columns
                return;
            }
            type = field.getType();
        }
        return RestSchemaTableDataProvider::getTypeFields(type);
    }

    #! Returns an iterator over the records matching the search criteria
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # the caller's own listing options are worked out first, so that a predicate cannot overwrite one:
        # the two together mean the intersection, and the predicate reaches the client side instead
        hash<auto> extra = getExtraSearchOptions(search_options);
        hash<RestSchemaRecordFilterSplit> split = filter.split(where_cond,
            getExplicitSearchOptions(search_options));
        if (exists split.id && table.get_action.val()) {
            # the caller named one record; reading the collection to find it would be wasteful and, on a
            # collection the service lags behind its own writes, wrong.
            #
            # the predicate is still applied to what comes back, because naming a record is not the whole of
            # what a caller may have asked: "id == 5 and state == open" names one record AND asks a question
            # about it, and answering only the first half would return a record the search excluded.  The
            # identity equality is in the residual too and simply matches
            *hash<auto> rec = readOne(split.id, search_options);
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(rec ? (rec,) : ()),
                split.client, search_options, getRecordTypeImpl(search_options));
        }
        hash<RestSchemaRecordTableInfo> listing = table;
        if (table.query_listing && exists split.options{table.query.option}) {
            listing = cast<hash<RestSchemaRecordTableInfo>>(table + table.query_listing);
        }
        *code<list<hash<auto>>(list<hash<auto>>)> hydrate;
        if (listing.hydrate_list) {
            hydrate = list<hash<auto>> sub (list<hash<auto>> ids) {
                return hydrateRecords(ids, search_options);
            };
        }
        RestSchemaRecordPageIterator pages = new RestSchemaRecordPageIterator(getLister(listing.list_action),
            listing, getScope(search_options, "search", listing.list_action) + extra + split.options, hydrate);
        return new DefaultRecordIterator<hash<auto>>(pages, split.client, search_options,
            getRecordTypeImpl(search_options));
    }

    #! Creates a record and returns it as the table presents it
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        if (!table.create_action.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y cannot create records; the application "
                "exports no create action for it", table.name);
        }
        auto resp = getLister(table.create_action).doRequest(getScope(create_options, "create", table.create_action)
            + getWriteBody(table.create_action, rec));
        return table.write_returns_record ? readWriteResponse(resp) : rec;
    }

    #! Updates the records matching the search criteria and returns how many were updated
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        list<hash<auto>> ids = getTargetIds(where_cond, search_options, "update");
        int rv = 0;
        foreach hash<auto> id in (ids) {
            if (updateOne(id, set, search_options)) {
                ++rv;
            }
        }
        return rv;
    }

    #! Updates the one record matching the search criteria
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        list<hash<auto>> ids = getTargetIds(where_cond, search_options, "update");
        if (ids.size() > 1) {
            throw "UPDATE-SINGLE-RECORD-ERROR", sprintf("%d records in table %y match the search criteria; "
                "no record has been updated", ids.size(), table.name);
        }
        if (!ids) {
            return False;
        }
        return updateOne(ids[0], set, search_options);
    }

    #! Deletes the records matching the search criteria and returns how many were deleted
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        if (!table.delete_action.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y cannot delete records; the application "
                "exports no delete action for it", table.name);
        }
        list<hash<auto>> ids = getTargetIds(where_cond, search_options, "delete");
        int rv = 0;
        foreach hash<auto> id in (ids) {
            # a predicate naming an identity is written straight through without reading first, so a
            # record that is not there is discovered here; it matched nothing and is not counted
            try {
                getLister(table.delete_action).doRequest(getScope(search_options, "delete", table.delete_action)
                    + getActionId(table.delete_action, id));
            } catch (hash<ExceptionInfo> ex) {
                if (ex.arg.status_code == 404) {
                    continue;
                }
                rethrow;
            }
            ++rv;
        }
        return rv;
    }

    #! Returns the identities of the records a write applies to
    /** A predicate that is exactly an equality on the identity field names the record directly; anything else
        is resolved by searching, and the matches are collected in full before the first write is issued.

        @param where_cond the predicate
        @param search_options the search options
        @param what the operation name, for error messages

        \c max_records is enforced by the search itself, which is what makes it a guard worth having here:
        it stops before the identities are collected, and therefore before the first write.

        @throw REST-SCHEMA-RECORD-ERROR the predicate is empty, so the write would apply to the whole table
        @throw MAX-RECORDS-EXCEEDED more records match than \c max_records allows
    */
    private list<hash<auto>> getTargetIds(*hash<auto> where_cond, *hash<auto> search_options, string what) {
        if (!where_cond) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("a %s on table %y needs search criteria; without them "
                "it would apply to every record in the table", what, table.name);
        }
        hash<RestSchemaRecordFilterSplit> split = filter.split(where_cond);
        if (exists split.id) {
            if (table.check_write_identity && !readOne(split.id, search_options)) {
                return ();
            }
            return (split.id,);
        }
        list<hash<auto>> rv = ();
        # collected in full before the first write: a paged collection modified while it is read returns a
        # different set of pages, and records would be skipped or repeated
        AbstractDataProviderRecordIterator<hash<auto>> i = searchRecordsImpl(where_cond,
            (search_options ?? {}) - ("columns", "limit", "offset"));
        while (i.next()) {
            rv += getRecordId(i.getValue(), what);
        }
        return rv;
    }

    #! Updates one record by its identity
    /** @param id the options addressing the record
        @param set the values to change
        @param search_options the search options, which carry the collection's scope
    */
    private bool updateOne(hash<auto> id, hash<auto> set, *hash<auto> search_options) {
        if (!table.update_action.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y cannot update records; the application "
                "exports no update action for it", table.name);
        }
        # a predicate naming an identity is written straight through without reading first, so a record
        # that is not there is discovered here; it matched nothing, and saying so is the honest answer
        try {
            if (getWriteListOption(table.update_action).val()) {
                # a service whose writes take a list of whole documents identifies the one being changed
                # inside the document, not beside it: there is no path variable to put it in, and the write
                # is the same operation as a create with the identity present
                getLister(table.update_action).doRequest(getScope(search_options, "update", table.update_action)
                    + getWriteBody(table.update_action, set + {table.id_field: id.firstValue()}));
            } else {
                getLister(table.update_action).doRequest(getScope(search_options, "update", table.update_action)
                    + getWriteBody(table.update_action, set) + getActionId(table.update_action, id));
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.arg.status_code == 404) {
                return False;
            }
            rethrow;
        }
        return True;
    }

    #! Reads one record by its identity
    /** A search that names a record the service does not have matches **nothing**; it is not an error.
        Every API in this family answers a single read of an unknown identity with a 404, so that answer is
        turned into an empty result rather than an exception - otherwise a search for a record that has been
        deleted, or an @ref updateRecords() naming one, would throw where it should report that nothing
        matched.

        Any other failure is still raised: a 403 is a permission problem and a 400 is a bad request, and
        neither means "no such record".

        @param id the options addressing the record
        @param search_options the search options, which carry the collection's scope
    */
    private *hash<auto> readOne(hash<auto> id, *hash<auto> search_options) {
        auto resp;
        try {
            resp = getLister(table.get_action).doRequest(getScope(search_options, "search", table.get_action)
                + getActionId(table.get_action, id));
        } catch (hash<ExceptionInfo> ex) {
            if (ex.arg.status_code == 404) {
                return;
            }
            rethrow;
        }
        auto rec = RestSchemaRecordPageIterator::followPath(resp, table.record_path);
        if (table.record_is_map) {
            if (!exists rec) {
                return;
            }
            if (rec.typeCode() != NT_HASH) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y expected an identity map, got %y",
                    table.name, rec.type());
            }
            rec = rec{id.firstValue().toString()};
            if (exists rec && rec.typeCode() != NT_HASH) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y expected a record in its identity map, got %y",
                    table.name, rec.type());
            }
        }
        if (rec.typeCode() == NT_LIST) {
            rec = rec[0];
        }
        if (rec.typeCode() != NT_HASH) {
            return;
        }
        return table.to_record ? table.to_record(rec) : rec;
    }

    #! Returns the record in a create or update response, as the table presents it
    private *hash<auto> readWriteResponse(auto resp) {
        auto rec = RestSchemaRecordPageIterator::followPath(resp,
            table.write_record_path ?? table.record_path);
        if (rec.typeCode() == NT_LIST) {
            # a service whose writes take and return a list of documents answers a single write with a list
            # of one
            rec = rec[0];
        }
        if (rec.typeCode() != NT_HASH) {
            return;
        }
        return table.to_record ? table.to_record(rec) : rec;
    }

    #! Returns a record's identity as the options carrying it, keyed by option name
    /** Normalises the two ways a table may declare an identity into the one the write path uses: a
        single-valued identity is one entry, and a composite one is what
        @ref RestSchemaRecordTableInfo::id_parts "id_parts" declares.

        @param table the table declaration
    */
    static hash<string, string> getIdParts(hash<RestSchemaRecordTableInfo> table) {
        if (exists table.id_parts) {
            return table.id_parts;
        }
        return {(table.id_option ?? table.id_field): table.id_field};
    }

    #! Returns the identity of a record, as the options addressing it
    /** @param rec the record, in the table's shape
        @param what the operation name, for the error message

        @throw REST-SCHEMA-RECORD-ERROR the record does not carry a part of its own identity
    */
    private hash<auto> getRecordId(hash<auto> rec, string what) {
        # "auto!" because a composite identity is a mix of names and numbers, and a hash narrowed to the
        # type of its first part would reject the rest
        hash<auto!> rv = {};
        foreach hash<auto> i in (RestSchemaTableDataProvider::getIdParts(table).pairIterator()) {
            auto value = RestSchemaRecordPageIterator::followPath(rec, split(".", i.value));
            if (!exists value) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("a record matched by the %s on table %y carries "
                    "no %y, so it cannot be addressed", what, table.name, i.value);
            }
            rv{i.key} = value;
        }
        return rv;
    }

    #! Returns the options a write sends, from a record in the table's shape
    /** A record is read and written in the same shape, and a record type therefore describes columns a write
        cannot carry: an identity and its timestamps are the service's to assign, and a column the service
        will accept on an update it will not always accept on a create.

        So a write sends the columns the write action **declares**, and nothing else.  A mapper hands over a
        whole row, and the alternative to narrowing it is a request the schema rejects for carrying a value
        the caller never chose to send.  @ref RestSchemaRecordTableInfo::read_only_fields is the same rule
        stated by hand, for a column the action does declare but the service will not accept.

        @param action the write action the body is for
        @param rec the record, in the table's shape
    */
    private hash<auto> getWriteBody(string action, hash<auto> rec) {
        hash<auto> wire = table.to_wire ? table.to_wire(rec) : rec;
        wire -= table.read_only_fields;
        # a record arrives from a mapper with every column present, and a value that was never set is not a
        # value the service should be told about
        # "auto!" because a record's values are of every type, and a hash narrowed to the first one would
        # reject the rest
        hash<auto!> rv = {};
        map rv{$1.key} = $1.value, wire.pairIterator(), exists $1.value;
        *string list_option = getWriteListOption(action);
        if (list_option.val()) {
            # the whole record travels as one value, so its columns are not action options and cannot be
            # checked against them
            return {list_option: (rv,)};
        }
        # a column whose option is spelled differently from the record field would be dropped by the
        # narrowing below, and the write would report success having changed nothing
        foreach hash<auto> i in (table.write_field_map.pairIterator()) {
            if (rv.hasKey(i.key)) {
                auto value = rv{i.key};
                rv -= i.key;
                rv{i.value} = value;
            }
        }
        hash<auto> body = rv{keys action_set.getOperationEx(action).getFlatMap()};
        # a write that supplied values and carries none of them after the narrowing has had every one of
        # them dropped, which is the defect write_field_map exists to prevent: the columns are named one
        # way on the record and another way on the action, and nothing else would say so.
        #
        # This is caught here rather than left to the schema, because whether the schema catches it is an
        # accident of the operation: a body the vendor declares required rejects the request, and one it
        # declares optional - or one whose properties are all optional, which is a valid empty object -
        # is accepted, and the update then reports success having changed no value at all.
        if (rv && !body) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("the write to table %y supplied %y, and the %y "
                "action declares none of them, so the request would carry no value at all; the action's "
                "options are %y - declare the pairs in \"write_field_map\" if the table's columns are "
                "spelled differently from them", table.name, (keys rv), action,
                sort(keys action_set.getOperationEx(action).getFlatMap()));
        }
        return body;
    }

    #! Returns the options identifying the collection the operation works in
    /** @param options the search, create or write options
        @param what the operation name, for the error message
        @param action the target action; scopes it does not accept are omitted

        @throw REST-SCHEMA-RECORD-ERROR an option identifying the collection is missing
    */
    private hash<auto> getScope(*hash<auto> options, string what, string action) {
        hash<auto!> rv = {};
        *softlist<string> names = what == "create"
            ? (table.create_scope_options ?? table.scope_options) : table.scope_options;
        hash<string, AbstractDataField> fields = action_set.getOperationEx(action).getRequestFields();
        hash<string, hash<DataProviderOptionInfo>> declared = getListingOptions(names, True,
            what == "create" ? table.create_action : NOTHING);
        foreach string name in (names) {
            if (!fields.hasKey(name)) {
                continue;
            }
            bool required = declared{name}.required || fields{name}.getType().isMandatory()
                || (fields{name}.getAttributes().required ?? False);
            auto value = options{name} ?? declared{name}.default_value;
            if (!exists value || (value.typeCode() == NT_STRING && !value.val() && required)) {
                if (!required) {
                    continue;
                }
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y needs the %y %s option, which identifies "
                    "the collection to work in", table.name, name, what);
            }
            rv{name} = value;
        }
        return rv;
    }

    #! Returns the identity options an action accepts
    private hash<auto> getActionId(string action, hash<auto> id) {
        if (table.action_id_options{action}.val() || inlist(action, table.id_list_actions)) {
            string option = table.action_id_options{action} ?? table.id_option ?? table.id_field;
            auto value = id.firstValue();
            return {option: inlist(action, table.id_list_actions) ? (value,) : value};
        }
        return id;
    }

    #! Returns the list wrapper used by this write action
    private *string getWriteListOption(string action) {
        return table.write_list_options{action} ?? table.write_list_option;
    }

    #! Fetches a listing page's full records in listing order, skipping concurrently removed IDs
    private list<hash<auto>> hydrateRecords(list<hash<auto>> ids, *hash<auto> search_options) {
        string option = table.action_id_options{table.get_action} ?? table.id_option ?? table.id_field;
        list<auto> values = map getRecordId($1, "fetch").firstValue(), ids;
        auto resp = getLister(table.get_action).doRequest(
            getScope(search_options, "search", table.get_action) + {option: values});
        list<hash<auto>> records = RestSchemaRecordPageIterator::extractRecordValues(resp, table.record_path,
            table.record_is_map, table.name);
        hash<string, hash<auto>> by_id = {};
        foreach hash<auto> rec in (records) {
            by_id{getRecordId(rec, "fetch").firstValue().toString()} = rec;
        }
        return (map by_id{$1.toString()}, values, by_id.hasKey($1.toString())) ?? ();
    }

    #! Returns the further listing options a search passes through
    private hash<auto> getExtraSearchOptions(*hash<auto> search_options) {
        # "auto!" because the listing's own options are of every type
        hash<auto!> rv = {};
        foreach string name in (table.search_options) {
            if (exists search_options{name}) {
                rv{name} = search_options{name};
            }
        }
        return rv;
    }

    #! Returns the listing options the caller chose, as opposed to the ones the table defaulted for it
    /** A default is what the table sends when the caller has not said otherwise, and a predicate on the same
        field **is** the caller saying otherwise - so a predicate may replace a defaulted option, and may not
        replace one the caller set.

        Without the distinction a table could not push down a predicate on any field whose option carries a
        default at all, which is the common case: GitHub's issue tables default their \c state option to
        \c "all" so that a plain search returns open and closed issues alike, and a search for the closed
        ones has to be able to say so on the wire.

        @param search_options the search options, after the framework has filled in the defaults
    */
    private hash<string, bool> getExplicitSearchOptions(*hash<auto> search_options) {
        hash<string, bool> rv = {};
        if (!table.search_options) {
            return rv;
        }
        hash<string, hash<DataProviderOptionInfo>> opts = getListingOptions(table.search_options, False);
        foreach string name in (table.search_options) {
            if (!exists search_options{name}) {
                continue;
            }
            auto def = opts{name}.default_value;
            if (exists def && def == search_options{name}) {
                continue;
            }
            rv{name} = True;
        }
        return rv;
    }

    #! Returns a provider executing one of the table's actions
    private RestSchemaActionDataProvider getLister(string action) {
        return new RestSchemaActionDataProvider(action_set, action_set.getOperationEx(action), rest, router);
    }

    #! Returns the search options the table accepts
    private hash<string, hash<DataProviderOptionInfo>> getSearchOptions() {
        hash<string, hash<DataProviderOptionInfo>> rv = getListingOptions(table.scope_options, True);
        rv += getListingOptions(table.search_options, False);
        return rv + TableSearchOptions;
    }

    #! Returns the create options the table accepts
    private *hash<string, hash<DataProviderOptionInfo>> getCreateOptions() {
        return getListingOptions(table.create_scope_options ?? table.scope_options, True,
            table.create_action) ?: NOTHING;
    }

    #! Returns table options taken from the listing action's own declarations
    /** The options are the listing's, so their names, types, descriptions, allowed values and dropdowns are
        the manifest's and are not written a second time here.

        @param names the listing options to publish
        @param required whether to narrow them to required
        @param action the action supplying the options; defaults to the listing
    */
    private hash<string, hash<DataProviderOptionInfo>> getListingOptions(*softlist<string> names,
            bool required, *string action) {
        hash<string, hash<DataProviderOptionInfo>> rv = {};
        if (!names) {
            return rv;
        }
        *hash<string, AbstractDataField> fields =
            action_set.getOperationEx(action ?? table.list_action).getRequestFields();
        foreach string name in (names) {
            *AbstractDataField field = fields{name};
            if (!field) {
                continue;
            }
            hash<DataProviderOptionInfo> opt = RestSchemaTableDataProvider::fieldToOption(field, required);
            if (*hash<auto> ovl = table.search_option_overrides{name}) {
                opt = cast<hash<DataProviderOptionInfo>>(opt + ovl);
            }
            if (action.val() && (field.getType().isMandatory() || (field.getAttributes().required ?? False))) {
                opt.required = True;
            }
            rv{name} = opt;
        }
        return rv;
    }

    #! Returns a record type built from a list of column declarations
    /** For a table that names its own columns rather than deriving them: a projection of the API's shape, or
        a collection whose element type is a union and therefore has none to derive.

        @param fields the columns, each a hash of \c "name", \c "desc" and \c "type", where the type names an
        entry of @ref DataProvider::AbstractDataProviderTypeMap "AbstractDataProviderTypeMap"

        @throw REST-SCHEMA-RECORD-ERROR a column names a type that does not exist
    */
    static hash<string, AbstractDataField> makeRecordType(list<hash<auto>> fields) {
        hash<string, AbstractDataField> rv = {};
        foreach hash<auto> f in (fields) {
            *AbstractDataProviderType type = AbstractDataProviderTypeMap{f."type"};
            if (!type) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("column %y declares type %y, which is not a known "
                    "data provider type", f.name, f."type");
            }
            rv{f.name} = new QoreDataField(f.name, f.desc, type);
        }
        return rv;
    }

    #! Returns a data provider option built from an action's field
    static hash<DataProviderOptionInfo> fieldToOption(AbstractDataField field, bool required) {
        hash<DataFieldInfo> info = field.getLocalInfo();
        *hash<auto> attr = field.getAttributes();
        hash<DataProviderOptionInfo> rv = <DataProviderOptionInfo>{
            "display_name": info.display_name,
            "short_desc": info.short_desc,
            "desc": info.desc ?? info.short_desc ?? info.display_name,
            "type": field.getType(),
            "required": required,
        };
        if (exists info.default_value) {
            rv.default_value = info.default_value;
        }
        if (exists info.example_value) {
            rv.example_value = info.example_value;
        }
        if (info.allowed_values) {
            rv.allowed_values = info.allowed_values;
            rv.allowed_values_creatable = info.allowed_values_creatable ?? False;
        }
        # a list-valued option enumerates its elements, not whole values; dropping these would leave a
        # multi-select as free text in the table where the action offers a list
        if (info.element_allowed_values) {
            rv.element_allowed_values = info.element_allowed_values;
            rv.element_allowed_values_creatable = info.element_allowed_values_creatable ?? False;
        }
        # a dropdown and its scope are the manifest's, and a table option that dropped them would be a
        # free-text field where the action offers a list
        foreach string key in (("ref_data", "element_ref_data", "depends_on", "preselected",
                "has_dependents", "sensitive", "ui_type", "required_groups", "exclusive_with")) {
            if (exists attr{key}) {
                rv{key} = attr{key};
            }
        }
        # an action field says a value may also be typed by hand with "supports_custom_values"; a data
        # provider option says the same thing with "allowed_values_creatable"
        if (attr.supports_custom_values) {
            rv.allowed_values_creatable = True;
        }
        return rv;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": table.name,
            "type": "RestSchemaTableDataProvider",
            "display_name": table.display_name,
            "short_desc": table.short_desc,
            "desc": table.desc,
            "has_record": True,
            "supports_read": True,
            "supports_create": table.create_action.val().toBool(),
            "supports_update": table.update_action.val().toBool(),
            "supports_delete": table.delete_action.val().toBool(),
            # only a table whose API takes a filter expression can be said to search natively; see
            # @ref restschematable_native
            "supports_native_search": filter.hasNativeSearch(),
            "supports_search_expressions": True,
            "expressions": filter.getExpressionInfo(),
            "search_options": getSearchOptions(),
            "create_options": getCreateOptions(),
        };
    }
}
}
