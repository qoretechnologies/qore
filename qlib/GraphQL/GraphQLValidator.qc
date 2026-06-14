# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLValidator.qc GraphQL pre-execution document validation

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
#! Validates a parsed GraphQL document against a schema before execution
/** Implements the executable-document validation rules: operation-name uniqueness and the
    lone-anonymous-operation rule; fragment-name uniqueness, fragment-spread target existence,
    fragment cycles, unused fragments, and valid type conditions; variable uniqueness, all
    variables defined-and-used and used-are-defined; field existence on the type in scope,
    leaf/composite selection correctness, valid and present arguments, and valid directive
    locations. It also enforces a configurable query-complexity (total field count) budget.

    Validation collects all errors and returns them together (rather than aborting at the first),
    so a single request reports every problem at once.
*/
public class GraphQLValidator {
    public {
        #! the default maximum query complexity (total selected fields after fragment expansion)
        const DefaultMaxComplexity = 10000;
        #! the maximum selection-set nesting depth the validator will recurse
        const MaxDepth = 64;
        #! directive names allowed on fields/fragments by this engine
        const ExecutableDirectives = {"skip": True, "include": True};
    }

    private {
        #! the schema
        GraphQLSchema schema;
        #! the working type map (schema + introspection types)
        hash<string, hash<GraphQLTypeDef>> all_types;
        #! the maximum query complexity
        int max_complexity;
        #! the fragment definitions in the document, keyed by name
        hash<string, hash<GraphQLFragmentDefinition>> fragments;
        #! the accumulated validation errors
        list<auto> errors;
        #! the names of fragments referenced (used) anywhere
        hash<string, bool> used_fragments;
        #! the variable names used in the operation currently being validated
        hash<string, bool> used_vars;
        #! the running field count for the complexity budget
        int complexity;
        #! the current selection-set recursion depth (guards the validator's own stack)
        int depth = 0;
        #! set once the depth limit has been reported, to avoid repeated errors
        bool depth_exceeded = False;
    }

    #! creates the validator
    /** @param n_schema the schema to validate against
        @param n_all_types the working type map (schema types plus introspection types)
        @param n_max_complexity the maximum total field count (default @ref DefaultMaxComplexity)
    */
    constructor(GraphQLSchema n_schema, hash<string, hash<GraphQLTypeDef>> n_all_types,
            *int n_max_complexity) {
        schema = n_schema;
        all_types = n_all_types;
        max_complexity = n_max_complexity ?? DefaultMaxComplexity;
    }

    #! validates a document and returns the list of error hashes (empty if the document is valid)
    /** @param doc the parsed document

        @return a list of error hashes (each with a \c message); empty if the document is valid
    */
    list<auto> validate(hash<GraphQLDocument> doc) {
        errors = ();
        fragments = {};
        used_fragments = {};
        complexity = 0;

        # collect operations and fragments, checking name uniqueness
        list<hash<GraphQLOperationDefinition>> operations();
        hash<string, bool> opNames();
        bool hasAnonymous = False;
        foreach hash<GraphQLNode> def in (doc.definitions) {
            switch (def.kind) {
                case NK_OPERATION: {
                    hash<GraphQLOperationDefinition> op = cast<hash<GraphQLOperationDefinition>>(def);
                    push operations, op;
                    if (exists op.name) {
                        if (opNames{op.name}) {
                            addError(sprintf("there can be only one operation named %y", op.name));
                        }
                        opNames{op.name} = True;
                    } else {
                        hasAnonymous = True;
                    }
                    break;
                }
                case NK_FRAGMENT_DEFINITION: {
                    hash<GraphQLFragmentDefinition> fd = cast<hash<GraphQLFragmentDefinition>>(def);
                    if (fragments{fd.name}) {
                        addError(sprintf("there can be only one fragment named %y", fd.name));
                    }
                    fragments{fd.name} = fd;
                    break;
                }
            }
        }
        # the lone-anonymous-operation rule
        if (hasAnonymous && operations.size() > 1) {
            addError("an anonymous operation must be the only operation in the document");
        }

        # validate fragment type conditions and detect cycles
        foreach hash<GraphQLFragmentDefinition> fd in (fragments.iterator()) {
            if (!all_types{fd.typeCondition}) {
                addError(sprintf("fragment %y has an undefined type condition %y", fd.name,
                    fd.typeCondition));
            }
            checkFragmentCycle(fd.name, (fd.name,));
        }

        # validate each operation
        foreach hash<GraphQLOperationDefinition> op in (operations) {
            validateOperation(op);
        }

        # unused fragments
        foreach string fname in (keys fragments) {
            if (!used_fragments{fname}) {
                addError(sprintf("fragment %y is never used", fname));
            }
        }

        if (complexity > max_complexity) {
            addError(sprintf("query complexity %d exceeds the maximum of %d", complexity,
                max_complexity));
        }
        return errors;
    }

    #! validates a single operation
    private validateOperation(hash<GraphQLOperationDefinition> op) {
        *hash<GraphQLTypeDef> rootType = schema.getRootType(op.operation);
        if (!rootType) {
            addError(sprintf("the schema does not support %s operations", op.operation));
            return;
        }

        # variable definitions: uniqueness + input-type check
        hash<string, bool> declared();
        foreach hash<GraphQLNode> vdn in (op.variableDefinitions) {
            hash<GraphQLVariableDefinition> vd = cast<hash<GraphQLVariableDefinition>>(vdn);
            if (declared{vd.name}) {
                addError(sprintf("there can be only one variable named %y", vd.name));
            }
            declared{vd.name} = True;
            string tname = GraphQLSchema::getNamedTypeName(vd.type);
            *hash<GraphQLTypeDef> td = all_types{tname};
            if (!td) {
                addError(sprintf("variable %y has an undefined type %y", vd.name, tname));
            } else if (td.kind != TYPE_SCALAR && td.kind != TYPE_ENUM && td.kind != TYPE_INPUT_OBJECT) {
                addError(sprintf("variable %y must be an input type, not %s %y", vd.name, td.kind,
                    tname));
            }
        }

        # walk the selection set, collecting used variables into the used_vars member
        used_vars = {};
        validateSelectionSet(rootType, cast<hash<GraphQLSelectionSet>>(op.selectionSet), {});

        # all used variables must be declared, and all declared variables must be used
        foreach string uv in (keys used_vars) {
            if (!declared{uv}) {
                addError(sprintf("variable %y is used but not declared", uv));
            }
        }
        foreach string dv in (keys declared) {
            if (!used_vars{dv}) {
                addError(sprintf("variable %y is declared but never used", dv));
            }
        }
    }

    #! validates a selection set against the type in scope, collecting variable usages
    /** @param typeDef the composite type in scope
        @param selectionSet the selection set
        @param usedVars (in/out) collects variable names used in arguments/directives
        @param visited fragment names on the current spread path (cycle guard)
    */
    private validateSelectionSet(hash<GraphQLTypeDef> typeDef, hash<GraphQLSelectionSet> selectionSet,
            hash<string, bool> visited) {
        # guard the validator's own recursion against deeply nested queries
        if (++depth > MaxDepth) {
            --depth;
            if (!depth_exceeded) {
                depth_exceeded = True;
                addError(sprintf("query exceeds the maximum nesting depth of %d", MaxDepth));
            }
            return;
        }
        on_exit --depth;
        foreach hash<GraphQLNode> sel in (selectionSet.selections) {
            switch (sel.kind) {
                case NK_FIELD:
                    validateField(typeDef, cast<hash<GraphQLField>>(sel), visited);
                    break;
                case NK_INLINE_FRAGMENT: {
                    hash<GraphQLInlineFragment> inf = cast<hash<GraphQLInlineFragment>>(sel);
                    validateDirectives(inf.directives);
                    hash<GraphQLTypeDef> scope = typeDef;
                    if (exists inf.typeCondition) {
                        *hash<GraphQLTypeDef> ct = all_types{inf.typeCondition};
                        if (!ct) {
                            addError(sprintf("inline fragment has an undefined type condition %y",
                                inf.typeCondition));
                            break;
                        }
                        scope = ct;
                    }
                    validateSelectionSet(scope, cast<hash<GraphQLSelectionSet>>(inf.selectionSet),
                        visited);
                    break;
                }
                case NK_FRAGMENT_SPREAD: {
                    hash<GraphQLFragmentSpread> fs = cast<hash<GraphQLFragmentSpread>>(sel);
                    validateDirectives(fs.directives);
                    used_fragments{fs.name} = True;
                    *hash<GraphQLFragmentDefinition> frag = fragments{fs.name};
                    if (!frag) {
                        addError(sprintf("unknown fragment %y", fs.name));
                        break;
                    }
                    if (visited{fs.name}) {
                        # cycle already reported by checkFragmentCycle; stop recursing
                        break;
                    }
                    *hash<GraphQLTypeDef> ct = all_types{frag.typeCondition};
                    if (ct) {
                        validateSelectionSet(ct, cast<hash<GraphQLSelectionSet>>(frag.selectionSet),
                            visited + {fs.name: True});
                    }
                    break;
                }
            }
        }
    }

    #! validates a single field selection
    private validateField(hash<GraphQLTypeDef> typeDef, hash<GraphQLField> field,
            hash<string, bool> visited) {
        ++complexity;
        validateDirectives(field.directives);
        collectArgumentVars(field.arguments);

        # meta-fields are always valid
        if (field.name == "__typename") {
            return;
        }
        *hash<GraphQLFieldDef> fieldDef = getFieldDef(typeDef, field.name);
        if (!fieldDef) {
            # union types only permit __typename and inline fragments directly
            addError(sprintf("cannot query field %y on type %y", field.name, typeDef.name));
            return;
        }

        # argument validation: names exist, required present, no duplicates
        hash<string, bool> seen();
        foreach hash<GraphQLNode> an in (field.arguments) {
            hash<GraphQLArgument> a = cast<hash<GraphQLArgument>>(an);
            if (seen{a.name}) {
                addError(sprintf("duplicate argument %y on field %y", a.name, field.name));
            }
            seen{a.name} = True;
            if (!fieldDef.args{a.name}) {
                addError(sprintf("unknown argument %y on field %y", a.name, field.name));
            }
        }
        foreach hash<GraphQLInputValueDef> argDef in (fieldDef.args.iterator()) {
            if (argDef.type.kind == NK_NON_NULL_TYPE && !exists argDef.defaultValue
                    && !seen{argDef.name}) {
                addError(sprintf("field %y is missing required argument %y", field.name, argDef.name));
            }
        }

        # leaf vs composite selection-set correctness
        string tname = GraphQLSchema::getNamedTypeName(fieldDef.type);
        *hash<GraphQLTypeDef> ftd = all_types{tname};
        bool composite = ftd
            && (ftd.kind == TYPE_OBJECT || ftd.kind == TYPE_INTERFACE || ftd.kind == TYPE_UNION);
        if (composite) {
            if (!exists field.selectionSet) {
                addError(sprintf("field %y of type %y must have a selection set", field.name, tname));
            } else {
                validateSelectionSet(ftd, cast<hash<GraphQLSelectionSet>>(field.selectionSet),
                    visited);
            }
        } else if (exists field.selectionSet) {
            addError(sprintf("field %y of leaf type %y must not have a selection set", field.name,
                tname));
        }
    }

    #! validates directive locations and collects variables used in directive arguments
    private validateDirectives(list<hash<GraphQLNode>> directives) {
        foreach hash<GraphQLNode> dn in (directives) {
            hash<GraphQLDirective> d = cast<hash<GraphQLDirective>>(dn);
            if (!ExecutableDirectives{d.name}) {
                addError(sprintf("unknown or unsupported directive @%s", d.name));
            }
            collectArgumentVars(d.arguments);
        }
    }

    #! records variable references (into the used_vars member) in a list of arguments
    private collectArgumentVars(list<hash<GraphQLNode>> arguments) {
        foreach hash<GraphQLNode> an in (arguments) {
            collectValueVars(cast<hash<GraphQLArgument>>(an).value);
        }
    }

    #! recursively records variable references (into the used_vars member) in a value node
    private collectValueVars(hash<GraphQLNode> value) {
        switch (value.kind) {
            case NK_VARIABLE:
                used_vars{cast<hash<GraphQLVariable>>(value).name} = True;
                break;
            case NK_LIST:
                foreach hash<GraphQLNode> v in (cast<hash<GraphQLListValue>>(value).values) {
                    collectValueVars(v);
                }
                break;
            case NK_OBJECT:
                foreach hash<GraphQLNode> fn in (cast<hash<GraphQLObjectValue>>(value).fields) {
                    collectValueVars(cast<hash<GraphQLObjectField>>(fn).value);
                }
                break;
        }
    }

    #! detects a fragment-spread cycle reachable from a fragment, reporting it once
    private checkFragmentCycle(string startName, list<string> path) {
        *hash<GraphQLFragmentDefinition> frag = fragments{path[path.size() - 1]};
        if (!frag) {
            return;
        }
        foreach string spreadName in (collectSpreadNames(cast<hash<GraphQLSelectionSet>>(frag.selectionSet))) {
            if (spreadName == startName) {
                addError(sprintf("fragment %y forms a cycle: %s -> %s", startName,
                    path.join(" -> "), spreadName));
                continue;
            }
            if (!path.contains(spreadName) && fragments{spreadName}) {
                checkFragmentCycle(startName, path + spreadName);
            }
        }
    }

    #! collects the names of all fragment spreads directly within a selection set (recursively)
    private list<string> collectSpreadNames(hash<GraphQLSelectionSet> selectionSet) {
        list<string> out();
        foreach hash<GraphQLNode> sel in (selectionSet.selections) {
            switch (sel.kind) {
                case NK_FRAGMENT_SPREAD:
                    out += cast<hash<GraphQLFragmentSpread>>(sel).name;
                    break;
                case NK_FIELD: {
                    hash<GraphQLField> f = cast<hash<GraphQLField>>(sel);
                    if (exists f.selectionSet) {
                        out += collectSpreadNames(cast<hash<GraphQLSelectionSet>>(f.selectionSet));
                    }
                    break;
                }
                case NK_INLINE_FRAGMENT:
                    out += collectSpreadNames(
                        cast<hash<GraphQLSelectionSet>>(cast<hash<GraphQLInlineFragment>>(sel).selectionSet));
                    break;
            }
        }
        return out;
    }

    #! looks up a field definition on a type, including the introspection meta-fields
    private *hash<GraphQLFieldDef> getFieldDef(hash<GraphQLTypeDef> typeDef, string fieldName) {
        if (fieldName == "__schema" || fieldName == "__type") {
            *hash<GraphQLTypeDef> qt = schema.getQueryType();
            if (qt && typeDef.name == qt.name) {
                return GraphQLIntrospection::getMetaField(fieldName);
            }
        }
        return typeDef.fields{fieldName};
    }

    #! records a validation error
    private addError(string message) {
        push errors, {"message": message};
    }
}
}
