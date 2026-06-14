# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLSchema.qc GraphQL schema model and Schema Definition Language (SDL) parser

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
    #! schema type kind: scalar
    public const TYPE_SCALAR = "SCALAR";
    #! schema type kind: object
    public const TYPE_OBJECT = "OBJECT";
    #! schema type kind: interface
    public const TYPE_INTERFACE = "INTERFACE";
    #! schema type kind: union
    public const TYPE_UNION = "UNION";
    #! schema type kind: enum
    public const TYPE_ENUM = "ENUM";
    #! schema type kind: input object
    public const TYPE_INPUT_OBJECT = "INPUT_OBJECT";

    #! the names of the five built-in scalar types
    public const BuiltinScalars = ("Int", "Float", "String", "Boolean", "ID");

#! an input value definition (field argument or input object field)
public hashdecl GraphQLInputValueDef {
    #! the input value name
    string name;
    #! the description, if any
    *string description;
    #! the type reference (an AST type node: @ref GraphQLNamedType, @ref GraphQLListType, or @ref GraphQLNonNullType)
    hash<GraphQLNode> type;
    #! the default value (an AST value node), if any
    *hash<GraphQLNode> defaultValue;
}

#! a field definition on an object or interface type
public hashdecl GraphQLFieldDef {
    #! the field name
    string name;
    #! the description, if any
    *string description;
    #! the field type reference
    hash<GraphQLNode> type;
    #! the field arguments, keyed by argument name (insertion-ordered)
    hash<string, hash<GraphQLInputValueDef>> args = {};
}

#! a type definition in the schema
public hashdecl GraphQLTypeDef {
    #! the type kind; one of the \c TYPE_* constants
    string kind;
    #! the type name
    string name;
    #! the description, if any
    *string description;
    #! for @ref TYPE_OBJECT and @ref TYPE_INTERFACE: the fields, keyed by name (insertion-ordered)
    hash<string, hash<GraphQLFieldDef>> fields = {};
    #! for @ref TYPE_OBJECT: the names of implemented interfaces
    list<string> interfaces = ();
    #! for @ref TYPE_UNION: the member type names
    list<string> unionTypes = ();
    #! for @ref TYPE_ENUM: the enum value names
    list<string> enumValues = ();
    #! for @ref TYPE_INPUT_OBJECT: the input fields, keyed by name (insertion-ordered)
    hash<string, hash<GraphQLInputValueDef>> inputFields = {};
}

#! Parses GraphQL Schema Definition Language (SDL) into a list of type-system definitions
/** This parser reuses @ref GraphQLParserBase for shared value/type/directive productions and adds
    the type-system grammar (\c schema, \c scalar, \c type, \c interface, \c union, \c enum,
    \c input). The result is consumed by @ref GraphQLSchema to build the schema model.
*/
public class GraphQLSchemaParser inherits GraphQLParserBase {
    #! creates the parser for the given SDL source
    /** @param sdl the SDL source text

        @throw GRAPHQL-SYNTAX-ERROR the source contains an invalid token
    */
    constructor(string sdl) : GraphQLParserBase(sdl) {
    }

    #! parses SDL and returns the type definitions plus the root operation type mapping
    /** @return a hash with keys:
        - \c types: \c list of @ref GraphQLTypeDef "GraphQLTypeDef" hashes
        - \c roots: \c hash mapping operation type (\c query/\c mutation/\c subscription) to type name

        @throw GRAPHQL-SYNTAX-ERROR the source is not valid SDL
    */
    hash<auto> parseSchema() {
        list<hash<GraphQLTypeDef>> types();
        hash<string, string> roots();
        while (!at(GraphQLLexer::TK_EOF)) {
            # optional description (string literal preceding the definition)
            *string description;
            if (at(GraphQLLexer::TK_STRING)) {
                description = advance().value;
            }
            if (!at(GraphQLLexer::TK_NAME)) {
                throwSyntax("expected a type-system definition keyword");
            }
            string kw = peek().value;
            switch (kw) {
                case "schema":
                    roots += parseSchemaDef();
                    break;
                case "scalar":
                    push types, parseScalar(description);
                    break;
                case "type":
                    push types, parseObject(description);
                    break;
                case "interface":
                    push types, parseInterface(description);
                    break;
                case "union":
                    push types, parseUnion(description);
                    break;
                case "enum":
                    push types, parseEnum(description);
                    break;
                case "input":
                    push types, parseInputObject(description);
                    break;
                default:
                    throwSyntax(sprintf("unexpected keyword %y in SDL", kw));
            }
        }
        return {"types": types, "roots": roots};
    }

    #! parses a \c schema definition block
    private hash<string, string> parseSchemaDef() {
        expectKeyword("schema");
        parseDirectives(True);
        expect(GraphQLLexer::TK_BRACE_L);
        hash<string, string> roots();
        while (!at(GraphQLLexer::TK_BRACE_R)) {
            string op = expectName();
            if (op != "query" && op != "mutation" && op != "subscription") {
                throwSyntax(sprintf("invalid operation type %y in schema definition", op));
            }
            expect(GraphQLLexer::TK_COLON);
            roots{op} = expectName();
        }
        expect(GraphQLLexer::TK_BRACE_R);
        return roots;
    }

    #! parses a \c scalar type definition
    private hash<GraphQLTypeDef> parseScalar(*string description) {
        expectKeyword("scalar");
        string name = expectName();
        parseDirectives(True);
        return <GraphQLTypeDef>{"kind": TYPE_SCALAR, "name": name, "description": description};
    }

    #! parses an object \c type definition
    private hash<GraphQLTypeDef> parseObject(*string description) {
        expectKeyword("type");
        string name = expectName();
        list<string> interfaces = parseImplements();
        parseDirectives(True);
        hash<string, hash<GraphQLFieldDef>> fields = parseFieldDefs();
        return <GraphQLTypeDef>{
            "kind": TYPE_OBJECT,
            "name": name,
            "description": description,
            "interfaces": interfaces,
            "fields": fields,
        };
    }

    #! parses an \c interface type definition
    private hash<GraphQLTypeDef> parseInterface(*string description) {
        expectKeyword("interface");
        string name = expectName();
        list<string> interfaces = parseImplements();
        parseDirectives(True);
        hash<string, hash<GraphQLFieldDef>> fields = parseFieldDefs();
        return <GraphQLTypeDef>{
            "kind": TYPE_INTERFACE,
            "name": name,
            "description": description,
            "interfaces": interfaces,
            "fields": fields,
        };
    }

    #! parses an optional \c "implements A & B" clause
    private list<string> parseImplements() {
        list<string> ifaces();
        if (atKeyword("implements")) {
            advance();
            optional(GraphQLLexer::TK_AMP);
            push ifaces, expectName();
            while (optional(GraphQLLexer::TK_AMP)) {
                push ifaces, expectName();
            }
        }
        return ifaces;
    }

    #! parses a \c "{ FieldDefinition+ }" block
    private hash<string, hash<GraphQLFieldDef>> parseFieldDefs() {
        hash<string, hash<GraphQLFieldDef>> fields();
        if (optional(GraphQLLexer::TK_BRACE_L)) {
            while (!at(GraphQLLexer::TK_BRACE_R)) {
                *string fdesc;
                if (at(GraphQLLexer::TK_STRING)) {
                    fdesc = advance().value;
                }
                string fname = expectName();
                hash<string, hash<GraphQLInputValueDef>> args = parseArgumentDefs();
                expect(GraphQLLexer::TK_COLON);
                hash<GraphQLNode> type = parseTypeRef();
                parseDirectives(True);
                fields{fname} = <GraphQLFieldDef>{
                    "name": fname,
                    "description": fdesc,
                    "type": type,
                    "args": args,
                };
            }
            expect(GraphQLLexer::TK_BRACE_R);
        }
        return fields;
    }

    #! parses an optional \c "( InputValueDefinition+ )" argument-definition list
    private hash<string, hash<GraphQLInputValueDef>> parseArgumentDefs() {
        hash<string, hash<GraphQLInputValueDef>> args();
        if (optional(GraphQLLexer::TK_PAREN_L)) {
            while (!at(GraphQLLexer::TK_PAREN_R)) {
                args += parseInputValueDef();
            }
            expect(GraphQLLexer::TK_PAREN_R);
        }
        return args;
    }

    #! parses a single input value definition (used for arguments and input object fields)
    private hash<string, hash<GraphQLInputValueDef>> parseInputValueDef() {
        *string desc;
        if (at(GraphQLLexer::TK_STRING)) {
            desc = advance().value;
        }
        string name = expectName();
        expect(GraphQLLexer::TK_COLON);
        hash<GraphQLNode> type = parseTypeRef();
        *hash<GraphQLNode> defaultValue;
        if (optional(GraphQLLexer::TK_EQUALS)) {
            defaultValue = parseValue(True);
        }
        parseDirectives(True);
        return {
            name: <GraphQLInputValueDef>{
                "name": name,
                "description": desc,
                "type": type,
                "defaultValue": defaultValue,
            },
        };
    }

    #! parses a \c union type definition
    private hash<GraphQLTypeDef> parseUnion(*string description) {
        expectKeyword("union");
        string name = expectName();
        parseDirectives(True);
        list<string> members();
        if (optional(GraphQLLexer::TK_EQUALS)) {
            optional(GraphQLLexer::TK_PIPE);
            push members, expectName();
            while (optional(GraphQLLexer::TK_PIPE)) {
                push members, expectName();
            }
        }
        return <GraphQLTypeDef>{
            "kind": TYPE_UNION,
            "name": name,
            "description": description,
            "unionTypes": members,
        };
    }

    #! parses an \c enum type definition
    private hash<GraphQLTypeDef> parseEnum(*string description) {
        expectKeyword("enum");
        string name = expectName();
        parseDirectives(True);
        list<string> values();
        if (optional(GraphQLLexer::TK_BRACE_L)) {
            while (!at(GraphQLLexer::TK_BRACE_R)) {
                if (at(GraphQLLexer::TK_STRING)) {
                    advance();
                }
                string v = expectName();
                if (v == "true" || v == "false" || v == "null") {
                    throwSyntax(sprintf("invalid enum value %y", v));
                }
                parseDirectives(True);
                push values, v;
            }
            expect(GraphQLLexer::TK_BRACE_R);
        }
        return <GraphQLTypeDef>{
            "kind": TYPE_ENUM,
            "name": name,
            "description": description,
            "enumValues": values,
        };
    }

    #! parses an \c input object type definition
    private hash<GraphQLTypeDef> parseInputObject(*string description) {
        expectKeyword("input");
        string name = expectName();
        parseDirectives(True);
        hash<string, hash<GraphQLInputValueDef>> inputFields();
        if (optional(GraphQLLexer::TK_BRACE_L)) {
            while (!at(GraphQLLexer::TK_BRACE_R)) {
                inputFields += parseInputValueDef();
            }
            expect(GraphQLLexer::TK_BRACE_R);
        }
        return <GraphQLTypeDef>{
            "kind": TYPE_INPUT_OBJECT,
            "name": name,
            "description": description,
            "inputFields": inputFields,
        };
    }
}

#! An in-memory GraphQL schema: the type system plus root operation types
/** A schema is built from SDL text (or programmatically) and validated so that every referenced
    type exists. It is consumed by @ref GraphQLExecutor for validation and execution and by the
    introspection system.

    @par Example:
    @code{.py}
GraphQLSchema schema("type Query { hello: String }");
*hash<GraphQLTypeDef> qt = schema.getQueryType();
    @endcode
*/
public class GraphQLSchema {
    private {
        #! all types in the schema, keyed by name (includes built-in scalars)
        hash<string, hash<GraphQLTypeDef>> types();
        #! the query root type name
        *string query_type_name;
        #! the mutation root type name
        *string mutation_type_name;
        #! the subscription root type name
        *string subscription_type_name;
    }

    #! builds the schema from SDL source text
    /** @param sdl the SDL source defining the schema

        @throw GRAPHQL-SYNTAX-ERROR the SDL is syntactically invalid
        @throw GRAPHQL-SCHEMA-ERROR the SDL is syntactically valid but semantically invalid
        (e.g. references an undefined type, defines a type twice, or has no query type)
    */
    constructor(string sdl) {
        addBuiltinScalars();
        hash<auto> parsed = (new GraphQLSchemaParser(sdl)).parseSchema();
        foreach hash<GraphQLTypeDef> td in (parsed.types) {
            if (types{td.name}) {
                throw GRAPHQL_SCHEMA_ERROR, sprintf("type %y is defined more than once", td.name);
            }
            types{td.name} = td;
        }
        # resolve root operation types
        query_type_name = parsed.roots.query ?? (types.Query ? "Query" : NOTHING);
        mutation_type_name = parsed.roots.mutation ?? (types.Mutation ? "Mutation" : NOTHING);
        subscription_type_name = parsed.roots.subscription
            ?? (types.Subscription ? "Subscription" : NOTHING);
        validate();
    }

    #! registers the five built-in scalar types
    private addBuiltinScalars() {
        foreach string s in (BuiltinScalars) {
            types{s} = <GraphQLTypeDef>{"kind": TYPE_SCALAR, "name": s};
        }
    }

    #! validates the schema for internal consistency
    /** @throw GRAPHQL-SCHEMA-ERROR the schema references an undefined type or has no query type
    */
    private validate() {
        if (!exists query_type_name) {
            throw GRAPHQL_SCHEMA_ERROR, "the schema does not define a query root type "
                "(define a \"Query\" type or a \"schema { query: ... }\" block)";
        }
        foreach *string rt in ((query_type_name, mutation_type_name, subscription_type_name)) {
            if (exists rt && !types{rt}) {
                throw GRAPHQL_SCHEMA_ERROR, sprintf("root operation type %y is not defined", rt);
            }
        }
        # verify all referenced types exist
        foreach hash<GraphQLTypeDef> td in (types.iterator()) {
            foreach hash<GraphQLFieldDef> fd in (td.fields.iterator()) {
                checkTypeRefExists(fd.type, td.name, fd.name);
                foreach hash<GraphQLInputValueDef> arg in (fd.args.iterator()) {
                    checkTypeRefExists(arg.type, td.name, sprintf("%s.%s", fd.name, arg.name));
                }
            }
            foreach hash<GraphQLInputValueDef> ivd in (td.inputFields.iterator()) {
                checkTypeRefExists(ivd.type, td.name, ivd.name);
            }
            foreach string u in (td.unionTypes) {
                if (!types{u}) {
                    throw GRAPHQL_SCHEMA_ERROR, sprintf("union %y references undefined type %y",
                        td.name, u);
                }
            }
            foreach string i in (td.interfaces) {
                if (!types{i} || types{i}.kind != TYPE_INTERFACE) {
                    throw GRAPHQL_SCHEMA_ERROR, sprintf("type %y implements undefined interface %y",
                        td.name, i);
                }
            }
        }
    }

    #! throws if the named type referenced by a type node does not exist
    private checkTypeRefExists(hash<GraphQLNode> typeref, string owner, string member) {
        string name = getNamedTypeName(typeref);
        if (!types{name}) {
            throw GRAPHQL_SCHEMA_ERROR, sprintf("%s.%s references undefined type %y",
                owner, member, name);
        }
    }

    #! returns the underlying named type name of a (possibly list/non-null wrapped) type reference
    /** @param typeref an AST type node

        @return the innermost named type's name
    */
    static string getNamedTypeName(hash<GraphQLNode> typeref) {
        switch (typeref.kind) {
            case NK_NAMED_TYPE:
                return cast<hash<GraphQLNamedType>>(typeref).name;
            case NK_LIST_TYPE:
                return getNamedTypeName(cast<hash<GraphQLListType>>(typeref).type);
            case NK_NON_NULL_TYPE:
                return getNamedTypeName(cast<hash<GraphQLNonNullType>>(typeref).type);
            default:
                throw GRAPHQL_SCHEMA_ERROR, sprintf("invalid type reference node %y", typeref.kind);
        }
    }

    #! renders a type reference as a GraphQL type string (e.g. \c "[Int!]!")
    /** @param typeref an AST type node

        @return the GraphQL string representation of the type
    */
    static string typeRefToString(hash<GraphQLNode> typeref) {
        switch (typeref.kind) {
            case NK_NAMED_TYPE:
                return cast<hash<GraphQLNamedType>>(typeref).name;
            case NK_LIST_TYPE:
                return sprintf("[%s]", typeRefToString(cast<hash<GraphQLListType>>(typeref).type));
            case NK_NON_NULL_TYPE:
                return typeRefToString(cast<hash<GraphQLNonNullType>>(typeref).type) + "!";
            default:
                throw GRAPHQL_SCHEMA_ERROR, sprintf("invalid type reference node %y", typeref.kind);
        }
    }

    #! returns the type definition for the given name, or @ref NOTHING if not defined
    *hash<GraphQLTypeDef> getType(string name) {
        return types{name};
    }

    #! returns all type definitions keyed by name (including built-in scalars)
    hash<string, hash<GraphQLTypeDef>> getTypes() {
        return types;
    }

    #! returns the query root type definition, or @ref NOTHING if none
    *hash<GraphQLTypeDef> getQueryType() {
        return exists query_type_name ? types{query_type_name} : NOTHING;
    }

    #! returns the mutation root type definition, or @ref NOTHING if none
    *hash<GraphQLTypeDef> getMutationType() {
        return exists mutation_type_name ? types{mutation_type_name} : NOTHING;
    }

    #! returns the subscription root type definition, or @ref NOTHING if none
    *hash<GraphQLTypeDef> getSubscriptionType() {
        return exists subscription_type_name ? types{subscription_type_name} : NOTHING;
    }

    #! returns the root type definition for the given operation type
    /** @param operation one of \c "query", \c "mutation", or \c "subscription"

        @return the root type definition, or @ref NOTHING if the schema defines no such root
    */
    *hash<GraphQLTypeDef> getRootType(string operation) {
        switch (operation) {
            case "query": return getQueryType();
            case "mutation": return getMutationType();
            case "subscription": return getSubscriptionType();
            default: throw GRAPHQL_SCHEMA_ERROR, sprintf("invalid operation type %y", operation);
        }
    }
}
}
