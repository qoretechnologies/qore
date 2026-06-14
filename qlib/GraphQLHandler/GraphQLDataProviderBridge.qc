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

    @note Update and delete operate via the provider's native DPQL where-condition support;
    bulk operations and returning the affected records (rather than a count) are not yet
    generated. The @ref getSdl() output documents exactly what was generated.

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
        foreach string fname in (keys record_type) {
            string gqlName = sanitizeName(fname);
            field_name_map{gqlName} = fname;
            push fieldLines, sprintf("  %s: %s", gqlName, mapFieldType(record_type{fname}, False));
            # SetInput fields are always optional (a partial update assigns only supplied columns)
            push setLines, sprintf("  %s: %s", gqlName, mapFieldType(record_type{fname}, True));
        }

        # DPQL where-filtering requires the provider to support advanced search expressions;
        # update/delete are only generated when filtering is available, since their where argument
        # is mandatory (an unfiltered whole-table update/delete is intentionally not generated)
        bool can_filter = info.supports_search_expressions;
        bool gen_update = info.supports_update && can_filter;
        bool gen_delete = info.supports_delete && can_filter;

        list<string> defs();
        push defs, sprintf("type %s {\n%s\n}", record_type_name, fieldLines.join("\n"));

        # Query
        list<string> queryFields();
        if (info.supports_read) {
            queryFields += can_filter
                ? sprintf("  search(where: String, limit: Int, offset: Int): [%s!]!", record_type_name)
                : sprintf("  search(limit: Int, offset: Int): [%s!]!", record_type_name);
        }
        if (!queryFields) {
            queryFields += "  _empty: Boolean";
        }
        push defs, sprintf("type Query {\n%s\n}", queryFields.join("\n"));

        # Mutation
        list<string> mutationFields();
        if (info.supports_create) {
            mutationFields += sprintf("  create(%s): %s", createArgList(), record_type_name);
        }
        if (gen_update) {
            mutationFields += sprintf("  update(set: %sSetInput!, where: String!): Int!",
                record_type_name);
        }
        if (gen_delete) {
            mutationFields += "  delete(where: String!): Int!";
        }
        if (info.supports_upsert) {
            mutationFields += sprintf("  upsert(%s): String!", createArgList());
        }
        if (mutationFields) {
            push defs, sprintf("type Mutation {\n%s\n}", mutationFields.join("\n"));
        }

        # the SetInput type is only referenced by update
        if (gen_update) {
            push defs, sprintf("input %sSetInput {\n%s\n}", record_type_name, setLines.join("\n"));
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
        if (args.where.val()) {
            DataProviderGraphQLSchema::validateWhere(spec, args.where);
            it = p.searchRecords(args.where, {});
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

    #! updates records matching the DPQL filter, returning the number affected
    static int execUpdate(hash<auto> spec, hash<auto> args) {
        DataProviderGraphQLSchema::validateWhere(spec, args.where);
        AbstractDataProvider p = spec.provider;
        return p.updateRecords(DataProviderGraphQLSchema::toProviderRec(spec, args.set), args.where, {});
    }

    #! deletes records matching the DPQL filter, returning the number affected
    static int execDelete(hash<auto> spec, hash<auto> args) {
        DataProviderGraphQLSchema::validateWhere(spec, args.where);
        AbstractDataProvider p = spec.provider;
        return p.deleteRecords(args.where, {});
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
