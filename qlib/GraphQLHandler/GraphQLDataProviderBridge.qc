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
      of records (with \c limit and \c offset arguments)
    - if the provider supports creating records, a \c create mutation field is generated whose
      arguments are the record's scalar fields

    The generated schema and resolvers can be retrieved directly, or wrapped in a ready-to-use
    @ref GraphQLExecutor or @ref GraphQLHandler.

    @note Update and delete operations are not generated in this version because they require a
    structured representation of search/where conditions as GraphQL input types; this is planned
    for a future release. The @ref getSdl() output documents exactly what was generated.

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
        #! the generated GraphQL object type name for the provider's record
        string record_type_name;
        #! the generated SDL
        string sdl;
        #! the generated resolver map
        hash<auto> resolvers;
    }

    #! builds the bridge for the given data provider
    /** @param n_provider the data provider to expose over GraphQL

        @throw GRAPHQL-SCHEMA-ERROR the provider does not expose a record type
    */
    constructor(DataProvider::AbstractDataProvider n_provider) {
        provider = n_provider;
        record_type_name = sanitizeName(provider.getName()) + "Record";
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

        @return a ready-to-use @ref GraphQLHandler
    */
    static GraphQLHandler createHandler(DataProvider::AbstractDataProvider provider,
            *HttpServer::AbstractAuthenticator auth) {
        DataProviderGraphQLSchema bridge(provider);
        return new GraphQLHandler(bridge.getExecutor(), auth);
    }

    #! builds a @ref GraphQLExecutor that executes against the given data provider
    /** @param provider the data provider to expose

        @return a ready-to-use @ref GraphQLExecutor
    */
    static GraphQLExecutor createExecutor(DataProvider::AbstractDataProvider provider) {
        return (new DataProviderGraphQLSchema(provider)).getExecutor();
    }

    #! generates the SDL and resolvers from the provider
    private generate() {
        *hash<string, AbstractDataField> recordType = provider.getRecordType();
        if (!recordType) {
            throw GRAPHQL_SCHEMA_ERROR, sprintf("data provider %y does not expose a record type "
                "and cannot be mapped to a GraphQL schema", provider.getName());
        }
        hash<DataProviderInfo> info = provider.getInfo();

        # map record fields to GraphQL field definitions
        list<string> fieldLines();
        hash<auto> fieldResolvers();
        foreach string fname in (keys recordType) {
            AbstractDataField field = recordType{fname};
            string gqlName = sanitizeName(fname);
            string gqlType = mapFieldType(field);
            push fieldLines, sprintf("  %s: %s", gqlName, gqlType);
        }

        # build the object type and root types
        list<string> defs();
        push defs, sprintf("type %s {\n%s\n}", record_type_name, fieldLines.join("\n"));

        list<string> queryFields();
        if (info.supports_read) {
            queryFields += sprintf("  search(limit: Int, offset: Int): [%s!]!", record_type_name);
        }
        # always provide at least one query field (a schema requires a query type)
        if (!queryFields) {
            queryFields += "  _empty: Boolean";
        }
        push defs, sprintf("type Query {\n%s\n}", queryFields.join("\n"));

        list<string> mutationFields();
        if (info.supports_create) {
            list<string> argLines();
            foreach string fname in (keys recordType) {
                argLines += sprintf("%s: %s", sanitizeName(fname), mapFieldType(recordType{fname}));
            }
            mutationFields += sprintf("  create(%s): %s", argLines.join(", "), record_type_name);
        }
        if (mutationFields) {
            push defs, sprintf("type Mutation {\n%s\n}", mutationFields.join("\n"));
        }

        sdl = defs.join("\n\n");

        # build resolvers
        AbstractDataProvider p = provider;
        string rtn = record_type_name;
        hash<auto> queryResolvers();
        if (info.supports_read) {
            queryResolvers.search = auto sub (auto parent, hash<auto> args, auto ctx) {
                return DataProviderGraphQLSchema::doSearch(p, args);
            };
        }
        resolvers = {"Query": queryResolvers};
        if (info.supports_create) {
            resolvers.Mutation = {
                "create": auto sub (auto parent, hash<auto> args, auto ctx) {
                    return p.createRecord(args);
                },
            };
        }
    }

    #! executes a search against the provider, applying limit/offset, and materializes the records
    static list<auto> doSearch(AbstractDataProvider p, hash<auto> args) {
        int limit = args.limit ?? DefaultSearchLimit;
        int offset = args.offset ?? 0;
        AbstractDataProviderRecordIterator it = p.searchRecords();
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

    #! maps a data provider field to a GraphQL type string, marking mandatory fields non-null
    private string mapFieldType(AbstractDataField field) {
        string base = mapScalarType(field.getType());
        if (field.getType().isList()) {
            base = sprintf("[%s]", base);
        }
        if (field.isMandatory()) {
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
