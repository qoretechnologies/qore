# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLDataProviderBridge.qc generates a GraphQL schema and resolvers from a DataProvider

/*  GraphQLHandler Copyright (C) 2026 Qore Technologies, s.r.o.

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

    @par Example:
    @code{.py}
GraphQLHandler gh = DataProviderGraphQLSchema::createHandler(my_data_provider);
    @endcode
*/
public class DataProviderGraphQLSchema {
    public {
        #! the default maximum number of records returned by a generated \c search field
        const DefaultSearchLimit = 1000;
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

    #! returns a @ref GraphQLExecutor built from the generated schema and resolvers
    GraphQLExecutor getExecutor() {
        return new GraphQLExecutor(new GraphQLSchema(sdl), resolvers);
    }

    #! builds a @ref GraphQLHandler that serves the given data provider over HTTP
    /** @param provider the data provider to expose
        @param auth an optional authenticator for the handler
        @param opts optional bridge options (see the constructor)

        @return a ready-to-use @ref GraphQLHandler
    */
    static GraphQLHandler createHandler(DataProvider::AbstractDataProvider provider,
            *HttpServer::AbstractAuthenticator auth, *hash<auto> opts) {
        DataProviderGraphQLSchema bridge(provider, opts);
        return new GraphQLHandler(bridge.getExecutor(), auth);
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
        list<string> setLines();
        list<string> whereLines();
        hash<string, bool> usedFilterKinds();
        foreach string fname in (keys record_type) {
            string gqlName = sanitizeName(fname);
            field_name_map{gqlName} = fname;
            push fieldLines, sprintf("  %s: %s", gqlName, mapFieldType(record_type{fname}, False));
            # SetInput fields are always optional (a partial update assigns only supplied columns)
            push setLines, sprintf("  %s: %s", gqlName, mapFieldType(record_type{fname}, True));
            # WhereInput: each scalar field gets a typed per-scalar filter
            string fkind = scalarFilterKind(record_type{fname}.getType());
            usedFilterKinds{fkind} = True;
            push whereLines, sprintf("  %s: %sFilter", gqlName, fkind);
        }

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
            mutationFields += sprintf("  create(%s): %s", createArgList(), record_type_name);
        }
        if (gen_update) {
            mutationFields += sprintf("  update(set: %sSetInput!, %s): %s",
                record_type_name, filterArgs, mutationReturn);
        }
        if (gen_delete) {
            mutationFields += sprintf("  delete(%s): %s", filterArgs, mutationReturn);
        }
        if (info.supports_upsert) {
            mutationFields += sprintf("  upsert(%s): String!", createArgList());
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
        # the record Input type is referenced by the bulk operations
        if (gen_bulk_create || gen_bulk_upsert) {
            push defs, sprintf("input %s {\n%s\n}", recordInputName, fieldLines.join("\n"));
        }

        # the typed filter input types (only when filtering is available)
        if (can_filter) {
            # per-scalar filter input types actually used by the record's fields
            foreach string fkind in (keys usedFilterKinds) {
                push defs, scalarFilterTypeSdl(fkind);
            }
            # the record's WhereInput, with recursive logical combinators
            push whereLines, sprintf("  _and: [%s!]", whereInputName);
            push whereLines, sprintf("  _or: [%s!]", whereInputName);
            push whereLines, sprintf("  _not: %s", whereInputName);
            push defs, sprintf("input %s {\n%s\n}", whereInputName, whereLines.join("\n"));
        }

        sdl = defs.join("\n\n");

        # Build resolvers as closures over an immutable "spec" value (NOT over the bridge object):
        # the bridge instance may be a temporary (e.g. from createExecutor()) destroyed at scope
        # exit, so the resolvers must not depend on its lifetime. The spec holds the provider
        # reference and the metadata the static resolver helpers need.
        hash<auto> spec = {
            "provider": provider,
            "record_type": record_type,
            "expressions": info.expressions,
            "name_map": field_name_map,
            "allow_templates": allow_templates,
            "return_records": return_records,
        };

        hash<auto> queryResolvers();
        if (info.supports_read) {
            queryResolvers.search = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::execSearch(spec, args);
            };
        }
        resolvers = {"Query": queryResolvers};

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

    #! builds the comma-separated argument list for the create/upsert mutation fields
    private string createArgList() {
        list<string> argLines();
        foreach string fname in (keys record_type) {
            argLines += sprintf("%s: %s", sanitizeName(fname), mapFieldType(record_type{fname}, False));
        }
        return argLines.join(", ");
    }

    #! executes a search against the provider, applying the optional filter and limit/offset paging
    static list<auto> execSearch(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        AbstractDataProviderRecordIterator it;
        *string dpql = DataProviderGraphQLSchema::buildEffectiveDpql(spec, args);
        if (dpql.val()) {
            DataProviderGraphQLSchema::validateWhere(spec, dpql);
            it = p.searchRecords(dpql, {});
        } else {
            it = p.searchRecords();
        }
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

    #! creates a record from the GraphQL arguments
    static *hash<auto> execCreate(hash<auto> spec, hash<auto> args) {
        AbstractDataProvider p = spec.provider;
        return p.createRecord(DataProviderGraphQLSchema::toProviderRec(spec, args));
    }

    #! updates records matching the filter; returns the affected records or the affected count
    static auto execUpdate(hash<auto> spec, hash<auto> args) {
        string dpql = DataProviderGraphQLSchema::requireSelector(spec, args, "update");
        AbstractDataProvider p = spec.provider;
        int n = p.updateRecords(DataProviderGraphQLSchema::toProviderRec(spec, args.set), dpql, {});
        # when returning records, re-read the (now updated) matching records (non-atomic)
        return spec.return_records ? DataProviderGraphQLSchema::collectMatching(p, dpql) : n;
    }

    #! deletes records matching the filter; returns the affected records or the affected count
    static auto execDelete(hash<auto> spec, hash<auto> args) {
        string dpql = DataProviderGraphQLSchema::requireSelector(spec, args, "delete");
        AbstractDataProvider p = spec.provider;
        # when returning records, capture the matching records before deleting them (non-atomic)
        *list<auto> captured = spec.return_records
            ? DataProviderGraphQLSchema::collectMatching(p, dpql)
            : NOTHING;
        int n = p.deleteRecords(dpql, {});
        return spec.return_records ? captured : n;
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
                    list<string> subs = map "(" + DataProviderGraphQLSchema::whereInputToDpql(spec, $1) + ")",
                        filter._and;
                    clauses += subs.join(" && ");
                    break;
                }
                case "_or": {
                    list<string> subs = map "(" + DataProviderGraphQLSchema::whereInputToDpql(spec, $1) + ")",
                        filter._or;
                    clauses += "(" + subs.join(" || ") + ")";
                    break;
                }
                case "_not":
                    clauses += "!(" + DataProviderGraphQLSchema::whereInputToDpql(spec, filter._not) + ")";
                    break;
                default: {
                    # a per-field filter
                    string provName = name_map{key} ?? key;
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

    #! returns the per-scalar filter input-type kind name for a data provider type (\c Int, \c Float, ...)
    private string scalarFilterKind(AbstractDataProviderType t) {
        switch (t.getBaseTypeCode()) {
            case NT_INT:
                return "Int";
            case NT_FLOAT:
            case NT_NUMBER:
                return "Float";
            case NT_BOOLEAN:
                return "Boolean";
            default:
                # strings, dates, hashes, binary, etc. are filtered as strings
                return "String";
        }
    }

    #! returns the SDL definition of the per-scalar filter input type for the given kind
    static string scalarFilterTypeSdl(string kind) {
        switch (kind) {
            case "Int":
            case "Float":
                return sprintf("input %sFilter {\n  eq: %s\n  ne: %s\n  gt: %s\n  gte: %s\n  lt: %s\n"
                    "  lte: %s\n  in: [%s!]\n  nin: [%s!]\n}", kind, kind, kind, kind, kind, kind,
                    kind, kind, kind);
            case "Boolean":
                return "input BooleanFilter {\n  eq: Boolean\n  ne: Boolean\n}";
            default:
                # String (and anything mapped to String)
                return "input StringFilter {\n  eq: String\n  ne: String\n  in: [String!]\n"
                    "  nin: [String!]\n  like: String\n}";
        }
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
        list<hash<DpqlDiagnostic>> diags =
            DataProvider::validateDpqlExpression(dpql_where, spec.record_type, spec.expressions);
        list<auto> errs = select diags, $1.severity == "error";
        if (errs) {
            throw GRAPHQL_EXECUTION_ERROR, sprintf("invalid filter expression: %s",
                (map $1.message, errs).join("; "));
        }
        if (!spec.allow_templates) {
            hash<DataProviderExpression> exp = DataProvider::parseDpqlExpression(dpql_where);
            if (DataProviderGraphQLSchema::containsTemplateRef(exp)) {
                throw GRAPHQL_EXECUTION_ERROR, "template references are not permitted in filter "
                    "expressions";
            }
        }
    }

    #! rekeys a hash of GraphQL argument names to the provider's native field names
    static hash<auto> toProviderRec(hash<auto> spec, hash<auto> gqlArgs) {
        hash<auto> name_map = spec.name_map;
        hash<auto> out();
        foreach string k in (keys gqlArgs) {
            out{name_map{k} ?? k} = gqlArgs{k};
        }
        return out;
    }

    #! recursively determines whether a parsed DPQL expression contains a template reference
    /** @param node a parsed DPQL node (@ref DataProviderExpression, @ref DataProviderFieldReference,
        @ref DataProviderTemplateReference, or a literal value)

        @return @ref True if a template reference appears anywhere in the expression tree
    */
    static bool containsTemplateRef(auto node) {
        if (node.typeCode() != NT_HASH) {
            return False;
        }
        list<string> k = keys node;
        # a DataProviderTemplateReference is identified by its "tmpl_context" member
        if (inlist("tmpl_context", k)) {
            return True;
        }
        # a DataProviderExpression has "exp" + "args"; recurse into the arguments
        if (inlist("exp", k) && inlist("args", k)) {
            foreach auto arg in (node.args) {
                if (containsTemplateRef(arg)) {
                    return True;
                }
            }
        }
        return False;
    }

    #! maps a data provider field to a GraphQL type string
    /** @param field the data provider field
        @param force_optional if @ref True, the type is never marked non-null (used for the
        \c SetInput type where every field is optional)
    */
    private string mapFieldType(AbstractDataField field, bool force_optional) {
        string base = mapScalarType(field.getType());
        if (field.getType().isList()) {
            base = sprintf("[%s]", base);
        }
        if (!force_optional && field.isMandatory()) {
            base += "!";
        }
        return base;
    }

    #! maps a data provider type to a GraphQL scalar type name
    /** Non-scalar Qore types (hashes, lists of objects, dates, etc.) are mapped to \c String;
        resolvers serialize such values via their default string representation.
    */
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
            default:
                # dates, hashes, binary, and anything else are exposed as String
                return "String";
        }
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
