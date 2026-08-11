# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaTablesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! Names the record-based actions an application registers against its tables
/** Tables are reached by **actions**; declaring the providers is only half of it, and an application that
    declares tables and registers no action against them offers them to nobody.

    The IDs are the application's to choose and are a product commitment: where a native application replaces
    an existing one, they are the IDs the superseded application registered, whatever naming style that
    implies.  Leave an ID unset to withdraw that action.
*/
public hashdecl RestSchemaRecordActionIds {
    #! The ID of the action searching a table
    *string search_records = "search";

    #! The ID of the action searching a table for one record
    *string search_single_record = "search-single";

    #! The ID of the action creating records in a table
    *string create_records = "create";

    #! The ID of the action updating records in a table
    *string update_records = "update";

    #! The ID of the action deleting records from a table
    *string delete_records = "delete";
}

#! The container holding an application's record-based tables
/** Resolves a table name to a
    @ref RestSchemaActions::RestSchemaTableDataProvider "RestSchemaTableDataProvider" built on the
    application's action set, and registers the record-based actions that reach them.
*/
public class RestSchemaTablesDataProvider inherits DataProvider::AbstractDataProvider {
    private {
        #! The name of this provider under the application root
        string name;

        #! The action sets the tables' actions belong to
        list<RestSchemaActionSet> action_sets;

        #! The table declarations, keyed by table name
        hash<string, hash<RestSchemaRecordTableInfo>> tables;

        #! The REST client used for API calls
        *AbstractRestClient rest;
    }

    #! Creates the object from the arguments
    /** @param name the name of this provider under the application root
        @param action_sets the action set the tables' actions belong to, or the sets they belong to

        An application split across documents has one set per document, and its tables do not all belong to
        the same one: Xero's invoices are declared in its accounting description and its projects in a
        separate one.  Each table is resolved against the set that exports its listing action.

        @param tables the table declarations, in presentation order
        @param rest the REST client to use for API calls

        @throw REST-SCHEMA-RECORD-ERROR a table is declared twice, or no set exports its listing action
    */
    constructor(string name, softlist<RestSchemaActionSet> action_sets,
            list<hash<RestSchemaRecordTableInfo>> tables, *AbstractRestClient rest) {
        self.name = name;
        self.action_sets = action_sets;
        self.rest = rest;
        foreach hash<RestSchemaRecordTableInfo> t in (tables) {
            if (exists self.tables{t.name}) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y is declared more than once", t.name);
            }
            # resolved now rather than when the table is opened, so that a table naming a listing no set
            # exports fails while the application is being built
            RestSchemaTablesDataProvider::getTableSet(action_sets, t);
            self.tables{t.name} = t;
        }
    }

    #! Returns the action set exporting a table's listing action
    /** @param action_sets the sets to look in
        @param table the table

        @throw REST-SCHEMA-RECORD-ERROR no set exports the table's listing action
    */
    static RestSchemaActionSet getTableSet(softlist<RestSchemaActionSet> action_sets,
            hash<RestSchemaRecordTableInfo> table) {
        foreach RestSchemaActionSet aset in (action_sets) {
            if (aset.getOperation(table.list_action)) {
                return aset;
            }
        }
        throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y is listed by action %y, which no action set in "
            "this application exports", table.name, table.list_action);
    }

    #! Returns the provider name
    string getName() {
        return name;
    }

    #! Returns the provider description
    *string getDesc() {
        return "record-based tables";
    }

    #! Returns the table declarations, keyed by table name
    hash<string, hash<RestSchemaRecordTableInfo>> getTables() {
        return tables;
    }

    #! Validates every table against the action set it is built on
    /** An application's test suite should call this, so that a table naming an action that has been withdrawn
        from the manifest fails there rather than when a user opens the table.

        @throw REST-SCHEMA-RECORD-ERROR a table declaration is inconsistent with the manifest
    */
    checkTables() {
        foreach hash<RestSchemaRecordTableInfo> t in (tables.iterator()) {
            RestSchemaActionSet action_set = RestSchemaTablesDataProvider::getTableSet(action_sets, t);
            RestSchemaTableDataProvider::check(action_set, t);
            # a path that does not resolve against the response it reads fails silently, so it is checked
            # here rather than found when one record comes back as none
            RestSchemaTableDataProvider::checkRecordPaths(action_set, t);
            # asking for the record type is what proves the table has one; a collection whose element is a
            # union has none to derive, and a table with no columns is worse than a table that failed here
            RestSchemaTableDataProvider tp(action_set, t, rest);
            on_error rethrow $1.err, sprintf("%s (while checking record table %y)", $1.desc, t.name);
            RestSchemaTablesDataProvider::checkTableFields(t, tp.getRecordType());
        }
    }

    #! Checks that everything a table declares about its records names a column the records have
    /** A filter, a filterable query field and an identity are all **predicates or references on a record**,
        so each has to name a column of the record type.  One that does not is dead on arrival and fails in a
        way that does not point at the declaration: the framework rejects a search on an unknown field before
        the table is reached, so a filter on a column that is not there is simply never used, and an identity
        that is not there produces a table that reads correctly and cannot write.

        This is checked here rather than in
        @ref RestSchemaActions::RestSchemaTableDataProvider::check() "check()" because it needs the record
        type, and deriving that parses the listing action's response - work a declaration check should not
        do.  An application's test suite reaches it through @ref checkTables().

        The identity is only required to be a column when something addresses a record by it.  An append-only
        log - Xero's invoice history - has no identity of its own and needs none, because it is only read
        and appended to.

        @param table the table declaration
        @param rt the table's record type

        @throw REST-SCHEMA-RECORD-ERROR something declared is not a column
    */
    static checkTableFields(hash<RestSchemaRecordTableInfo> table, *hash<string, AbstractDataField> rt) {
        list<string> bad = ();
        foreach hash<RestSchemaRecordFilterInfo> f in (table.filters) {
            if (!rt{f.field}) {
                bad += sprintf("filter on %y (%s)", f.field, f.operator);
            }
        }
        foreach string f in (keys table.query.fields) {
            if (!rt{f}) {
                bad += sprintf("filterable query field %y", f);
            }
        }
        if (table.get_action.val() || table.update_action.val() || table.delete_action.val()) {
            foreach hash<auto> i in (RestSchemaTableDataProvider::getIdParts(table).pairIterator()) {
                # an identity part may name a field nested in the record, so only the root has to be a column
                if (!rt{i.value.split(".")[0]}) {
                    bad += sprintf("identity %y for option %y", i.value, i.key);
                }
            }
        }
        if (bad) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y declares %y, which %s not a column of its "
                "record type; columns: %y", table.name, bad, bad.size() == 1 ? "is" : "are", keys rt);
        }
    }

    #! Returns the table names
    private *list<string> getChildProviderNamesImpl() {
        return keys tables;
    }

    #! Returns the given table, or @ref nothing if the table is unknown
    private *AbstractDataProvider getChildProviderImpl(string child_name) {
        *hash<RestSchemaRecordTableInfo> t = tables{child_name};
        if (!t) {
            return;
        }
        return new RestSchemaTableDataProvider(RestSchemaTablesDataProvider::getTableSet(action_sets, t), t,
            rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": name,
            "type": "RestSchemaTablesDataProvider",
            "supports_children": True,
            "children_can_support_records": True,
        };
    }

    #! Returns the path variable naming the table a record action works on
    /** @param tables the table declarations, whose names are listed in the description
    */
    static hash<DataProviderPathVarInfo> getTablePathVar(list<hash<RestSchemaRecordTableInfo>> tables) {
        list<string> names = map "`" + $1.name + "`", tables;
        string list_desc;
        if (names.size() == 1) {
            list_desc = names[0];
        } else {
            list_desc = (foldl $1 + ", " + $2, names[0..(names.size() - 2)]) + " or " + names.last();
        }
        return <DataProviderPathVarInfo>{
            "display_name": "Table",
            "short_desc": "The name of the table to work with",
            "desc": "The table to work with: " + list_desc,
            "example_value": tables[0].name,
        };
    }

    #! Returns what every table in the set supports, for the record-based actions
    /** The per-table search and create options are not here: they differ by table and are resolved through
        \c data_dependent_options by walking to the chosen table.  What is here is the capability set common
        to the whole application, together with the scope options **every** table requires - a repository, a
        workspace - which is what a form can show before a table has been chosen.

        @param action_sets the action sets the tables' actions belong to
        @param tables the table declarations
    */
    static hash<DataProviderInfo> getRecordActionProviderInfo(softlist<RestSchemaActionSet> action_sets,
            list<hash<RestSchemaRecordTableInfo>> tables) {
        hash<string, hash<DataProviderOptionInfo>> scope =
            RestSchemaTablesDataProvider::getCommonScopeOptions(action_sets, tables);
        return <DataProviderInfo>{
            "type": "RestSchemaTableDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_create": (select tables, $1.create_action.val()).toBool(),
            "supports_update": (select tables, $1.update_action.val()).toBool(),
            "supports_delete": (select tables, $1.delete_action.val()).toBool(),
            "supports_native_search": (select tables, exists $1.query).toBool(),
            "supports_search_expressions": True,
            "expressions": RestSchemaRecordFilter::SearchExpressions,
            "search_options": scope + RestSchemaTableDataProvider::TableSearchOptions,
            "create_options": scope ?: NOTHING,
        };
    }

    #! Returns the scope options every table in the set requires
    /** An option only some tables need cannot be published app-wide: a form showing it before a table has
        been chosen would ask for a value half the tables have no use for.

        @param action_sets the action sets the tables' actions belong to
        @param tables the table declarations
    */
    static hash<string, hash<DataProviderOptionInfo>> getCommonScopeOptions(
            softlist<RestSchemaActionSet> action_sets, list<hash<RestSchemaRecordTableInfo>> tables) {
        hash<string, hash<DataProviderOptionInfo>> rv = {};
        if (!tables) {
            return rv;
        }
        hash<string, int> counts = {};
        foreach hash<RestSchemaRecordTableInfo> t in (tables) {
            map counts{$1} = counts{$1} + 1, t.scope_options;
        }
        foreach hash<RestSchemaRecordTableInfo> t in (tables) {
            *hash<string, AbstractDataField> fields = RestSchemaTablesDataProvider::getTableSet(action_sets,
                t).getOperationEx(t.list_action).getRequestFields();
            foreach string name in (t.scope_options) {
                if (counts{name} != tables.size() || exists rv{name} || !exists fields{name}) {
                    continue;
                }
                rv{name} = RestSchemaTableDataProvider::fieldToOption(fields{name}, True);
            }
        }
        return rv;
    }

    #! Registers the record-based actions that reach an application's tables
    /** @param app the application name
        @param path the data provider path of the tables container, without the table path variable
        @param action_sets the action set the tables' actions belong to, or the sets they belong to
        @param tables the table declarations
        @param ids the action IDs to register; leave one unset to withdraw that action
        @param common values merged into every registered action definition, such as its groups
        @param overrides values merged into one action's definition, keyed by the registered action ID

        The generic wording here describes any application's tables.  An application replacing an existing
        one usually has better wording of its own, and changing what a user reads is a product decision, so
        @a overrides is how it keeps it.
    */
    static registerRecordActions(string app, string path, softlist<RestSchemaActionSet> action_sets,
            list<hash<RestSchemaRecordTableInfo>> tables, hash<RestSchemaRecordActionIds> ids,
            *hash<auto> common, *hash<string, hash<auto>> overrides) {
        hash<DataProviderInfo> dpinfo = RestSchemaTablesDataProvider::getRecordActionProviderInfo(
            action_sets, tables);
        hash<auto> base = {
            "app": app,
            "path": path + "/{table}",
            "path_vars": {
                "table": RestSchemaTablesDataProvider::getTablePathVar(tables),
            },
            "data_dependent_options": True,
            "data_provider_info": dpinfo,
        } + common;

        if (ids.search_records.val()) {
            DataProviderActionCatalog::registerAction(cast<hash<DataProviderActionInfo>>(base + {
                "action": ids.search_records,
                "display_name": "Search Records",
                "short_desc": "Search for matching records in the given table",
                "desc": "Returns every record in the given table matching the search criteria",
                "action_code": DPAT_FIND,
            } + (overrides{ids.search_records} ?? {})));
        }
        if (ids.search_single_record.val()) {
            DataProviderActionCatalog::registerAction(cast<hash<DataProviderActionInfo>>(base + {
                "action": ids.search_single_record,
                "display_name": "Find a Single Record",
                "short_desc": "Search for a single matching record in the given table",
                "desc": "Returns the one record in the given table matching the search criteria; it is an "
                    "error if more than one matches",
                "action_code": DPAT_FIND_SINGLE,
            } + (overrides{ids.search_single_record} ?? {})));
        }
        if (ids.create_records.val() && dpinfo.supports_create) {
            DataProviderActionCatalog::registerAction(cast<hash<DataProviderActionInfo>>(base + {
                "action": ids.create_records,
                "display_name": "Create Records",
                "short_desc": "Create new records in the given table",
                "desc": "Creates new records in the given table",
                "action_code": DPAT_CREATE,
            } + (overrides{ids.create_records} ?? {})));
        }
        if (ids.update_records.val() && dpinfo.supports_update) {
            DataProviderActionCatalog::registerAction(cast<hash<DataProviderActionInfo>>(base + {
                "action": ids.update_records,
                "display_name": "Update Records",
                "short_desc": "Update the records matching the search criteria",
                "desc": "Updates every record in the given table matching the search criteria",
                "action_code": DPAT_UPDATE,
            } + (overrides{ids.update_records} ?? {})));
        }
        if (ids.delete_records.val() && dpinfo.supports_delete) {
            DataProviderActionCatalog::registerAction(cast<hash<DataProviderActionInfo>>(base + {
                "action": ids.delete_records,
                "display_name": "Delete Records",
                "short_desc": "Delete the records matching the search criteria",
                "desc": "Deletes every record in the given table matching the search criteria",
                "action_code": DPAT_DELETE,
            } + (overrides{ids.delete_records} ?? {})));
        }
    }
}
}
