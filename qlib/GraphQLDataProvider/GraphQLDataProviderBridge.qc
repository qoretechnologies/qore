# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLDataProviderBridge.qc generates a GraphQL schema and resolvers from a DataProvider

/*  GraphQLDataProvider Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace GraphQLHandler {
#! Generates a GraphQL schema and resolvers automatically from a @ref dataproviderintro "DataProvider"
/** This bridge maps a data provider's record type and capabilities onto a GraphQL schema:
    - the record type's fields become a GraphQL object type (Qore types are mapped to GraphQL
      scalars; non-scalar fields are exposed as \c String)
    - if the provider supports reading, a \c search query field is generated that returns a list
      of records (with an optional DPQL \c where filter and \c limit / \c offset arguments)
    - if the provider supports creating records, a \c create mutation field is generated whose
      arguments are the record's fields
    - if the provider supports updating, a \c update mutation field is generated taking a
      \c set input object (the columns to assign) and a DPQL \c where filter; it returns the
      number of records affected
    - if the provider supports deleting, a \c delete mutation field is generated taking a DPQL
      \c where filter; it returns the number of records affected
    - if the provider supports upserting, an \c upsert mutation field is generated returning the
      record id

    @par DPQL filters
    The \c where arguments accept a <a href="https://qoretechnologies.com">DPQL</a> (Data
    Provider Query Language) expression string, for example \c "@status == \"active\" && @age >= 18".
    Filters are validated against the record's field schema and the provider's supported operators
    before execution. For safety, the \c where argument on \c update and \c delete is non-null, so
    the GraphQL schema itself forbids an unconditional (whole-table) mutation; use a tautology such
    as \c "@id != null" to deliberately match all records.

    @par Security
    DPQL template references (e.g. \c $qore-expr:{...}, \c $env:...) are rejected in filter
    expressions by default, since they can resolve to arbitrary runtime values. Pass
    \c {"allow_templates": True} to the constructor only when filter input is fully trusted, and
    never register permissive template callbacks for a client-facing deployment.

    @note Update and delete operate via the provider's native DPQL where-condition support. Bulk
    \c createMany / \c upsertMany operations are generated when the provider advertises
    \c supports_bulk_create / \c supports_bulk_upsert. By default \c update and \c delete return
    the affected-row count; pass \c {"return_records": True} to the constructor to instead return
    the affected records (read via a non-atomic follow-up search). The @ref getSdl() output
    documents exactly what was generated.

    @note \c search applies the \c limit and \c offset arguments client-side (the bridge stops
    iterating after \c offset + \c limit records) rather than pushing them into the provider's
    search options, because not all providers accept those options. For very large result sets a
    provider that supports server-side paging should be queried through a more specific interface;
    a DPQL \c where filter still narrows the result set at the provider.

    @par Example:
    @code{.py}
# generate a transport-agnostic executor from any data provider
GraphQLExecutor executor = DataProviderGraphQLSchema::createExecutor(my_data_provider);
# expose it over HTTP with a GraphQLHandler (from the GraphQLHandler module)
GraphQLHandler gh(executor);
    @endcode
*/
public class DataProviderGraphQLSchema {
    public {
        #! the default maximum number of records returned by a generated \c search field
        const DefaultSearchLimit = 1000;

        #! maps a typed-filter operator field to the DPQL operator it requires the provider to support
        const FilterOpToDpql = {
            "eq": "==",
            "ne": "!=",
            "gt": ">",
            "gte": ">=",
            "lt": "<",
            "lte": "<=",
            "in": "in",
            "nin": "notIn",
            "like": "like",
        };
    }

    private {
        #! the source data provider
        DataProvider::AbstractDataProvider provider;
        #! the provider's static info (capability flags, supported expressions)
        hash<DataProviderInfo> info;
        #! the provider's record type (field name -> field)
        *hash<string, AbstractDataField> record_type;
        #! the generated GraphQL object type name for the provider's record
        string record_type_name;
        #! maps the (sanitized) GraphQL field/argument name to the provider's native field name
        hash<string, string> field_name_map();
        #! whether DPQL template references are permitted in filter expressions
        bool allow_templates = False;
        #! whether update/delete mutations return the affected records (instead of a count)
        bool return_records = False;
        #! the maximum nested-type generation depth before falling back to a JSON scalar
        int max_type_depth = 8;
        #! generated nested object type definitions, keyed by type name (de-duplicated)
        hash<string, string> extra_type_defs();
        #! generated nested input-object type definitions, keyed by type name (de-duplicated)
        /** GraphQL forbids using an output object type in input position, so non-scalar fields
            appearing in create/update/upsert arguments get a parallel \c input type generated here
        */
        hash<string, string> extra_input_defs();
        #! custom scalar names that must be declared and coerced (DateTime, Base64, JSON)
        hash<string, bool> needed_scalars();
        #! output field resolvers for fields whose GraphQL name differs from the provider field
        #! name, keyed by object type name then GraphQL field name
        hash<auto> output_field_resolvers();
        #! the maximum child-provider traversal depth
        int max_provider_depth = 1;
        #! the generated SDL
        string sdl;
        #! the generated resolver map
        hash<auto> resolvers;
    }

    #! builds the bridge for the given data provider
    /** @param n_provider the data provider to expose over GraphQL
        @param opts optional options:
        - \c allow_templates (bool): permit DPQL template references in filter expressions
          (default @ref False)
        - \c return_records (bool): generate \c update and \c delete mutations that return the
          affected records (instead of an affected-row count); the records are read via a
          non-atomic follow-up search (default @ref False)

        @throw GRAPHQL-SCHEMA-ERROR the provider does not expose a record type
    */
    constructor(DataProvider::AbstractDataProvider n_provider, *hash<auto> opts) {
        provider = n_provider;
        info = provider.getInfo();
        record_type = provider.getRecordType();
        if (!record_type) {
            throw GRAPHQL_SCHEMA_ERROR, sprintf("data provider %y does not expose a record type "
                "and cannot be mapped to a GraphQL schema", provider.getName());
        }
        record_type_name = sanitizeName(provider.getName()) + "Record";
        if (exists opts.allow_templates) {
            allow_templates = opts.allow_templates;
        }
        if (exists opts.return_records) {
            return_records = opts.return_records;
        }
        if (exists opts.max_type_depth) {
            max_type_depth = opts.max_type_depth;
        }
        if (exists opts.max_provider_depth) {
            max_provider_depth = opts.max_provider_depth;
        }
        generate();
    }

    #! returns the generated SDL schema
    string getSdl() {
        return sdl;
    }

    #! returns the generated resolver map
    hash<auto> getResolvers() {
        return resolvers;
    }

    #! returns a @ref GraphQLExecutor built from the generated schema, resolvers, and scalar coercers
    GraphQLExecutor getExecutor() {
        return new GraphQLExecutor(new GraphQLSchema(sdl), resolvers, buildScalarCoercers());
    }

    #! builds a @ref GraphQLExecutor that executes against the given data provider
    /** @param provider the data provider to expose
        @param opts optional bridge options (see the constructor)

        @return a ready-to-use @ref GraphQLExecutor
    */
    static GraphQLExecutor createExecutor(DataProvider::AbstractDataProvider provider, *hash<auto> opts) {
        return (new DataProviderGraphQLSchema(provider, opts)).getExecutor();
    }

    #! generates the SDL and resolvers from the provider
    private generate() {
        # map record fields to GraphQL field definitions, building the gql-name -> provider-name map
        list<string> fieldLines();
        # input field lines (for create arguments, the record Input type, and bulk operations):
        # non-scalar fields here map to generated input-object types, never output object types
        list<string> inputFieldLines();
        list<string> setLines();
        list<string> whereLines();
        hash<string, bool> usedFilterKinds();
        foreach string fname in (keys record_type) {
            string gqlName = sanitizeName(fname);
            # distinct provider field names must not collide to the same GraphQL name, or one
            # would silently shadow the other in the generated schema and field-name round-trip
            if (exists field_name_map{gqlName} && field_name_map{gqlName} != fname) {
                throw GRAPHQL_SCHEMA_ERROR, sprintf("data provider %y has fields %y and %y that "
                    "both map to the GraphQL field name %y; rename one to expose this provider "
                    "over GraphQL", provider.getName(), field_name_map{gqlName}, fname, gqlName);
            }
            field_name_map{gqlName} = fname;
            # if the GraphQL field name was sanitized, register an output resolver that reads the
            # provider's native field name from the record hash
            if (gqlName != fname) {
                output_field_resolvers{record_type_name}{gqlName} = makeFieldReader(fname);
            }
            string nameHint = record_type_name + capitalize(gqlName);
            push fieldLines, sprintf("  %s: %s", gqlName, mapFieldType(record_type{fname}, nameHint, False));
            push inputFieldLines, sprintf("  %s: %s", gqlName,
                mapInputFieldType(record_type{fname}, nameHint, False));
            # SetInput fields are always optional (a partial update assigns only supplied columns)
            push setLines, sprintf("  %s: %s", gqlName, mapInputFieldType(record_type{fname}, nameHint, True));
            # WhereInput: only simple scalar fields get a typed per-scalar filter
            *string fkind = scalarFilterKind(record_type{fname}.getType());
            if (exists fkind) {
                usedFilterKinds{fkind} = True;
                push whereLines, sprintf("  %s: %sFilter", gqlName, fkind);
            }
        }
        # the comma-separated argument list for create/upsert (shares the input field mapping)
        string createArgs = inputFieldLines.join(", ");

        # DPQL where-filtering requires the provider to support advanced search expressions;
        # update/delete are only generated when filtering is available
        bool can_filter = info.supports_search_expressions;
        bool gen_update = info.supports_update && can_filter;
        bool gen_delete = info.supports_delete && can_filter;
        string whereInputName = record_type_name + "WhereInput";

        list<string> defs();
        push defs, sprintf("type %s {\n%s\n}", record_type_name, fieldLines.join("\n"));

        # the filter argument fragment (typed WhereInput + raw DPQL string escape hatch)
        string filterArgs = sprintf("filter: %s, where: String", whereInputName);

        # Query
        list<string> queryFields();
        if (info.supports_read) {
            queryFields += can_filter
                ? sprintf("  search(%s, limit: Int, offset: Int): [%s!]!", filterArgs, record_type_name)
                : sprintf("  search(limit: Int, offset: Int): [%s!]!", record_type_name);
        }
        # child-provider traversal (gap 5): each direct child provider with a record type is
        # exposed as a read-only Query field returning its records
        hash<auto> child_resolvers();
        if (max_provider_depth >= 1) {
            foreach string cn in (provider.getChildProviderNames() ?? ()) {
                *hash<string, AbstractDataField> crt = getChildRecordType(cn);
                if (!crt) {
                    continue;
                }
                string childField = sanitizeName(cn);
                string childTypeName = record_type_name + "Child" + capitalize(childField);
                buildObjectType(childTypeName, crt, 0);
                queryFields += sprintf("  %s(limit: Int, offset: Int): [%s!]!", childField, childTypeName);
                string cnCapture = cn;
                AbstractDataProvider parentP = provider;
                child_resolvers{childField} = auto sub (auto parent, hash<auto> args, auto ctx) {
                    return DataProviderGraphQLSchema::searchChild(parentP, cnCapture, args);
                };
            }
        }
        if (!queryFields) {
            queryFields += "  _empty: Boolean";
        }
        push defs, sprintf("type Query {\n%s\n}", queryFields.join("\n"));

        # update/delete return the affected records when requested, otherwise the affected count
        string mutationReturn = return_records ? sprintf("[%s!]!", record_type_name) : "Int!";
        # bulk operations (bulk create/upsert build on the corresponding single-record capability)
        bool gen_bulk_create = info.supports_create && info.supports_bulk_create;
        bool gen_bulk_upsert = info.supports_upsert && info.supports_bulk_upsert;
        string recordInputName = record_type_name + "Input";

        # Mutation
        list<string> mutationFields();
        if (info.supports_create) {
            mutationFields += sprintf("  create(%s): %s", createArgs, record_type_name);
        }
        if (gen_update) {
            mutationFields += sprintf("  update(set: %sSetInput!, %s): %s",
                record_type_name, filterArgs, mutationReturn);
        }
        if (gen_delete) {
            mutationFields += sprintf("  delete(%s): %s", filterArgs, mutationReturn);
        }
        if (info.supports_upsert) {
            mutationFields += sprintf("  upsert(%s): String!", createArgs);
        }
        if (gen_bulk_create) {
            mutationFields += sprintf("  createMany(records: [%s!]!): Int!", recordInputName);
        }
        if (gen_bulk_upsert) {
            mutationFields += sprintf("  upsertMany(records: [%s!]!): [String!]!", recordInputName);
        }
        if (mutationFields) {
            push defs, sprintf("type Mutation {\n%s\n}", mutationFields.join("\n"));
        }

        # the SetInput type is only referenced by update
        if (gen_update) {
            push defs, sprintf("input %sSetInput {\n%s\n}", record_type_name, setLines.join("\n"));
        }
        # the record Input type is referenced by the bulk operations (uses the input field mapping
        # so non-scalar fields reference generated input types, not output object types)
        if (gen_bulk_create || gen_bulk_upsert) {
            push defs, sprintf("input %s {\n%s\n}", recordInputName, inputFieldLines.join("\n"));
        }

        # the typed filter input types (only when filtering is available)
        if (can_filter) {
            # per-scalar filter input types actually used by the record's fields, with operators
            # gated on what the provider supports
            foreach string fkind in (keys usedFilterKinds) {
                push defs, filterTypeSdl(fkind);
            }
            # the record's WhereInput, with recursive logical combinators
            push whereLines, sprintf("  _and: [%s!]", whereInputName);
            push whereLines, sprintf("  _or: [%s!]", whereInputName);
            push whereLines, sprintf("  _not: %s", whereInputName);
            push defs, sprintf("input %s {\n%s\n}", whereInputName, whereLines.join("\n"));
        }

        # nested object types generated from non-scalar record fields
        foreach string nested in (extra_type_defs.values()) {
            push defs, nested;
        }
        # nested input-object types generated from non-scalar create/update/upsert argument fields
        foreach string nested in (extra_input_defs.values()) {
            push defs, nested;
        }
        # custom scalar declarations (DateTime, Base64, JSON) used by the generated types
        foreach string sc in (keys needed_scalars) {
            push defs, sprintf("scalar %s", sc);
        }

        sdl = defs.join("\n\n");

        # Build resolvers as closures over an immutable "spec" value (NOT over the bridge object):
        # the bridge instance may be a temporary (e.g. from createExecutor()) destroyed at scope
        # exit, so the resolvers must not depend on its lifetime. The spec holds the provider
        # reference and the metadata the static resolver helpers need.
        # determine whether the provider supports server-side limit/offset paging
        *hash<auto> searchOpts;
        try {
            searchOpts = provider.getSearchOptions();
        } catch (hash<ExceptionInfo> ex) {
            # provider does not support search options; fall back to client-side paging
        }
        hash<auto> spec = {
            "provider": provider,
            "record_type": record_type,
            "expressions": info.expressions,
            "name_map": field_name_map,
            "allow_templates": allow_templates,
            "return_records": return_records,
            "push_paging": exists searchOpts.limit && exists searchOpts.offset,
            "requires_tx": info.transaction_management,
        };

        hash<auto> queryResolvers();
        if (info.supports_read) {
            queryResolvers.search = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execSearch(spec, args);
            };
        }
        # child-provider Query resolvers
        queryResolvers += child_resolvers;
        resolvers = {"Query": queryResolvers};
        # output field resolvers for record/nested fields whose GraphQL name was sanitized
        foreach string tname in (keys output_field_resolvers) {
            resolvers{tname} = (resolvers{tname} ?? {}) + output_field_resolvers{tname};
        }

        hash<auto> mutationResolvers();
        if (info.supports_create) {
            mutationResolvers.create = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execCreate(spec, args);
            };
        }
        if (gen_update) {
            mutationResolvers.update = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execUpdate(spec, args);
            };
        }
        if (gen_delete) {
            mutationResolvers.delete = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execDelete(spec, args);
            };
        }
        if (info.supports_upsert) {
            mutationResolvers.upsert = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execUpsert(spec, args);
            };
        }
        if (gen_bulk_create) {
            mutationResolvers.createMany = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execCreateMany(spec, args);
            };
        }
        if (gen_bulk_upsert) {
            mutationResolvers.upsertMany = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execUpsertMany(spec, args);
            };
        }
        if (mutationResolvers) {
            resolvers.Mutation = mutationResolvers;
        }
    }

    #! executes a search against the provider, applying the optional filter and limit/offset paging
    static list<auto> execSearch(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        int limit = args.limit ?? DefaultSearchLimit;
        int offset = args.offset ?? 0;
        *string dpql = DataProviderGraphQLSchema::buildEffectiveDpql(spec, args);
        if (dpql.val()) {
            DataProviderGraphQLSchema::validateWhere(spec, dpql);
        }

        # if the provider supports server-side paging, push limit/offset into the search options and
        # return all returned records; otherwise page client-side
        if (spec.push_paging) {
            hash<auto> sopts = {"limit": limit, "offset": offset};
            AbstractDataProviderRecordIterator pit = dpql.val()
                ? p.searchRecords(dpql, sopts)
                : p.searchRecords(NOTHING, sopts);
            list<auto> pout();
            map pout += $1, pit;
            return pout;
        }

        AbstractDataProviderRecordIterator it = dpql.val() ? p.searchRecords(dpql, {}) : p.searchRecords();
        list<auto> out();
        int idx = 0;
        int taken = 0;
        while (it.next()) {
            if (idx++ < offset) {
                continue;
            }
            if (taken >= limit) {
                break;
            }
            push out, it.getValue();
            ++taken;
        }
        return out;
    }

    #! creates a record from the GraphQL arguments
    static *hash<auto> execCreate(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        return p.createRecord(DataProviderGraphQLSchema::toProviderRec(spec, args));
    }

    #! updates records matching the filter; returns the affected records or the affected count
    static auto execUpdate(hash<auto> spec, hash<auto> args) {
        string dpql = DataProviderGraphQLSchema::requireSelector(spec, args, "update");
        AbstractDataProvider p = spec.provider;
        hash<auto> set = DataProviderGraphQLSchema::toProviderRec(spec, args.set);
        if (!spec.return_records) {
            return p.updateRecords(set, dpql, {});
        }
        # returning records: re-read the matching records after the update. When the provider
        # supports transactions, wrap mutate + re-read so the returned records are atomic
        if (spec.requires_tx) {
            p.beginTransaction();
            try {
                p.updateRecords(set, dpql, {});
                list<auto> rv = DataProviderGraphQLSchema::collectMatching(p, dpql);
                p.commit();
                return rv;
            } catch (hash<ExceptionInfo> ex) {
                p.rollback();
                rethrow;
            }
        }
        p.updateRecords(set, dpql, {});
        return DataProviderGraphQLSchema::collectMatching(p, dpql);
    }

    #! deletes records matching the filter; returns the affected records or the affected count
    static auto execDelete(hash<auto> spec, hash<auto> args) {
        string dpql = DataProviderGraphQLSchema::requireSelector(spec, args, "delete");
        AbstractDataProvider p = spec.provider;
        if (!spec.return_records) {
            return p.deleteRecords(dpql, {});
        }
        # returning records: capture the matching records before deleting. When the provider
        # supports transactions, wrap capture + delete so the returned records are atomic
        if (spec.requires_tx) {
            p.beginTransaction();
            try {
                list<auto> captured = DataProviderGraphQLSchema::collectMatching(p, dpql);
                p.deleteRecords(dpql, {});
                p.commit();
                return captured;
            } catch (hash<ExceptionInfo> ex) {
                p.rollback();
                rethrow;
            }
        }
        list<auto> captured = DataProviderGraphQLSchema::collectMatching(p, dpql);
        p.deleteRecords(dpql, {});
        return captured;
    }

    #! creates a batch of records, returning the number created
    static int execCreateMany(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        list<auto> rows = map DataProviderGraphQLSchema::toProviderRec(spec, $1), args.records;
        p.createRecordsBulk(rows);
        return rows.size();
    }

    #! upserts a batch of records, returning the list of record ids
    static list<auto> execUpsertMany(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        list<auto> rows = map DataProviderGraphQLSchema::toProviderRec(spec, $1), args.records;
        return p.upsertRecordsBulk(rows) ?? ();
    }

    #! materializes all records matching a DPQL filter (used when returning affected records)
    static list<auto> collectMatching(AbstractDataProvider p, string dpql) {
        list<auto> out();
        AbstractDataProviderRecordIterator it = p.searchRecords(dpql, {});
        map out += $1, it;
        return out;
    }

    #! returns a child provider's record type, or @ref NOTHING if the child has none or is inaccessible
    private *hash<string, AbstractDataField> getChildRecordType(string name) {
        *AbstractDataProvider cp;
        try {
            cp = provider.getChildProvider(name);
        } catch (hash<ExceptionInfo> ex) {
            return NOTHING;
        }
        if (!cp) {
            return NOTHING;
        }
        try {
            return cp.getRecordType();
        } catch (hash<ExceptionInfo> ex) {
            return NOTHING;
        }
    }

    #! searches a child provider's records, applying limit/offset paging
    static list<auto> searchChild(AbstractDataProvider parentP, string childName, hash<auto> args) {
        AbstractDataProvider cp = parentP.getChildProvider(childName);
        AbstractDataProviderRecordIterator it = cp.searchRecords();
        int limit = args.limit ?? DefaultSearchLimit;
        int offset = args.offset ?? 0;
        list<auto> out();
        int idx = 0;
        int taken = 0;
        while (it.next()) {
            if (idx++ < offset) {
                continue;
            }
            if (taken >= limit) {
                break;
            }
            push out, it.getValue();
            ++taken;
        }
        return out;
    }

    #! builds the effective DPQL for a mutation, requiring at least one selector (mass-mutation guard)
    /** @throw GRAPHQL-EXECUTION-ERROR no \c filter or \c where selector was provided, or the
        resulting filter is invalid
    */
    static string requireSelector(hash<auto> spec, hash<auto> args, string op) {
        *string dpql = DataProviderGraphQLSchema::buildEffectiveDpql(spec, args);
        if (!dpql.val()) {
            throw GRAPHQL_EXECUTION_ERROR, sprintf("%s requires a \"filter\" or \"where\" condition "
                "to select the records to affect", op);
        }
        DataProviderGraphQLSchema::validateWhere(spec, dpql);
        return dpql;
    }

    #! combines the typed \c filter argument (compiled to DPQL) and the raw \c where DPQL string
    /** @return the effective DPQL expression, or @ref NOTHING if no selector was provided
    */
    static *string buildEffectiveDpql(hash<auto> spec, hash<auto> args) {
        list<string> parts();
        if (args.filter.typeCode() == NT_HASH) {
            string fdpql = DataProviderGraphQLSchema::whereInputToDpql(spec, args.filter);
            if (fdpql.val()) {
                parts += fdpql;
            }
        }
        if (args.where.val()) {
            parts += args.where;
        }
        if (!parts) {
            return NOTHING;
        }
        if (parts.size() == 1) {
            return parts[0];
        }
        return "(" + parts.join(") && (") + ")";
    }

    #! compiles a WhereInput value to a DPQL expression string
    /** @param spec the resolver spec (for the gql->provider name map)
        @param filter the coerced WhereInput hash

        @return the DPQL expression (empty string if the filter is empty)
    */
    static string whereInputToDpql(hash<auto> spec, hash<auto> filter) {
        hash<auto> name_map = spec.name_map;
        list<string> clauses();
        foreach string key in (keys filter) {
            switch (key) {
                case "_and": {
                    # combine non-empty sub-clauses with AND; empty sub-filters contribute nothing
                    # (so that {_and: [{}]} does NOT degenerate to "()" and bypass the guard)
                    list<string> subs = select
                        (map DataProviderGraphQLSchema::whereInputToDpql(spec, $1), filter._and),
                        $1.val();
                    if (subs) {
                        clauses += subs.size() == 1 ? subs[0] : "(" + subs.join(") && (") + ")";
                    }
                    break;
                }
                case "_or": {
                    list<string> subs = select
                        (map DataProviderGraphQLSchema::whereInputToDpql(spec, $1), filter._or),
                        $1.val();
                    if (subs) {
                        clauses += "(" + subs.join(") || (") + ")";
                    }
                    break;
                }
                case "_not": {
                    string not_dpql = DataProviderGraphQLSchema::whereInputToDpql(spec, filter._not);
                    # an empty inner filter contributes nothing (avoids a match-everything "!()")
                    if (not_dpql.val()) {
                        clauses += "!(" + not_dpql + ")";
                    }
                    break;
                }
                default: {
                    # a per-field filter; the field must be a known (mapped) record field
                    *string provName = name_map{key};
                    if (!exists provName) {
                        throw GRAPHQL_EXECUTION_ERROR, sprintf("unknown filter field %y", key);
                    }
                    clauses += DataProviderGraphQLSchema::filterFieldToDpql(provName, filter{key});
                    break;
                }
            }
        }
        return (select clauses, $1.val()).join(" && ");
    }

    #! compiles a single field's per-scalar filter to DPQL conditions (AND-ed)
    static string filterFieldToDpql(string provName, hash<auto> ops) {
        string ref = DataProviderGraphQLSchema::quoteFieldRef(provName);
        list<string> conds();
        foreach string op in (keys ops) {
            switch (op) {
                case "eq": conds += sprintf("%s == %s", ref, renderDpqlValue(ops.eq)); break;
                case "ne": conds += sprintf("%s != %s", ref, renderDpqlValue(ops.ne)); break;
                case "gt": conds += sprintf("%s > %s", ref, renderDpqlValue(ops.gt)); break;
                case "gte": conds += sprintf("%s >= %s", ref, renderDpqlValue(ops.gte)); break;
                case "lt": conds += sprintf("%s < %s", ref, renderDpqlValue(ops.lt)); break;
                case "lte": conds += sprintf("%s <= %s", ref, renderDpqlValue(ops.lte)); break;
                case "in": conds += sprintf("%s in %s", ref, renderDpqlValue(ops.in)); break;
                case "nin": conds += sprintf("%s not in %s", ref, renderDpqlValue(ops.nin)); break;
                case "like": conds += sprintf("%s like %s", ref, renderDpqlValue(ops.like)); break;
            }
        }
        return conds.join(" && ");
    }

    #! renders a runtime value as a DPQL literal
    static string renderDpqlValue(auto v) {
        switch (v.typeCode()) {
            case NT_STRING: {
                string s = v;
                s =~ s/\\/\\\\/g;
                s =~ s/"/\\"/g;
                return "\"" + s + "\"";
            }
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return v.toString();
            case NT_BOOLEAN:
                return v ? "true" : "false";
            case NT_LIST:
                return "(" + (map renderDpqlValue($1), v).join(", ") + ")";
            case NT_NOTHING:
            case NT_NULL:
                return "null";
            default:
                return renderDpqlValue(v.toString());
        }
    }

    #! renders a DPQL field reference, quoting names that are not simple identifiers
    static string quoteFieldRef(string name) {
        if (name =~ /^[A-Za-z_][A-Za-z0-9_.]*$/) {
            return "@" + name;
        }
        string e = name;
        e =~ s/\\/\\\\/g;
        e =~ s/"/\\"/g;
        return "@\"" + e + "\"";
    }

    #! returns the per-scalar filter input-type kind for a type, or @ref NOTHING if not filterable
    /** Only simple scalar fields get a typed filter. Object, list, binary, and free-form hash
        fields are not typed-filterable; date fields are filtered through the raw DPQL \c where
        argument (which supports proper date literals), not a string filter.
    */
    private *string scalarFilterKind(AbstractDataProviderType t) {
        # real lists (element type present) and object types are not typed-filterable; "soft"
        # scalars report isList() but have no element type and ARE filterable
        if ((t.isList() && exists t.getElementType()) || t.getFields()) {
            return NOTHING;
        }
        switch (t.getBaseTypeCode()) {
            case NT_INT:
                return "Int";
            case NT_FLOAT:
            case NT_NUMBER:
                return "Float";
            case NT_BOOLEAN:
                return "Boolean";
            case NT_STRING:
                return "String";
            default:
                # dates, binary, hashes: no typed filter (use the raw DPQL where for dates)
                return NOTHING;
        }
    }

    #! returns the SDL for the per-scalar filter input type, emitting only the operators the
    #! provider advertises in \c info.expressions (gap 8); the full standard set if unadvertised
    private string filterTypeSdl(string kind) {
        # candidate operator fields per scalar kind
        list<string> candidates;
        switch (kind) {
            case "Int":
            case "Float":
                candidates = ("eq", "ne", "gt", "gte", "lt", "lte", "in", "nin");
                break;
            case "Boolean":
                candidates = ("eq", "ne");
                break;
            default:
                # String (and anything mapped to String)
                candidates = ("eq", "ne", "in", "nin", "like");
                break;
        }
        # gate on the provider's advertised operators when present
        list<string> ops = info.expressions
            ? (select candidates, info.expressions{FilterOpToDpql{$1}})
            : candidates;
        # always keep at least equality so the filter type is non-empty (valid GraphQL input type)
        if (!ops) {
            ops = ("eq",);
        }
        list<string> lines();
        foreach string op in (ops) {
            switch (op) {
                case "in":
                case "nin":
                    lines += sprintf("  %s: [%s!]", op, kind);
                    break;
                case "like":
                    lines += "  like: String";
                    break;
                default:
                    lines += sprintf("  %s: %s", op, kind);
                    break;
            }
        }
        return sprintf("input %sFilter {\n%s\n}", kind, lines.join("\n"));
    }

    #! upserts a record from the GraphQL arguments, returning the record id
    static string execUpsert(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        return p.upsertRecord(DataProviderGraphQLSchema::toProviderRec(spec, args));
    }

    #! validates a DPQL filter expression against the record schema and rejects template references
    /** @param spec the resolver spec (record type, supported expressions, allow_templates flag)
        @param dpql_where the DPQL filter string

        @throw GRAPHQL-EXECUTION-ERROR the filter is invalid, references unknown fields/operators,
        or (unless \c allow_templates is set) contains a template reference
    */
    static validateWhere(hash<auto> spec, string dpql_where) {
        # validate against the record fields (3rd arg) AND gate operators against the provider's
        # advertised server-side search expressions (4th arg, server_expressions) so a filter
        # cannot use an operator the provider does not actually support
        list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(dpql_where,
            spec.record_type, spec.expressions, spec.expressions);
        list<auto> errs = select diags, $1.severity == "error";
        if (errs) {
            throw GRAPHQL_EXECUTION_ERROR, sprintf("invalid filter expression: %s",
                (map $1.message, errs).join("; "));
        }
        if (!spec.allow_templates) {
            # parse with the same expression set used for validation, then walk the entire tree
            hash<DataProviderExpression> exp =
                DataProvider::parseDpqlExpression(dpql_where, spec.expressions);
            if (DataProviderGraphQLSchema::containsTemplateRef(exp)) {
                throw GRAPHQL_EXECUTION_ERROR, "template references are not permitted in filter "
                    "expressions";
            }
        }
    }

    #! converts a hash of GraphQL argument values to a provider record, recursively round-tripping
    #! sanitized GraphQL field names back to the provider's native names (including nested objects
    #! and lists of objects)
    static hash<auto> toProviderRec(hash<auto> spec, hash<auto> gqlArgs) {
        return DataProviderGraphQLSchema::toProviderRecFields(spec.record_type, gqlArgs);
    }

    #! recursively maps GraphQL input values to provider field names, driven by the provider's field
    #! schema (so nested object and list-of-object arguments round-trip their sanitized names too)
    /** @param fields the provider field schema (gql names are derived from these via
        @ref sanitizeName()); if @ref NOTHING the input is passed through unchanged
        @param gqlArgs the coerced GraphQL input hash (keyed by sanitized GraphQL names)

        @return a hash keyed by the provider's native field names
    */
    static hash<auto> toProviderRecFields(*hash<string, AbstractDataField> fields, hash<auto> gqlArgs) {
        if (!fields) {
            return gqlArgs;
        }
        hash<auto> out();
        foreach string fname in (keys fields) {
            string gqlName = DataProviderGraphQLSchema::sanitizeName(fname);
            if (!gqlArgs.hasKey(gqlName)) {
                continue;
            }
            out{fname} = DataProviderGraphQLSchema::convertInputValue(fields{fname}.getType(),
                gqlArgs{gqlName});
        }
        return out;
    }

    #! recursively converts a single GraphQL input value to its provider representation, descending
    #! into nested object types and lists of objects to round-trip field names
    static auto convertInputValue(AbstractDataProviderType t, auto v) {
        if (!exists v) {
            return v;
        }
        # a real list type has an element type; recurse into each element
        *AbstractDataProviderType et = t.getElementType();
        if (t.isList() && exists et && v.typeCode() == NT_LIST) {
            return map DataProviderGraphQLSchema::convertInputValue(et, $1), v;
        }
        *hash<string, AbstractDataField> fields = t.getFields();
        if (fields && v.typeCode() == NT_HASH) {
            return DataProviderGraphQLSchema::toProviderRecFields(fields, v);
        }
        return v;
    }

    #! recursively determines whether a parsed DPQL expression contains a template reference
    /** @param node a parsed DPQL node (@ref DataProviderExpression, @ref DataProviderFieldReference,
        @ref DataProviderTemplateReference, or a literal value)

        @return @ref True if a template reference appears anywhere in the expression tree
    */
    static bool containsTemplateRef(auto node) {
        switch (node.typeCode()) {
            case NT_HASH:
                # a DataProviderTemplateReference is identified by its "tmpl_context" member
                if (inlist("tmpl_context", keys node)) {
                    return True;
                }
                # recurse into every member value (not just expression args) so a template
                # reference nested in any node shape is detected
                foreach auto v in (node.values()) {
                    if (containsTemplateRef(v)) {
                        return True;
                    }
                }
                return False;
            case NT_LIST:
                foreach auto v in (node) {
                    if (containsTemplateRef(v)) {
                        return True;
                    }
                }
                return False;
            default:
                return False;
        }
    }

    #! maps a data provider field to a GraphQL type string
    /** @param field the data provider field
        @param force_optional if @ref True, the type is never marked non-null (used for the
        \c SetInput type where every field is optional)
    */
    private string mapFieldType(AbstractDataField field, string name_hint, bool force_optional) {
        return mapFieldTypeAtDepth(field, name_hint, 0, force_optional);
    }

    #! maps a field to a GraphQL type reference at the given nesting depth
    private string mapFieldTypeAtDepth(AbstractDataField field, string name_hint, int depth,
            bool force_optional) {
        string base = mapType(field.getType(), name_hint, depth);
        if (!force_optional && field.isMandatory()) {
            base += "!";
        }
        return base;
    }

    #! recursively maps a data provider type to a GraphQL type reference, generating nested object
    #! types and registering custom scalars (\c DateTime, \c Base64, \c JSON) as needed
    /** @param t the data provider type
        @param name_hint the name to use if a nested object type must be generated
        @param depth the current nesting depth; past @ref max_type_depth the type collapses to the
        \c JSON custom scalar (guards against unbounded/recursive types)
    */
    private string mapType(AbstractDataProviderType t, string name_hint, int depth) {
        if (depth > max_type_depth) {
            registerScalar("JSON");
            return "JSON";
        }
        # a real list type has a single element type; "soft" scalar types also report isList()
        # (they accept list input) but have no element type, so check for the element type
        *AbstractDataProviderType et = t.getElementType();
        if (t.isList() && exists et) {
            return sprintf("[%s!]", mapType(et, name_hint + "Item", depth + 1));
        }
        *hash<string, AbstractDataField> fields = t.getFields();
        if (fields) {
            return buildObjectType(name_hint, fields, depth);
        }
        return mapScalarType(t);
    }

    #! maps a leaf (scalar) data provider type to a GraphQL scalar name, registering any required
    #! custom scalar (\c DateTime, \c Base64, \c JSON); shared by the output and input type mappers
    private string mapScalarType(AbstractDataProviderType t) {
        switch (t.getBaseTypeCode()) {
            case NT_INT:
                return "Int";
            case NT_FLOAT:
            case NT_NUMBER:
                return "Float";
            case NT_BOOLEAN:
                return "Boolean";
            case NT_STRING:
                return "String";
            case NT_DATE:
                registerScalar("DateTime");
                return "DateTime";
            case NT_BINARY:
                registerScalar("Base64");
                return "Base64";
            case NT_HASH:
                # a free-form hash with no declared fields
                registerScalar("JSON");
                return "JSON";
            default:
                return "String";
        }
    }

    #! maps a data provider field to a GraphQL type string for use in input position (create/update/
    #! upsert arguments), generating nested \c input types for non-scalar fields
    private string mapInputFieldType(AbstractDataField field, string name_hint, bool force_optional) {
        return mapInputFieldTypeAtDepth(field, name_hint, 0, force_optional);
    }

    #! maps a field to a GraphQL input type reference at the given nesting depth
    private string mapInputFieldTypeAtDepth(AbstractDataField field, string name_hint, int depth,
            bool force_optional) {
        string base = mapInputType(field.getType(), name_hint, depth);
        if (!force_optional && field.isMandatory()) {
            base += "!";
        }
        return base;
    }

    #! recursively maps a data provider type to a GraphQL input type reference, generating nested
    #! \c input object types (rather than output object types) and registering custom scalars
    /** GraphQL requires a distinct \c input type for object values used as arguments; this mirrors
        @ref mapType() but emits \c input types so create/update/upsert arguments are valid.
    */
    private string mapInputType(AbstractDataProviderType t, string name_hint, int depth) {
        if (depth > max_type_depth) {
            registerScalar("JSON");
            return "JSON";
        }
        *AbstractDataProviderType et = t.getElementType();
        if (t.isList() && exists et) {
            return sprintf("[%s!]", mapInputType(et, name_hint + "Item", depth + 1));
        }
        *hash<string, AbstractDataField> fields = t.getFields();
        if (fields) {
            return buildInputType(name_hint + "Input", fields, depth);
        }
        return mapScalarType(t);
    }

    #! generates (once) a nested GraphQL object type for a hash of fields and returns its name
    private string buildObjectType(string type_name, hash<string, AbstractDataField> fields, int depth) {
        if (!exists extra_type_defs{type_name}) {
            # register a placeholder first so a recursive type does not loop indefinitely
            extra_type_defs{type_name} = "";
            list<string> lines();
            foreach string fn in (keys fields) {
                string gqlfn = sanitizeName(fn);
                lines += sprintf("  %s: %s", gqlfn,
                    mapFieldTypeAtDepth(fields{fn}, type_name + capitalize(gqlfn), depth + 1, False));
                # if the GraphQL field name was sanitized, the default resolver would read the wrong
                # key from the record hash, so register a resolver that reads the provider's name
                if (gqlfn != fn) {
                    output_field_resolvers{type_name}{gqlfn} = makeFieldReader(fn);
                }
            }
            extra_type_defs{type_name} = sprintf("type %s {\n%s\n}", type_name, lines.join("\n"));
        }
        return type_name;
    }

    #! generates (once) a nested GraphQL \c input object type for a hash of fields and returns its
    #! name; nested non-scalar fields recurse into further \c input types
    /** Input-side field-name round-tripping is handled structurally by @ref toProviderRecFields()
        (driven by the provider record type), so no per-field resolver registration is needed here.
    */
    private string buildInputType(string type_name, hash<string, AbstractDataField> fields, int depth) {
        if (!exists extra_input_defs{type_name}) {
            # register a placeholder first so a recursive type does not loop indefinitely
            extra_input_defs{type_name} = "";
            list<string> lines();
            foreach string fn in (keys fields) {
                string gqlfn = sanitizeName(fn);
                lines += sprintf("  %s: %s", gqlfn,
                    mapInputFieldTypeAtDepth(fields{fn}, type_name + capitalize(gqlfn), depth + 1, False));
            }
            extra_input_defs{type_name} = sprintf("input %s {\n%s\n}", type_name, lines.join("\n"));
        }
        return type_name;
    }

    #! returns a resolver that reads the given (provider) field name from the parent record
    static code makeFieldReader(string field_name) {
        return auto sub (auto parent, hash<auto> args, auto ctx) {
            return parent.typeCode() == NT_HASH ? parent{field_name} : NOTHING;
        };
    }

    #! records that a custom scalar must be declared in the SDL and coerced at runtime
    private registerScalar(string name) {
        needed_scalars{name} = True;
    }

    #! capitalizes the first character of a string
    static string capitalize(string s) {
        return s.empty() ? s : s[0].upr() + s.substr(1);
    }

    #! builds the custom-scalar coercer map for the scalars used by the generated schema
    private hash<auto> buildScalarCoercers() {
        hash<auto> c();
        if (needed_scalars.DateTime) {
            c.DateTime = {
                # input: ISO-8601 string -> Qore date; output: Qore date -> ISO-8601 string
                "input": auto sub (auto v) { return v.typeCode() == NT_DATE ? v : date(v); },
                "output": auto sub (auto v) {
                    return v.typeCode() == NT_DATE ? v.format("YYYY-MM-DDTHH:mm:SS") : v;
                },
            };
        }
        if (needed_scalars.Base64) {
            c.Base64 = {
                "input": auto sub (auto v) {
                    return v.typeCode() == NT_BINARY ? v : parse_base64_string(v);
                },
                "output": auto sub (auto v) {
                    return v.typeCode() == NT_BINARY ? make_base64_string(v) : v;
                },
            };
        }
        # the JSON scalar passes structured values through unchanged (no coercer)
        return c;
    }

    #! converts an arbitrary name to a valid GraphQL name (\c /[_A-Za-z][_0-9A-Za-z]*/)
    static string sanitizeName(string name) {
        string out = name;
        # replace any character that is not a valid GraphQL name character with an underscore
        out =~ s/[^_0-9A-Za-z]/_/g;
        # a name may not start with a digit
        if (out =~ /^[0-9]/) {
            out = "_" + out;
        }
        if (!out.val()) {
            out = "_";
        }
        return out;
    }
}
}
