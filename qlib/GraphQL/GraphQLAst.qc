# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLAst.qc GraphQL abstract syntax tree node definitions

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
    #! AST node kind: a complete document
    public const NK_DOCUMENT = "Document";
    #! AST node kind: an operation definition (query/mutation/subscription)
    public const NK_OPERATION = "OperationDefinition";
    #! AST node kind: a variable definition in an operation
    public const NK_VARIABLE_DEFINITION = "VariableDefinition";
    #! AST node kind: a selection set
    public const NK_SELECTION_SET = "SelectionSet";
    #! AST node kind: a field selection
    public const NK_FIELD = "Field";
    #! AST node kind: an argument
    public const NK_ARGUMENT = "Argument";
    #! AST node kind: a fragment spread (\c "...Name")
    public const NK_FRAGMENT_SPREAD = "FragmentSpread";
    #! AST node kind: an inline fragment
    public const NK_INLINE_FRAGMENT = "InlineFragment";
    #! AST node kind: a fragment definition
    public const NK_FRAGMENT_DEFINITION = "FragmentDefinition";
    #! AST node kind: a directive
    public const NK_DIRECTIVE = "Directive";

    #! AST node kind: a variable reference value (\c "$name")
    public const NK_VARIABLE = "Variable";
    #! AST node kind: an integer value
    public const NK_INT = "IntValue";
    #! AST node kind: a float value
    public const NK_FLOAT = "FloatValue";
    #! AST node kind: a string value
    public const NK_STRING = "StringValue";
    #! AST node kind: a boolean value
    public const NK_BOOLEAN = "BooleanValue";
    #! AST node kind: a null value
    public const NK_NULL = "NullValue";
    #! AST node kind: an enum value
    public const NK_ENUM = "EnumValue";
    #! AST node kind: a list value
    public const NK_LIST = "ListValue";
    #! AST node kind: an input object value
    public const NK_OBJECT = "ObjectValue";
    #! AST node kind: a field of an input object value
    public const NK_OBJECT_FIELD = "ObjectField";

    #! AST node kind: a named type reference
    public const NK_NAMED_TYPE = "NamedType";
    #! AST node kind: a list type reference (\c "[T]")
    public const NK_LIST_TYPE = "ListType";
    #! AST node kind: a non-null type reference (\c "T!")
    public const NK_NON_NULL_TYPE = "NonNullType";

#! base class for all GraphQL AST nodes
/** every node carries a @ref kind discriminator naming one of the \c NK_* constants; derived
    hashdecls add node-specific members. Heterogeneous collections are typed as
    \c list<hash<GraphQLNode>> and accessed by switching on @ref kind and casting to the
    appropriate derived hashdecl.
*/
public hashdecl GraphQLNode {
    #! the node kind; one of the \c NK_* constants
    string kind;
}

#! a parsed GraphQL document
public hashdecl GraphQLDocument inherits GraphQLNode {
    #! the list of top-level definitions (operations, fragments, or type-system definitions)
    list<hash<GraphQLNode>> definitions = ();
}

#! a GraphQL operation definition (query, mutation, or subscription)
public hashdecl GraphQLOperationDefinition inherits GraphQLNode {
    #! the operation type: \c "query", \c "mutation", or \c "subscription"
    string operation;
    #! the operation name, if any
    *string name;
    #! the variable definitions
    list<hash<GraphQLNode>> variableDefinitions = ();
    #! the directives applied to the operation
    list<hash<GraphQLNode>> directives = ();
    #! the top-level selection set
    hash<GraphQLNode> selectionSet;
}

#! a variable definition within an operation
public hashdecl GraphQLVariableDefinition inherits GraphQLNode {
    #! the variable name (without the leading \c "$")
    string name;
    #! the declared type reference
    hash<GraphQLNode> type;
    #! the default value node, if any
    *hash<GraphQLNode> defaultValue;
    #! the directives applied to the variable definition
    list<hash<GraphQLNode>> directives = ();
}

#! a selection set
public hashdecl GraphQLSelectionSet inherits GraphQLNode {
    #! the selections (fields, fragment spreads, inline fragments)
    list<hash<GraphQLNode>> selections = ();
}

#! a field selection
public hashdecl GraphQLField inherits GraphQLNode {
    #! the response-key alias, if any
    *string alias;
    #! the field name
    string name;
    #! the field arguments
    list<hash<GraphQLNode>> arguments = ();
    #! the directives applied to the field
    list<hash<GraphQLNode>> directives = ();
    #! the nested selection set, if any
    *hash<GraphQLNode> selectionSet;
}

#! an argument (name/value pair)
public hashdecl GraphQLArgument inherits GraphQLNode {
    #! the argument name
    string name;
    #! the argument value node
    hash<GraphQLNode> value;
}

#! a fragment spread (\c "...FragmentName")
public hashdecl GraphQLFragmentSpread inherits GraphQLNode {
    #! the referenced fragment name
    string name;
    #! the directives applied to the spread
    list<hash<GraphQLNode>> directives = ();
}

#! an inline fragment
public hashdecl GraphQLInlineFragment inherits GraphQLNode {
    #! the type condition (named type), if any
    *string typeCondition;
    #! the directives applied to the inline fragment
    list<hash<GraphQLNode>> directives = ();
    #! the fragment selection set
    hash<GraphQLNode> selectionSet;
}

#! a named fragment definition
public hashdecl GraphQLFragmentDefinition inherits GraphQLNode {
    #! the fragment name
    string name;
    #! the type condition (named type)
    string typeCondition;
    #! the directives applied to the fragment definition
    list<hash<GraphQLNode>> directives = ();
    #! the fragment selection set
    hash<GraphQLNode> selectionSet;
}

#! a directive application
public hashdecl GraphQLDirective inherits GraphQLNode {
    #! the directive name (without the leading \c "@")
    string name;
    #! the directive arguments
    list<hash<GraphQLNode>> arguments = ();
}

#! a variable reference used as a value (\c "$name")
public hashdecl GraphQLVariable inherits GraphQLNode {
    #! the variable name (without the leading \c "$")
    string name;
}

#! an integer value
public hashdecl GraphQLIntValue inherits GraphQLNode {
    #! the integer value
    int value;
}

#! a float value
public hashdecl GraphQLFloatValue inherits GraphQLNode {
    #! the float value
    float value;
}

#! a string value
public hashdecl GraphQLStringValue inherits GraphQLNode {
    #! the string value
    string value;
}

#! a boolean value
public hashdecl GraphQLBooleanValue inherits GraphQLNode {
    #! the boolean value
    bool value;
}

#! a null value is represented by the base @ref GraphQLNode with @ref NK_NULL as its kind
#! (it carries no additional members)

#! an enum value
public hashdecl GraphQLEnumValue inherits GraphQLNode {
    #! the enum value name
    string value;
}

#! a list value
public hashdecl GraphQLListValue inherits GraphQLNode {
    #! the element value nodes
    list<hash<GraphQLNode>> values = ();
}

#! an input object value
public hashdecl GraphQLObjectValue inherits GraphQLNode {
    #! the object field nodes
    list<hash<GraphQLNode>> fields = ();
}

#! a field of an input object value
public hashdecl GraphQLObjectField inherits GraphQLNode {
    #! the field name
    string name;
    #! the field value node
    hash<GraphQLNode> value;
}

#! a named type reference
public hashdecl GraphQLNamedType inherits GraphQLNode {
    #! the type name
    string name;
}

#! a list type reference (\c "[T]")
public hashdecl GraphQLListType inherits GraphQLNode {
    #! the inner type reference
    hash<GraphQLNode> type;
}

#! a non-null type reference (\c "T!")
public hashdecl GraphQLNonNullType inherits GraphQLNode {
    #! the inner (nullable) type reference
    hash<GraphQLNode> type;
}
}
