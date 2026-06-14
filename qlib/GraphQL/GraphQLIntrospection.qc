# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLIntrospection.qc GraphQL introspection type system and resolvers

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
#! Implements the GraphQL introspection system (\c __schema, \c __type, and the \c __* meta-types)
/** The introspection types (\c __Schema, \c __Type, \c __Field, \c __InputValue, \c __EnumValue,
    \c __Directive, \c __TypeKind, \c __DirectiveLocation) are defined in SDL and merged into the
    executor's working type map. Resolution is lazy (types are looked up by name on demand) so
    recursive and mutually-recursive schemas introspect without infinite expansion.

    An instance is created per @ref GraphQLExecutor and bound to the schema being introspected.
*/
public class GraphQLIntrospection {
    public {
        #! the SDL defining the introspection type system
        const IntrospectionSdl = "
            type __Schema {
                description: String
                types: [__Type!]!
                queryType: __Type!
                mutationType: __Type
                subscriptionType: __Type
                directives: [__Directive!]!
            }
            type __Type {
                kind: __TypeKind!
                name: String
                description: String
                fields(includeDeprecated: Boolean = false): [__Field!]
                interfaces: [__Type!]
                possibleTypes: [__Type!]
                enumValues(includeDeprecated: Boolean = false): [__EnumValue!]
                inputFields(includeDeprecated: Boolean = false): [__InputValue!]
                ofType: __Type
                specifiedByURL: String
            }
            type __Field {
                name: String!
                description: String
                args(includeDeprecated: Boolean = false): [__InputValue!]!
                type: __Type!
                isDeprecated: Boolean!
                deprecationReason: String
            }
            type __InputValue {
                name: String!
                description: String
                type: __Type!
                defaultValue: String
                isDeprecated: Boolean!
                deprecationReason: String
            }
            type __EnumValue {
                name: String!
                description: String
                isDeprecated: Boolean!
                deprecationReason: String
            }
            type __Directive {
                name: String!
                description: String
                locations: [__DirectiveLocation!]!
                args(includeDeprecated: Boolean = false): [__InputValue!]!
                isRepeatable: Boolean!
            }
            enum __TypeKind { SCALAR OBJECT INTERFACE UNION ENUM INPUT_OBJECT LIST NON_NULL }
            enum __DirectiveLocation {
                QUERY MUTATION SUBSCRIPTION FIELD FRAGMENT_DEFINITION FRAGMENT_SPREAD
                INLINE_FRAGMENT
            }
        ";

        #! the built-in directives reported by introspection
        const BuiltinDirectives = (
            {
                "name": "skip",
                "description": "Directs the executor to skip this field or fragment when the "
                    "`if` argument is true.",
                "locations": ("FIELD", "FRAGMENT_SPREAD", "INLINE_FRAGMENT"),
                "args": ({"name": "if", "type": "Boolean!"},),
                "isRepeatable": False,
            },
            {
                "name": "include",
                "description": "Directs the executor to include this field or fragment only when "
                    "the `if` argument is true.",
                "locations": ("FIELD", "FRAGMENT_SPREAD", "INLINE_FRAGMENT"),
                "args": ({"name": "if", "type": "Boolean!"},),
                "isRepeatable": False,
            },
        );
    }

    private {
        #! the schema being introspected
        GraphQLSchema schema;
        #! the working type map (schema types plus introspection types)
        hash<string, hash<GraphQLTypeDef>> all_types;
    }

    #! creates the introspection system bound to a schema and its working type map
    /** @param n_schema the schema
        @param n_all_types the working type map (schema types plus introspection types)
    */
    constructor(GraphQLSchema n_schema, hash<string, hash<GraphQLTypeDef>> n_all_types) {
        schema = n_schema;
        all_types = n_all_types;
    }

    #! parses and returns the introspection type definitions keyed by name
    /** @return the introspection \c GraphQLTypeDef hashes keyed by type name
    */
    static hash<string, hash<GraphQLTypeDef>> getIntrospectionTypes() {
        hash<auto> parsed = (new GraphQLSchemaParser(IntrospectionSdl)).parseSchema();
        hash<string, hash<GraphQLTypeDef>> out();
        foreach hash<GraphQLTypeDef> td in (parsed.types) {
            out{td.name} = td;
        }
        return out;
    }

    #! returns the field definition for a root introspection meta-field (\c __schema or \c __type)
    /** @param name either \c "__schema" or \c "__type"

        @return the @ref GraphQLFieldDef for the meta-field
    */
    static hash<GraphQLFieldDef> getMetaField(string name) {
        switch (name) {
            case "__schema":
                return <GraphQLFieldDef>{
                    "name": "__schema",
                    "type": nonNull(named("__Schema")),
                };
            case "__type":
                return <GraphQLFieldDef>{
                    "name": "__type",
                    "type": named("__Type"),
                    "args": {
                        "name": <GraphQLInputValueDef>{"name": "name", "type": nonNull(named("String"))},
                    },
                };
            default:
                throw GRAPHQL_SCHEMA_ERROR, sprintf("unknown introspection meta-field %y", name);
        }
    }

    #! builds a named type reference node
    static hash<GraphQLNode> named(string name) {
        return <GraphQLNamedType>{"kind": NK_NAMED_TYPE, "name": name};
    }

    #! builds a non-null type reference node wrapping the given type
    static hash<GraphQLNode> nonNull(hash<GraphQLNode> t) {
        return <GraphQLNonNullType>{"kind": NK_NON_NULL_TYPE, "type": t};
    }

    #! returns the resolver map for the introspection types
    /** @return a resolver map keyed by introspection type name then field name
    */
    hash<auto> getResolvers() {
        GraphQLIntrospection gi = self;
        return {
            "__Schema": {
                "description": auto sub (auto p, hash<auto> a, auto c) { return NOTHING; },
                "types": auto sub (auto p, hash<auto> a, auto c) { return gi.allTypeValues(); },
                "queryType": auto sub (auto p, hash<auto> a, auto c) {
                    return gi.rootTypeValue("query");
                },
                "mutationType": auto sub (auto p, hash<auto> a, auto c) {
                    return gi.rootTypeValue("mutation");
                },
                "subscriptionType": auto sub (auto p, hash<auto> a, auto c) {
                    return gi.rootTypeValue("subscription");
                },
                "directives": auto sub (auto p, hash<auto> a, auto c) {
                    return GraphQLIntrospection::BuiltinDirectives;
                },
            },
            "__Type": {
                "kind": auto sub (auto p, hash<auto> a, auto c) { return gi.typeKind(p); },
                "name": auto sub (auto p, hash<auto> a, auto c) { return gi.typeName(p); },
                "description": auto sub (auto p, hash<auto> a, auto c) { return gi.typeDescription(p); },
                "ofType": auto sub (auto p, hash<auto> a, auto c) { return p.ofType; },
                "fields": auto sub (auto p, hash<auto> a, auto c) { return gi.typeFields(p); },
                "interfaces": auto sub (auto p, hash<auto> a, auto c) { return gi.typeInterfaces(p); },
                "possibleTypes": auto sub (auto p, hash<auto> a, auto c) {
                    return gi.typePossibleTypes(p);
                },
                "enumValues": auto sub (auto p, hash<auto> a, auto c) { return gi.typeEnumValues(p); },
                "inputFields": auto sub (auto p, hash<auto> a, auto c) { return gi.typeInputFields(p); },
                "specifiedByURL": auto sub (auto p, hash<auto> a, auto c) { return NOTHING; },
            },
            "__Field": {
                "name": auto sub (auto p, hash<auto> a, auto c) { return p.name; },
                "description": auto sub (auto p, hash<auto> a, auto c) { return p.description; },
                "args": auto sub (auto p, hash<auto> a, auto c) { return gi.fieldArgs(p); },
                "type": auto sub (auto p, hash<auto> a, auto c) {
                    return GraphQLIntrospection::typeRefToValue(p.type);
                },
                "isDeprecated": auto sub (auto p, hash<auto> a, auto c) { return False; },
                "deprecationReason": auto sub (auto p, hash<auto> a, auto c) { return NOTHING; },
            },
            "__InputValue": {
                "name": auto sub (auto p, hash<auto> a, auto c) { return p.name; },
                "description": auto sub (auto p, hash<auto> a, auto c) { return p.description; },
                "type": auto sub (auto p, hash<auto> a, auto c) {
                    return GraphQLIntrospection::typeRefToValue(p.type);
                },
                "defaultValue": auto sub (auto p, hash<auto> a, auto c) {
                    return exists p.defaultValue ? GraphQLValue::printAst(p.defaultValue) : NOTHING;
                },
                "isDeprecated": auto sub (auto p, hash<auto> a, auto c) { return False; },
                "deprecationReason": auto sub (auto p, hash<auto> a, auto c) { return NOTHING; },
            },
            "__EnumValue": {
                "name": auto sub (auto p, hash<auto> a, auto c) { return p.name; },
                "description": auto sub (auto p, hash<auto> a, auto c) { return p.description; },
                "isDeprecated": auto sub (auto p, hash<auto> a, auto c) { return False; },
                "deprecationReason": auto sub (auto p, hash<auto> a, auto c) { return NOTHING; },
            },
            "__Directive": {
                "name": auto sub (auto p, hash<auto> a, auto c) { return p.name; },
                "description": auto sub (auto p, hash<auto> a, auto c) { return p.description; },
                "locations": auto sub (auto p, hash<auto> a, auto c) { return p.locations; },
                "isRepeatable": auto sub (auto p, hash<auto> a, auto c) { return p.isRepeatable; },
                "args": auto sub (auto p, hash<auto> a, auto c) { return gi.directiveArgs(p); },
            },
        };
    }

    #! returns the value representing the schema root (for the \c __schema meta-field)
    auto getSchemaValue() {
        return {"ref": "schema"};
    }

    #! returns the type value for a named type, or @ref NOTHING if not present (for \c __type)
    *hash<auto> getTypeValueByName(*string name) {
        if (!exists name || !all_types{name}) {
            return NOTHING;
        }
        return GraphQLIntrospection::namedValue(name);
    }

    #! builds a lazy type-value marker for a named type
    static hash<auto> namedValue(string name) {
        return {"ref": "name", "name": name};
    }

    #! converts an AST type reference to a lazy type-value marker
    static hash<auto> typeRefToValue(hash<GraphQLNode> typeref) {
        switch (typeref.kind) {
            case NK_NAMED_TYPE:
                return namedValue(cast<hash<GraphQLNamedType>>(typeref).name);
            case NK_LIST_TYPE:
                return {"ref": "list", "ofType": typeRefToValue(cast<hash<GraphQLListType>>(typeref).type)};
            case NK_NON_NULL_TYPE:
                return {"ref": "nonnull",
                    "ofType": typeRefToValue(cast<hash<GraphQLNonNullType>>(typeref).type)};
            default:
                throw GRAPHQL_SCHEMA_ERROR, sprintf("invalid type reference %y", typeref.kind);
        }
    }

    #! returns the list of type values for all types in the schema (including introspection types)
    list<auto> allTypeValues() {
        list<auto> out();
        foreach string name in (keys all_types) {
            push out, GraphQLIntrospection::namedValue(name);
        }
        return out;
    }

    #! returns the type value for a root operation type, or @ref NOTHING
    *hash<auto> rootTypeValue(string operation) {
        *hash<GraphQLTypeDef> td = schema.getRootType(operation);
        return td ? GraphQLIntrospection::namedValue(td.name) : NOTHING;
    }

    #! resolves the \c __TypeKind of a type value
    string typeKind(hash<auto> tv) {
        switch (tv.ref) {
            case "list":
                return "LIST";
            case "nonnull":
                return "NON_NULL";
            default:
                return all_types{tv.name}.kind;
        }
    }

    #! resolves the name of a type value (\c NOTHING for wrapper types)
    *string typeName(hash<auto> tv) {
        return tv.ref == "name" ? tv.name : NOTHING;
    }

    #! resolves the description of a type value
    *string typeDescription(hash<auto> tv) {
        return tv.ref == "name" ? all_types{tv.name}.description : NOTHING;
    }

    #! resolves the fields of an object/interface type value (\c NOTHING otherwise)
    *list<auto> typeFields(hash<auto> tv) {
        if (tv.ref != "name") {
            return NOTHING;
        }
        hash<GraphQLTypeDef> td = all_types{tv.name};
        if (td.kind != TYPE_OBJECT && td.kind != TYPE_INTERFACE) {
            return NOTHING;
        }
        list<auto> out();
        foreach hash<GraphQLFieldDef> fd in (td.fields.iterator()) {
            push out, fd;
        }
        return out;
    }

    #! resolves the implemented interfaces of an object/interface type value
    *list<auto> typeInterfaces(hash<auto> tv) {
        if (tv.ref != "name") {
            return NOTHING;
        }
        hash<GraphQLTypeDef> td = all_types{tv.name};
        if (td.kind != TYPE_OBJECT && td.kind != TYPE_INTERFACE) {
            return NOTHING;
        }
        return map GraphQLIntrospection::namedValue($1), td.interfaces;
    }

    #! resolves the possible concrete types of an interface or union type value
    *list<auto> typePossibleTypes(hash<auto> tv) {
        if (tv.ref != "name") {
            return NOTHING;
        }
        hash<GraphQLTypeDef> td = all_types{tv.name};
        if (td.kind == TYPE_UNION) {
            return map GraphQLIntrospection::namedValue($1), td.unionTypes;
        }
        if (td.kind == TYPE_INTERFACE) {
            list<auto> out();
            foreach hash<GraphQLTypeDef> t in (all_types.iterator()) {
                if (t.kind == TYPE_OBJECT && inlist(td.name, t.interfaces)) {
                    push out, GraphQLIntrospection::namedValue(t.name);
                }
            }
            return out;
        }
        return NOTHING;
    }

    #! resolves the enum values of an enum type value (\c NOTHING otherwise)
    *list<auto> typeEnumValues(hash<auto> tv) {
        if (tv.ref != "name") {
            return NOTHING;
        }
        hash<GraphQLTypeDef> td = all_types{tv.name};
        if (td.kind != TYPE_ENUM) {
            return NOTHING;
        }
        list<auto> out();
        foreach string v in (td.enumValues) {
            push out, {"name": v};
        }
        return out;
    }

    #! resolves the input fields of an input object type value (\c NOTHING otherwise)
    *list<auto> typeInputFields(hash<auto> tv) {
        if (tv.ref != "name") {
            return NOTHING;
        }
        hash<GraphQLTypeDef> td = all_types{tv.name};
        if (td.kind != TYPE_INPUT_OBJECT) {
            return NOTHING;
        }
        list<auto> out();
        foreach hash<GraphQLInputValueDef> ivd in (td.inputFields.iterator()) {
            push out, ivd;
        }
        return out;
    }

    #! resolves the arguments of a field value
    list<auto> fieldArgs(hash<GraphQLFieldDef> fd) {
        list<auto> out();
        foreach hash<GraphQLInputValueDef> ivd in (fd.args.iterator()) {
            push out, ivd;
        }
        return out;
    }

    #! resolves the arguments of a directive value (building input-value-like hashes)
    list<auto> directiveArgs(hash<auto> d) {
        list<auto> out();
        foreach hash<auto> arg in (d.args) {
            push out, <GraphQLInputValueDef>{
                "name": arg.name,
                "type": simpleTypeRef(arg.type),
            };
        }
        return out;
    }

    #! builds a type reference node from a simple type string (a name optionally followed by \c "!")
    static hash<GraphQLNode> simpleTypeRef(string spec) {
        if (spec =~ /!$/) {
            string base = spec;
            base =~ s/!$//;
            return nonNull(named(base));
        }
        return named(spec);
    }
}
}
