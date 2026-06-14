# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLValue.qc helpers for converting between GraphQL value AST nodes and runtime values

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
#! Helpers for converting GraphQL value AST nodes to and from runtime Qore values
public class GraphQLValue {
    #! converts a value AST node to a runtime Qore value, resolving variable references
    /** @param node the value AST node
        @param variables the coerced variable values used to resolve @ref GraphQLVariable nodes

        @return the runtime value (\c NOTHING for a null value or an unset variable)
    */
    static auto valueFromAst(hash<GraphQLNode> node, hash<auto> variables) {
        switch (node.kind) {
            case NK_INT:
                return cast<hash<GraphQLIntValue>>(node).value;
            case NK_FLOAT:
                return cast<hash<GraphQLFloatValue>>(node).value;
            case NK_STRING:
                return cast<hash<GraphQLStringValue>>(node).value;
            case NK_BOOLEAN:
                return cast<hash<GraphQLBooleanValue>>(node).value;
            case NK_ENUM:
                return cast<hash<GraphQLEnumValue>>(node).value;
            case NK_NULL:
                return NOTHING;
            case NK_VARIABLE:
                return variables{cast<hash<GraphQLVariable>>(node).name};
            case NK_LIST: {
                list<auto> out();
                foreach hash<GraphQLNode> v in (cast<hash<GraphQLListValue>>(node).values) {
                    push out, valueFromAst(v, variables);
                }
                return out;
            }
            case NK_OBJECT: {
                hash<auto> out();
                foreach hash<GraphQLNode> fn in (cast<hash<GraphQLObjectValue>>(node).fields) {
                    hash<GraphQLObjectField> f = cast<hash<GraphQLObjectField>>(fn);
                    out{f.name} = valueFromAst(f.value, variables);
                }
                return out;
            }
            default:
                throw GRAPHQL_EXECUTION_ERROR, sprintf("cannot convert node of kind %y to a value",
                    node.kind);
        }
    }

    #! renders a value AST node as a GraphQL literal string (used for introspection default values)
    /** @param node the value AST node

        @return the GraphQL literal representation of the value
    */
    static string printAst(hash<GraphQLNode> node) {
        switch (node.kind) {
            case NK_INT:
                return sprintf("%d", cast<hash<GraphQLIntValue>>(node).value);
            case NK_FLOAT:
                return sprintf("%g", cast<hash<GraphQLFloatValue>>(node).value);
            case NK_STRING:
                return sprintf("%y", cast<hash<GraphQLStringValue>>(node).value);
            case NK_BOOLEAN:
                return cast<hash<GraphQLBooleanValue>>(node).value ? "true" : "false";
            case NK_ENUM:
                return cast<hash<GraphQLEnumValue>>(node).value;
            case NK_NULL:
                return "null";
            case NK_VARIABLE:
                return "$" + cast<hash<GraphQLVariable>>(node).name;
            case NK_LIST: {
                list<string> parts();
                foreach hash<GraphQLNode> v in (cast<hash<GraphQLListValue>>(node).values) {
                    push parts, printAst(v);
                }
                return "[" + parts.join(", ") + "]";
            }
            case NK_OBJECT: {
                list<string> parts();
                foreach hash<GraphQLNode> fn in (cast<hash<GraphQLObjectValue>>(node).fields) {
                    hash<GraphQLObjectField> f = cast<hash<GraphQLObjectField>>(fn);
                    push parts, sprintf("%s: %s", f.name, printAst(f.value));
                }
                return "{" + parts.join(", ") + "}";
            }
            default:
                return "null";
        }
    }
}
}
