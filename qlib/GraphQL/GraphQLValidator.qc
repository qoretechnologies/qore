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
    leaf/composite selection correctness, valid and present arguments, valid directive locations,
    and field-selection merging (selections sharing a response key must be mergeable). It also
    enforces a configurable query-complexity (total field count) budget.

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
        # reset the recursion-tracking state so a prior validation that hit MaxDepth does not
        # suppress the depth check on this (independent) document
        depth = 0;
        depth_exceeded = False;

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
        hash<GraphQLSelectionSet> ss = cast<hash<GraphQLSelectionSet>>(op.selectionSet);
        validateSelectionSet(rootType, ss, {});

        # field-selection-merging rule (spec 5.3.2): selections sharing a response key must be
        # mergeable (compatible response shapes; same field+arguments when on the same object type)
        fieldsInSetCanMerge(collectMergeFields(rootType.name, ss, {}), 0);

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

    #! flattens a selection set into per-field merge entries, expanding fragments and inline
    #! fragments and tracking the type each field is selected on
    /** @param parentTypeName the name of the composite type the selection set is scoped to
        @param selectionSet the selection set
        @param visited fragment names on the current spread path (cycle guard)

        @return a list of \c {key, field, parentType, fieldDef} hashes, one per field occurrence
    */
    private list<auto> collectMergeFields(string parentTypeName, hash<GraphQLSelectionSet> selectionSet,
            hash<string, bool> visited) {
        list<auto> out();
        *hash<GraphQLTypeDef> parentType = all_types{parentTypeName};
        foreach hash<GraphQLNode> sel in (selectionSet.selections) {
            switch (sel.kind) {
                case NK_FIELD: {
                    hash<GraphQLField> f = cast<hash<GraphQLField>>(sel);
                    *hash<GraphQLFieldDef> fd = parentType ? getFieldDef(parentType, f.name) : NOTHING;
                    out += {
                        "key": f.alias ?? f.name,
                        "field": f,
                        "parentType": parentTypeName,
                        "fieldDef": fd,
                    };
                    break;
                }
                case NK_INLINE_FRAGMENT: {
                    hash<GraphQLInlineFragment> inf = cast<hash<GraphQLInlineFragment>>(sel);
                    string scope = inf.typeCondition ?? parentTypeName;
                    out += collectMergeFields(scope,
                        cast<hash<GraphQLSelectionSet>>(inf.selectionSet), visited);
                    break;
                }
                case NK_FRAGMENT_SPREAD: {
                    hash<GraphQLFragmentSpread> fs = cast<hash<GraphQLFragmentSpread>>(sel);
                    if (visited{fs.name}) {
                        break;
                    }
                    *hash<GraphQLFragmentDefinition> frag = fragments{fs.name};
                    if (!frag) {
                        break;
                    }
                    out += collectMergeFields(frag.typeCondition,
                        cast<hash<GraphQLSelectionSet>>(frag.selectionSet), visited + {fs.name: True});
                    break;
                }
            }
        }
        return out;
    }

    #! verifies that all fields sharing a response key in a (flattened) selection set can be merged,
    #! recursing into their merged sub-selection sets
    private fieldsInSetCanMerge(list<auto> entries, int mdepth) {
        if (mdepth > MaxDepth) {
            return;
        }
        # group the entries by response key, preserving insertion order
        hash<string, list<auto>> groups();
        foreach hash<auto> e in (entries) {
            groups{e.key} = (groups{e.key} ?? ()) + e;
        }
        foreach string key in (keys groups) {
            list<auto> group = groups{key};
            bool mergeable = True;
            # pairwise comparison of all members sharing this response key
            for (int i = 0; i < group.size() && mergeable; ++i) {
                for (int j = i + 1; j < group.size(); ++j) {
                    if (!sameResponseShape(group[i], group[j])) {
                        addError(sprintf("fields %y conflict because they return conflicting types; "
                            "use different aliases on the fields to fetch both", key));
                        mergeable = False;
                        break;
                    }
                    # when the fields are on the same object type (or either parent is not an object
                    # type, so they could apply to the same object), they must be the same field
                    # with the same arguments
                    if (canBeSameObject(group[i].parentType, group[j].parentType)) {
                        hash<GraphQLField> fa = group[i].field;
                        hash<GraphQLField> fb = group[j].field;
                        if (fa.name != fb.name) {
                            addError(sprintf("fields %y conflict because %y and %y are different "
                                "fields; use different aliases to fetch both", key, fa.name, fb.name));
                            mergeable = False;
                            break;
                        }
                        if (!sameArguments(fa, fb)) {
                            addError(sprintf("fields %y conflict because they are given different "
                                "arguments; use different aliases to fetch both", key));
                            mergeable = False;
                            break;
                        }
                    }
                }
            }
            # recurse into the merged sub-selection sets of a mergeable group
            if (mergeable) {
                list<auto> merged();
                foreach hash<auto> e in (group) {
                    if (exists e.field.selectionSet && exists e.fieldDef) {
                        merged += collectMergeFields(GraphQLSchema::getNamedTypeName(e.fieldDef.type),
                            cast<hash<GraphQLSelectionSet>>(e.field.selectionSet), {});
                    }
                }
                if (merged) {
                    fieldsInSetCanMerge(merged, mdepth + 1);
                }
            }
        }
    }

    #! returns @ref True if two merge entries have compatible response shapes (spec SameResponseShape)
    private bool sameResponseShape(hash<auto> a, hash<auto> b) {
        # a meta-field/unknown field has no field definition; the field-existence rule reports that
        # separately, so treat the shape as compatible here
        if (!exists a.fieldDef || !exists b.fieldDef) {
            return True;
        }
        return typeShapesCompatible(a.fieldDef.type, b.fieldDef.type);
    }

    #! recursively compares two type references for response-shape compatibility
    private bool typeShapesCompatible(hash<GraphQLNode> ta, hash<GraphQLNode> tb) {
        # unwrap non-null wrappers in lockstep
        while (ta.kind == NK_NON_NULL_TYPE || tb.kind == NK_NON_NULL_TYPE) {
            if (ta.kind != tb.kind) {
                return False;
            }
            ta = cast<hash<GraphQLNonNullType>>(ta).type;
            tb = cast<hash<GraphQLNonNullType>>(tb).type;
        }
        # unwrap list wrappers in lockstep
        if (ta.kind == NK_LIST_TYPE || tb.kind == NK_LIST_TYPE) {
            if (ta.kind != tb.kind) {
                return False;
            }
            return typeShapesCompatible(cast<hash<GraphQLListType>>(ta).type,
                cast<hash<GraphQLListType>>(tb).type);
        }
        # named types: leaf (scalar/enum) types must be identical; two composite types are
        # shape-compatible (their fields are checked by the recursive merge of the merged subset)
        string na = GraphQLSchema::getNamedTypeName(ta);
        string nb = GraphQLSchema::getNamedTypeName(tb);
        *hash<GraphQLTypeDef> tda = all_types{na};
        *hash<GraphQLTypeDef> tdb = all_types{nb};
        bool leafA = tda && (tda.kind == TYPE_SCALAR || tda.kind == TYPE_ENUM);
        bool leafB = tdb && (tdb.kind == TYPE_SCALAR || tdb.kind == TYPE_ENUM);
        if (leafA || leafB) {
            return na == nb;
        }
        return True;
    }

    #! returns @ref True if two parent types could be the same concrete object at runtime
    /** True when the type names are equal, or when either is not an object type (so a field on an
        interface/union could resolve to the same object as a field declared elsewhere)
    */
    private bool canBeSameObject(string a, string b) {
        if (a == b) {
            return True;
        }
        *hash<GraphQLTypeDef> ta = all_types{a};
        *hash<GraphQLTypeDef> tb = all_types{b};
        return !(ta && ta.kind == TYPE_OBJECT) || !(tb && tb.kind == TYPE_OBJECT);
    }

    #! returns @ref True if two fields have identical argument sets (names and values)
    private bool sameArguments(hash<GraphQLField> fa, hash<GraphQLField> fb) {
        hash<auto> ma = argumentMap(fa.arguments);
        hash<auto> mb = argumentMap(fb.arguments);
        if (ma.size() != mb.size()) {
            return False;
        }
        foreach string name in (keys ma) {
            if (!exists mb{name} || !valuesEqual(ma{name}, mb{name})) {
                return False;
            }
        }
        return True;
    }

    #! maps argument names to their value AST nodes
    private hash<auto> argumentMap(list<hash<GraphQLNode>> arguments) {
        hash<auto> m();
        foreach hash<GraphQLNode> an in (arguments) {
            hash<GraphQLArgument> a = cast<hash<GraphQLArgument>>(an);
            m{a.name} = a.value;
        }
        return m;
    }

    #! structurally compares two value AST nodes for equality (variables by name, literals by value,
    #! lists element-wise, input objects by field set)
    private bool valuesEqual(hash<GraphQLNode> a, hash<GraphQLNode> b) {
        if (a.kind != b.kind) {
            return False;
        }
        switch (a.kind) {
            case NK_VARIABLE:
                return cast<hash<GraphQLVariable>>(a).name == cast<hash<GraphQLVariable>>(b).name;
            case NK_INT:
                return cast<hash<GraphQLIntValue>>(a).value == cast<hash<GraphQLIntValue>>(b).value;
            case NK_FLOAT:
                return cast<hash<GraphQLFloatValue>>(a).value == cast<hash<GraphQLFloatValue>>(b).value;
            case NK_STRING:
                return cast<hash<GraphQLStringValue>>(a).value == cast<hash<GraphQLStringValue>>(b).value;
            case NK_BOOLEAN:
                return cast<hash<GraphQLBooleanValue>>(a).value == cast<hash<GraphQLBooleanValue>>(b).value;
            case NK_ENUM:
                return cast<hash<GraphQLEnumValue>>(a).value == cast<hash<GraphQLEnumValue>>(b).value;
            case NK_NULL:
                return True;
            case NK_LIST: {
                list<hash<GraphQLNode>> la = cast<hash<GraphQLListValue>>(a).values;
                list<hash<GraphQLNode>> lb = cast<hash<GraphQLListValue>>(b).values;
                if (la.size() != lb.size()) {
                    return False;
                }
                for (int i = 0; i < la.size(); ++i) {
                    if (!valuesEqual(la[i], lb[i])) {
                        return False;
                    }
                }
                return True;
            }
            case NK_OBJECT: {
                hash<auto> ma = objectFieldMap(cast<hash<GraphQLObjectValue>>(a));
                hash<auto> mb = objectFieldMap(cast<hash<GraphQLObjectValue>>(b));
                if (ma.size() != mb.size()) {
                    return False;
                }
                foreach string name in (keys ma) {
                    if (!exists mb{name} || !valuesEqual(ma{name}, mb{name})) {
                        return False;
                    }
                }
                return True;
            }
        }
        return False;
    }

    #! maps an object value's field names to their value AST nodes
    private hash<auto> objectFieldMap(hash<GraphQLObjectValue> obj) {
        hash<auto> m();
        foreach hash<GraphQLNode> fn in (obj.fields) {
            hash<GraphQLObjectField> f = cast<hash<GraphQLObjectField>>(fn);
            m{f.name} = f.value;
        }
        return m;
    }

    #! records a validation error
    private addError(string message) {
        push errors, {"message": message};
    }
}
}
