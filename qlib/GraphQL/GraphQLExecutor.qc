# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLExecutor.qc GraphQL document validation and execution engine

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
#! internal exception code used to propagate a non-null violation up to the nearest nullable position
const GRAPHQL_NULL_BUBBLE = "GRAPHQL-NULL-BUBBLE";

#! Validates and executes GraphQL documents against a schema and a resolver map
/** A resolver map is a hash keyed by type name, then by field name, where each value is a
    closure or call reference with the signature:
    @code{.py}
auto sub (auto parent, hash<auto> args, auto ctx)
    @endcode

    If no resolver is registered for a field, a default resolver is used that reads the field
    (by name) from the parent value when the parent is a hash or object.

    Execution returns the standard GraphQL response envelope as a hash with \c data and/or
    \c errors keys. Field-level errors are collected with a \c path and do not abort the whole
    operation unless they propagate through a non-null type.

    @par Example:
    @code{.py}
GraphQLSchema schema("type Query { hello: String }");
GraphQLExecutor executor(schema, {
    "Query": {"hello": string sub (auto p, hash<auto> a, auto c) { return "hi"; }},
});
hash<auto> result = executor.execute("{ hello }");
# result == {"data": {"hello": "hi"}}
    @endcode
*/
public class GraphQLExecutor {
    private {
        #! the schema to execute against
        GraphQLSchema schema;
        #! the effective resolver map (user resolvers merged with introspection resolvers)
        hash<auto> effective_resolvers;
        #! the working type map (schema types plus introspection types)
        hash<string, hash<GraphQLTypeDef>> all_types;
        #! custom-scalar coercers, keyed by scalar type name
        hash<auto> scalar_coercers;
        #! whether to validate documents before execution
        bool validate_documents = True;
        #! the maximum query complexity (total field count); @ref NOTHING uses the validator default
        *int max_complexity;
    }

    #! creates the executor for the given schema and resolver map
    /** @param n_schema the schema to execute against
        @param n_resolvers the resolver map (type name -> field name -> resolver code)
        @param n_scalar_coercers optional custom-scalar coercers, keyed by scalar type name; each
        value is a hash that may contain:
        - \c input: <tt>auto sub (auto value)</tt> — validates/parses a client input value for the
          scalar (should throw to reject an invalid value)
        - \c output: <tt>auto sub (auto value)</tt> — serializes a resolved value for the scalar

        Built-in scalars (\c Int, \c Float, \c String, \c Boolean, \c ID) are always validated by
        the engine. A custom scalar with no registered coercer passes its value through unchanged.
    */
    constructor(GraphQLSchema n_schema, hash<auto> n_resolvers, *hash<auto> n_scalar_coercers) {
        schema = n_schema;
        scalar_coercers = n_scalar_coercers ?? {};
        # merge introspection types into the working type map
        all_types = schema.getTypes() + GraphQLIntrospection::getIntrospectionTypes();

        # build the schema-bound introspection system and its resolvers
        GraphQLIntrospection intro(n_schema, all_types);
        hash<auto> introRes = intro.getResolvers();
        # register the root meta-field resolvers (__schema, __type) on the query type
        *hash<GraphQLTypeDef> qt = n_schema.getQueryType();
        if (qt) {
            introRes{qt.name} = (introRes{qt.name} ?? {}) + {
                "__schema": auto sub (auto p, hash<auto> a, auto c) { return intro.getSchemaValue(); },
                "__type": auto sub (auto p, hash<auto> a, auto c) {
                    return intro.getTypeValueByName(a.name);
                },
            };
        }

        # merge user resolvers with introspection resolvers, per type (field maps merged so that
        # user field resolvers on the query type are preserved alongside the meta-field resolvers)
        effective_resolvers = n_resolvers;
        foreach string tname in (keys introRes) {
            effective_resolvers{tname} = (effective_resolvers{tname} ?? {}) + introRes{tname};
        }
    }

    #! returns the schema this executor runs against
    GraphQLSchema getSchema() {
        return schema;
    }

    #! enables or disables pre-execution document validation and sets the complexity budget
    /** @param enable if @ref True (the default), documents are validated before execution
        @param n_max_complexity the maximum query complexity (total field count); @ref NOTHING uses
        the validator default
    */
    setValidation(bool enable = True, *int n_max_complexity) {
        validate_documents = enable;
        max_complexity = n_max_complexity;
    }

    #! parses, validates, and executes a GraphQL request, returning the response envelope
    /** @param query the GraphQL document source
        @param variable_values the input variable values (as received from the client), if any
        @param operation_name the name of the operation to execute (required if the document
        contains more than one operation)
        @param ctx an arbitrary ctx value passed to every resolver

        @return the GraphQL response envelope: a hash with \c data and/or \c errors keys

        @note request-level failures (syntax errors, validation errors, unknown operation) are
        returned as an \c errors array with no \c data key, per the GraphQL-over-HTTP convention
    */
    hash<auto> execute(string query, *hash<auto> variable_values, *string operation_name, auto ctx) {
        hash<GraphQLDocument> doc;
        try {
            doc = GraphQLParser::parse(query);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_SYNTAX_ERROR) {
                return {"errors": (makeError(ex.desc, ex.arg),)};
            }
            rethrow;
        }
        return executeDocument(doc, variable_values, operation_name, ctx);
    }

    #! validates and executes an already-parsed document
    /** @param doc the parsed document
        @param variable_values the input variable values, if any
        @param operation_name the operation to execute, if the document has more than one
        @param ctx an arbitrary ctx value passed to every resolver
        @param require_operation_type if set (e.g. \c "query"), the selected operation must be of
        this type, otherwise an error envelope is returned; used to enforce query-only semantics
        for HTTP GET requests

        @return the GraphQL response envelope
    */
    hash<auto> executeDocument(hash<GraphQLDocument> doc, *hash<auto> variable_values,
            *string operation_name, auto ctx, *string require_operation_type) {
        # validate the document before execution; on any validation error, return all errors
        # together without executing (a fresh validator per call -- it holds per-call state)
        if (validate_documents) {
            list<auto> verrs = (new GraphQLValidator(schema, all_types, max_complexity)).validate(doc);
            if (verrs) {
                return {"errors": verrs};
            }
        }
        # collect fragment definitions and operations
        hash<string, hash<GraphQLFragmentDefinition>> fragments();
        list<hash<GraphQLOperationDefinition>> operations();
        foreach hash<GraphQLNode> def in (doc.definitions) {
            switch (def.kind) {
                case NK_OPERATION:
                    push operations, cast<hash<GraphQLOperationDefinition>>(def);
                    break;
                case NK_FRAGMENT_DEFINITION: {
                    hash<GraphQLFragmentDefinition> fd = cast<hash<GraphQLFragmentDefinition>>(def);
                    if (fragments{fd.name}) {
                        return {"errors": (makeError(sprintf("fragment %y is defined more than once",
                            fd.name)),)};
                    }
                    fragments{fd.name} = fd;
                    break;
                }
                default:
                    return {"errors": (makeError("a document for execution may only contain "
                        "operations and fragments"),)};
            }
        }

        # select the operation to execute
        hash<GraphQLOperationDefinition> op;
        try {
            op = selectOperation(operations, operation_name);
        } catch (hash<ExceptionInfo> ex) {
            return {"errors": (makeError(ex.desc),)};
        }

        # optionally restrict the allowed operation type (e.g. queries only over HTTP GET)
        if (exists require_operation_type && op.operation != require_operation_type) {
            return {"errors": (makeError(sprintf("%s operations are not permitted here; only %s "
                "operations are allowed", op.operation, require_operation_type)),)};
        }

        # establish the execution state
        ExecutionState state(self, schema, all_types, effective_resolvers, fragments, ctx,
            scalar_coercers);

        # coerce variables
        try {
            state.coerceVariables(op.variableDefinitions, variable_values);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_VALIDATION_ERROR) {
                return {"errors": (makeError(ex.desc, ex.arg),)};
            }
            rethrow;
        }

        # resolve the root type
        *hash<GraphQLTypeDef> rootType = schema.getRootType(op.operation);
        if (!rootType) {
            return {"errors": (makeError(sprintf("the schema does not support %s operations",
                op.operation)),)};
        }

        # execute the root selection set
        auto data;
        try {
            data = state.executeSelectionSet(rootType, cast<hash<GraphQLSelectionSet>>(op.selectionSet),
                NOTHING, ());
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_NULL_BUBBLE) {
                # a non-null violation propagated to the root: data is null
                data = NOTHING;
            } else {
                rethrow;
            }
        }

        # build the full literal at once with mixed value types to avoid hash<auto> narrowing
        list<auto> errs = state.getErrors();
        return errs
            ? {"data": data, "errors": errs}
            : {"data": data};
    }

    #! sets up a subscription: validates the document, calls the subscription field's resolver to
    #! obtain an event source, and returns the per-event execution state
    /** @param query the GraphQL subscription document source
        @param variable_values the input variable values, if any
        @param operation_name the operation to execute, if the document has more than one
        @param ctx an arbitrary context value passed to resolvers

        @return on success, a hash with a \c subscription key (an opaque value to pass to
        @ref getSubscriptionStream() to obtain the event source, and to
        @ref resolveSubscriptionEvent() per event). On failure (not a subscription or a validation
        error) a hash with an \c errors key is returned instead.

        @note the subscription field's resolver is NOT called here; call
        @ref getSubscriptionStream() from the thread that will consume the events, because Qore
        iterators are thread-bound (an iterator must be created and used in the same thread).
    */
    hash<auto> executeSubscription(string query, *hash<auto> variable_values, *string operation_name,
            auto ctx) {
        hash<GraphQLDocument> doc;
        try {
            doc = GraphQLParser::parse(query);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_SYNTAX_ERROR) {
                return {"errors": (makeError(ex.desc, ex.arg),)};
            }
            rethrow;
        }
        if (validate_documents) {
            list<auto> verrs = (new GraphQLValidator(schema, all_types, max_complexity)).validate(doc);
            if (verrs) {
                return {"errors": verrs};
            }
        }
        hash<string, hash<GraphQLFragmentDefinition>> fragments();
        list<hash<GraphQLOperationDefinition>> operations();
        foreach hash<GraphQLNode> def in (doc.definitions) {
            if (def.kind == NK_OPERATION) {
                push operations, cast<hash<GraphQLOperationDefinition>>(def);
            } else if (def.kind == NK_FRAGMENT_DEFINITION) {
                hash<GraphQLFragmentDefinition> fd = cast<hash<GraphQLFragmentDefinition>>(def);
                fragments{fd.name} = fd;
            }
        }
        hash<GraphQLOperationDefinition> op;
        try {
            op = selectOperation(operations, operation_name);
        } catch (hash<ExceptionInfo> ex) {
            return {"errors": (makeError(ex.desc),)};
        }
        if (op.operation != "subscription") {
            return {"errors": (makeError("executeSubscription() requires a subscription operation"),)};
        }
        *hash<GraphQLTypeDef> rootType = schema.getSubscriptionType();
        if (!rootType) {
            return {"errors": (makeError("the schema does not support subscription operations"),)};
        }

        ExecutionState state(self, schema, all_types, effective_resolvers, fragments, ctx,
            scalar_coercers);
        try {
            state.coerceVariables(op.variableDefinitions, variable_values);
            # validate that the subscription selects exactly one root field
            hash<GraphQLField> field = state.getSingleRootField(rootType,
                cast<hash<GraphQLSelectionSet>>(op.selectionSet));
            return {
                "subscription": {
                    "rootType": rootType,
                    "field": field,
                    "fragments": fragments,
                    "variable_values": variable_values,
                    "op": op,
                    "ctx": ctx,
                },
            };
        } catch (hash<ExceptionInfo> ex) {
            return {"errors": (makeError(ex.desc, ex.arg),)};
        }
    }

    #! calls the subscription field's resolver to obtain the event source (an AbstractIterator)
    /** Call this from the thread that will consume the events: Qore iterators are thread-bound, so
        the event-source iterator must be created in (and only used by) the consuming thread.

        @param subscription the opaque \c subscription value from @ref executeSubscription()

        @return the event source @ref Qore::AbstractIterator

        @throw GRAPHQL-EXECUTION-ERROR the resolver did not return an AbstractIterator
    */
    Qore::AbstractIterator getSubscriptionStream(hash<auto> subscription) {
        ExecutionState state(self, schema, all_types, effective_resolvers, subscription.fragments,
            subscription.ctx, scalar_coercers);
        state.coerceVariables(subscription.op.variableDefinitions, subscription.variable_values);
        auto src = state.resolveSubscriptionSource(subscription.rootType, subscription.field);
        if (!(src instanceof Qore::AbstractIterator)) {
            throw GRAPHQL_EXECUTION_ERROR, sprintf("the resolver for subscription field %y must "
                "return an AbstractIterator (the event source)", subscription.field.name);
        }
        return src;
    }

    #! executes one subscription event, returning the response envelope for that event
    /** @param subscription the opaque \c subscription value from @ref executeSubscription()
        @param event the event value yielded by the subscription's event source

        @return the GraphQL response envelope (\c data / \c errors) for this event
    */
    hash<auto> resolveSubscriptionEvent(hash<auto> subscription, auto event) {
        ExecutionState state(self, schema, all_types, effective_resolvers, subscription.fragments,
            subscription.ctx, scalar_coercers);
        auto data;
        try {
            state.coerceVariables(subscription.op.variableDefinitions, subscription.variable_values);
            data = state.completeRootFieldWithValue(subscription.rootType, subscription.field, event);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_NULL_BUBBLE) {
                data = NOTHING;
            } else {
                return {"errors": (makeError(ex.desc, ex.arg),)};
            }
        }
        list<auto> errs = state.getErrors();
        return errs ? {"data": data, "errors": errs} : {"data": data};
    }

    #! returns the operation type (\c query / \c mutation / \c subscription) for a request, or
    #! @ref NOTHING if the document cannot be parsed or the operation cannot be selected
    /** @param query the GraphQL document source
        @param operation_name the operation name, if the document has more than one operation
    */
    *string getOperationType(string query, *string operation_name) {
        hash<GraphQLDocument> doc;
        try {
            doc = GraphQLParser::parse(query);
        } catch (hash<ExceptionInfo> ex) {
            return NOTHING;
        }
        list<hash<GraphQLOperationDefinition>> ops();
        foreach hash<GraphQLNode> def in (doc.definitions) {
            if (def.kind == NK_OPERATION) {
                push ops, cast<hash<GraphQLOperationDefinition>>(def);
            }
        }
        try {
            return selectOperation(ops, operation_name).operation;
        } catch (hash<ExceptionInfo> ex) {
            return NOTHING;
        }
    }

    #! selects the operation to execute from the document's operations
    private hash<GraphQLOperationDefinition> selectOperation(
            list<hash<GraphQLOperationDefinition>> operations, *string operation_name) {
        if (!operations) {
            throw GRAPHQL_VALIDATION_ERROR, "the document contains no operations to execute";
        }
        if (exists operation_name) {
            foreach hash<GraphQLOperationDefinition> o in (operations) {
                if (o.name == operation_name) {
                    return o;
                }
            }
            throw GRAPHQL_VALIDATION_ERROR, sprintf("no operation named %y in the document",
                operation_name);
        }
        if (operations.size() > 1) {
            throw GRAPHQL_VALIDATION_ERROR, "the document contains multiple operations; an "
                "operation name must be provided";
        }
        return operations[0];
    }

    #! builds a GraphQL error hash with the given message and optional path/extra information
    static hash<auto> makeError(string message, *hash<auto> extra) {
        # build the full literal at once with mixed value types to avoid hash<auto> narrowing
        return extra.line
            ? {"message": message, "locations": ({"line": extra.line, "column": extra.column},)}
            : {"message": message};
    }
}

#! Holds the mutable per-execution state and implements the core execution algorithm
/** This class is created once per operation execution. It accumulates errors and resolved
    variables, and implements selection-set execution and value completion with GraphQL's
    non-null error-propagation semantics.
*/
class ExecutionState {
    public {
        #! the maximum selection-set / input-value nesting depth (DoS guard against deeply nested queries)
        const MaxDepth = 64;
        #! the minimum value representable by the GraphQL \c Int scalar (signed 32-bit)
        const Int32Min = -2147483648;
        #! the maximum value representable by the GraphQL \c Int scalar (signed 32-bit)
        const Int32Max = 2147483647;
    }

    private {
        #! the current selection-set nesting depth (bounded by @ref MaxDepth)
        int selection_depth = 0;
        #! the current input-value nesting depth (bounded by @ref MaxDepth)
        int input_depth = 0;
        #! custom-scalar coercers, keyed by scalar type name
        hash<auto> scalar_coercers;
        #! the owning executor
        GraphQLExecutor executor;
        #! the schema
        GraphQLSchema schema;
        #! the working type map (schema + introspection types)
        hash<string, hash<GraphQLTypeDef>> all_types;
        #! the effective resolver map
        hash<auto> resolvers;
        #! the fragment definitions in the document, keyed by name
        hash<string, hash<GraphQLFragmentDefinition>> fragments;
        #! the resolver ctx value
        auto ctx;
        #! the coerced variable values
        hash<auto> variables();
        #! the accumulated execution errors
        list<auto> errors();
    }

    #! creates the execution state
    constructor(GraphQLExecutor n_executor, GraphQLSchema n_schema,
            hash<string, hash<GraphQLTypeDef>> n_all_types, hash<auto> n_resolvers,
            hash<string, hash<GraphQLFragmentDefinition>> n_fragments, auto n_ctx,
            hash<auto> n_scalar_coercers) {
        executor = n_executor;
        schema = n_schema;
        all_types = n_all_types;
        resolvers = n_resolvers;
        fragments = n_fragments;
        ctx = n_ctx;
        scalar_coercers = n_scalar_coercers;
    }

    #! returns the accumulated execution errors
    list<auto> getErrors() {
        return errors;
    }

    #! returns the coerced variable values (used by the introspection system and resolvers)
    hash<auto> getVariables() {
        return variables;
    }

    #! coerces the operation's variable values against their declared types
    /** @param varDefs the variable definitions
        @param values the raw input values

        @throw GRAPHQL-VALIDATION-ERROR a required variable is missing or a value is invalid
    */
    coerceVariables(list<hash<GraphQLNode>> varDefs, *hash<auto> values) {
        foreach hash<GraphQLNode> vdn in (varDefs) {
            hash<GraphQLVariableDefinition> vd = cast<hash<GraphQLVariableDefinition>>(vdn);
            bool has_value = exists values{vd.name};
            if (!has_value) {
                if (exists vd.defaultValue) {
                    variables{vd.name} = GraphQLValue::valueFromAst(vd.defaultValue, {});
                    continue;
                }
                if (vd.type.kind == NK_NON_NULL_TYPE) {
                    throw GRAPHQL_VALIDATION_ERROR, sprintf("required variable %y of type %s was "
                        "not provided", vd.name, GraphQLSchema::typeRefToString(vd.type));
                }
                continue;
            }
            variables{vd.name} = coerceInputValue(values{vd.name}, vd.type,
                sprintf("variable %y", vd.name));
        }
    }

    #! coerces a runtime input value against a type reference
    /** @param value the runtime value
        @param typeref the target type reference
        @param ctx a description of the value being coerced (for error messages)

        @return the coerced value

        @throw GRAPHQL-VALIDATION-ERROR the value is not valid for the type
    */
    auto coerceInputValue(auto value, hash<GraphQLNode> typeref, string ctx) {
        # bound input nesting depth to guard against deeply nested input objects/lists
        if (++input_depth > MaxDepth) {
            --input_depth;
            throw GRAPHQL_VALIDATION_ERROR, sprintf("%s exceeds the maximum input nesting depth of %d",
                ctx, MaxDepth);
        }
        on_exit --input_depth;
        if (typeref.kind == NK_NON_NULL_TYPE) {
            if (!exists value) {
                throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must not be null", ctx);
            }
            return coerceInputValue(value, cast<hash<GraphQLNonNullType>>(typeref).type, ctx);
        }
        if (!exists value) {
            return NOTHING;
        }
        if (typeref.kind == NK_LIST_TYPE) {
            hash<GraphQLNode> inner = cast<hash<GraphQLListType>>(typeref).type;
            if (value.typeCode() != NT_LIST) {
                # coerce a single value into a one-element list, per spec
                return (coerceInputValue(value, inner, ctx),);
            }
            list<auto> out();
            int i = 0;
            foreach auto item in (value) {
                push out, coerceInputValue(item, inner, sprintf("%s[%d]", ctx, i++));
            }
            return out;
        }
        # named type: scalar, enum, or input object
        string tname = cast<hash<GraphQLNamedType>>(typeref).name;
        *hash<GraphQLTypeDef> td = all_types{tname};
        if (!td) {
            throw GRAPHQL_VALIDATION_ERROR, sprintf("%s references unknown type %y", ctx, tname);
        }
        switch (td.kind) {
            case TYPE_SCALAR:
                return coerceScalar(value, tname, ctx);
            case TYPE_ENUM:
                if (value.typeCode() != NT_STRING || !inlist(value, td.enumValues)) {
                    throw GRAPHQL_VALIDATION_ERROR, sprintf("%s has invalid value %y for enum %y",
                        ctx, value, tname);
                }
                return value;
            case TYPE_INPUT_OBJECT: {
                if (value.typeCode() != NT_HASH) {
                    throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must be an input object for type %y",
                        ctx, tname);
                }
                # reject input-object fields not defined on the type (per the GraphQL spec)
                foreach string k in (keys value) {
                    if (!td.inputFields{k}) {
                        throw GRAPHQL_VALIDATION_ERROR, sprintf("%s contains unknown input field %y "
                            "for type %y", ctx, k, tname);
                    }
                }
                hash<auto> out();
                foreach hash<GraphQLInputValueDef> ivd in (td.inputFields.iterator()) {
                    if (exists value{ivd.name}) {
                        out{ivd.name} = coerceInputValue(value{ivd.name}, ivd.type,
                            sprintf("%s.%s", ctx, ivd.name));
                    } else if (exists ivd.defaultValue) {
                        out{ivd.name} = GraphQLValue::valueFromAst(ivd.defaultValue, {});
                    } else if (ivd.type.kind == NK_NON_NULL_TYPE) {
                        throw GRAPHQL_VALIDATION_ERROR, sprintf("%s is missing required input "
                            "field %y", ctx, ivd.name);
                    }
                }
                return out;
            }
            default:
                throw GRAPHQL_VALIDATION_ERROR, sprintf("%s cannot use %s type %y as an input",
                    ctx, td.kind, tname);
        }
    }

    #! coerces a value to a built-in or custom scalar
    private auto coerceScalar(auto value, string tname, string ctx) {
        switch (tname) {
            case "Int": {
                int iv;
                if (value.typeCode() == NT_INT) {
                    iv = value;
                } else if (value.typeCode() == NT_STRING && value =~ /^-?[0-9]+$/) {
                    iv = value.toInt();
                } else {
                    throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must be an integer", ctx);
                }
                # the GraphQL Int scalar is a signed 32-bit value
                if (iv < Int32Min || iv > Int32Max) {
                    throw GRAPHQL_VALIDATION_ERROR, sprintf("%s value %d is outside the 32-bit "
                        "range of the Int scalar", ctx, iv);
                }
                return iv;
            }
            case "Float":
                if (value.typeCode() == NT_FLOAT || value.typeCode() == NT_INT) {
                    return value.toFloat();
                }
                throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must be a number", ctx);
            case "Boolean":
                if (value.typeCode() == NT_BOOLEAN) {
                    return value;
                }
                throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must be a boolean", ctx);
            case "String":
            case "ID":
                if (value.typeCode() == NT_STRING) {
                    return value;
                }
                if (tname == "ID" && value.typeCode() == NT_INT) {
                    return value.toString();
                }
                throw GRAPHQL_VALIDATION_ERROR, sprintf("%s must be a string", ctx);
            default: {
                # custom scalar: use a registered input coercer if present, else pass through
                *code coercer = scalar_coercers{tname}.input;
                return exists coercer ? coercer(value) : value;
            }
        }
    }

    #! executes a selection set against an object value, returning the ordered result map
    /** @param objectType the concrete object/interface type being resolved
        @param selectionSet the selection set
        @param objectValue the parent value
        @param path the response path to this point

        @return the result map (response key -> completed value)

        @throw GRAPHQL-NULL-BUBBLE a non-null field error propagated out of this selection set
    */
    hash<auto> executeSelectionSet(hash<GraphQLTypeDef> objectType,
            hash<GraphQLSelectionSet> selectionSet, auto objectValue, list<auto> path) {
        # bound nesting depth to guard against stack exhaustion from deeply nested queries
        if (++selection_depth > MaxDepth) {
            --selection_depth;
            throw GRAPHQL_VALIDATION_ERROR, sprintf("query exceeds the maximum nesting depth of %d",
                MaxDepth);
        }
        on_exit --selection_depth;
        hash<auto> grouped = collectFields(objectType, selectionSet, {});
        hash<auto> resultMap();
        foreach string responseKey in (keys grouped) {
            list<hash<GraphQLField>> fieldGroup = grouped{responseKey};
            hash<GraphQLField> field = fieldGroup[0];
            list<auto> fieldPath = path + responseKey;

            # meta-field __typename
            if (field.name == "__typename") {
                resultMap{responseKey} = objectType.name;
                continue;
            }

            *hash<GraphQLFieldDef> fieldDef = getFieldDef(objectType, field.name);
            if (!fieldDef) {
                recordError(sprintf("cannot query field %y on type %y", field.name, objectType.name),
                    fieldPath);
                # treat as a validation-style error; field omitted/null
                resultMap{responseKey} = NOTHING;
                continue;
            }

            try {
                resultMap{responseKey} = executeField(objectType, fieldDef, fieldGroup,
                    objectValue, fieldPath);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == GRAPHQL_NULL_BUBBLE) {
                    # a non-null violation occurred below this field
                    if (fieldDef.type.kind == NK_NON_NULL_TYPE) {
                        # propagate further up
                        rethrow;
                    }
                    resultMap{responseKey} = NOTHING;
                } else {
                    rethrow;
                }
            }
        }
        return resultMap;
    }

    #! returns the single root field of a subscription selection set (errors if not exactly one)
    /** @throw GRAPHQL-VALIDATION-ERROR the selection set does not contain exactly one field
    */
    hash<GraphQLField> getSingleRootField(hash<GraphQLTypeDef> rootType, hash<GraphQLSelectionSet> ss) {
        hash<auto> grouped = collectFields(rootType, ss, {});
        if (grouped.size() != 1) {
            throw GRAPHQL_VALIDATION_ERROR, "a subscription operation must select exactly one root field";
        }
        return cast<hash<GraphQLField>>(grouped.firstValue()[0]);
    }

    #! calls a subscription root field's resolver (with coerced arguments) to obtain the event source
    auto resolveSubscriptionSource(hash<GraphQLTypeDef> rootType, hash<GraphQLField> field) {
        *hash<GraphQLFieldDef> fieldDef = getFieldDef(rootType, field.name);
        if (!fieldDef) {
            throw GRAPHQL_VALIDATION_ERROR, sprintf("cannot subscribe to field %y on type %y",
                field.name, rootType.name);
        }
        hash<auto> args = coerceArgumentValues(fieldDef, field, (field.alias ?? field.name,));
        return resolveFieldValue(rootType, fieldDef, field.name, NOTHING, args);
    }

    #! completes a subscription's root field against a provided event value, returning the data map
    /** @throw GRAPHQL-NULL-BUBBLE the event value is null in a non-null root field
    */
    hash<auto> completeRootFieldWithValue(hash<GraphQLTypeDef> rootType, hash<GraphQLField> field,
            auto value) {
        string responseKey = field.alias ?? field.name;
        *hash<GraphQLFieldDef> fieldDef = getFieldDef(rootType, field.name);
        if (!fieldDef) {
            recordError(sprintf("cannot query field %y on type %y", field.name, rootType.name),
                (responseKey,));
            return {};
        }
        return {responseKey: completeValue(fieldDef.type, (field,), value, (responseKey,))};
    }

    #! executes a single field, returning its completed value
    private auto executeField(hash<GraphQLTypeDef> objectType, hash<GraphQLFieldDef> fieldDef,
            list<hash<GraphQLField>> fieldGroup, auto objectValue, list<auto> path) {
        hash<GraphQLField> field = fieldGroup[0];

        try {
            # argument coercion, the resolver, and value completion can all raise; any such error
            # becomes a field error rather than aborting the whole operation
            hash<auto> args = coerceArgumentValues(fieldDef, field, path);
            auto resolved = resolveFieldValue(objectType, fieldDef, field.name, objectValue, args);
            return completeValue(fieldDef.type, fieldGroup, resolved, path);
        } catch (hash<ExceptionInfo> ex) {
            # a non-null violation below this field must keep propagating untouched (the error was
            # already recorded at its origin)
            if (ex.err == GRAPHQL_NULL_BUBBLE) {
                rethrow;
            }
            recordError(ex.desc, path);
            # for a non-null field the null must propagate, but without adding a redundant
            # "cannot return null" message, so bubble directly
            if (fieldDef.type.kind == NK_NON_NULL_TYPE) {
                throw GRAPHQL_NULL_BUBBLE, "field error";
            }
            return NOTHING;
        }
    }

    #! resolves a field value by invoking the registered resolver or the default resolver
    private auto resolveFieldValue(hash<GraphQLTypeDef> objectType, hash<GraphQLFieldDef> fieldDef,
            string fieldName, auto objectValue, hash<auto> args) {
        *code resolver = resolvers{objectType.name}{fieldName};
        if (exists resolver) {
            return resolver(objectValue, args, ctx);
        }
        # default resolver: read the field from the parent hash/object
        switch (objectValue.typeCode()) {
            case NT_HASH:
                return objectValue{fieldName};
            case NT_OBJECT:
                return objectValue.getMemberOrNothing(fieldName);
            default:
                return NOTHING;
        }
    }

    #! completes a resolved value against its declared type, applying non-null/list/leaf rules
    /** @throw GRAPHQL-NULL-BUBBLE the value is null in a non-null position
    */
    private auto completeValue(hash<GraphQLNode> typeref, list<hash<GraphQLField>> fieldGroup,
            auto result, list<auto> path) {
        if (typeref.kind == NK_NON_NULL_TYPE) {
            auto completed = completeValue(cast<hash<GraphQLNonNullType>>(typeref).type, fieldGroup,
                result, path);
            if (!exists completed) {
                # if no error was already recorded for this position, record one now
                recordError(sprintf("cannot return null for non-nullable field at path %s",
                    pathToString(path)), path);
                throw GRAPHQL_NULL_BUBBLE, "non-null violation";
            }
            return completed;
        }
        if (!exists result) {
            return NOTHING;
        }
        if (typeref.kind == NK_LIST_TYPE) {
            if (result.typeCode() != NT_LIST) {
                recordError("expected a list value", path);
                return NOTHING;
            }
            hash<GraphQLNode> inner = cast<hash<GraphQLListType>>(typeref).type;
            bool inner_non_null = inner.kind == NK_NON_NULL_TYPE;
            list<auto> out();
            int i = 0;
            foreach auto item in (result) {
                list<auto> itemPath = path + i++;
                try {
                    push out, completeValue(inner, fieldGroup, item, itemPath);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != GRAPHQL_NULL_BUBBLE) {
                        # an unexpected error completing the item becomes an error at the item path
                        recordError(ex.desc, itemPath);
                    }
                    # if the element type is non-null, the error propagates and the whole list
                    # becomes null; otherwise the offending element is nulled and the list continues
                    if (inner_non_null) {
                        throw GRAPHQL_NULL_BUBBLE, "list item error";
                    }
                    push out, NOTHING;
                }
            }
            return out;
        }
        # named type
        string tname = cast<hash<GraphQLNamedType>>(typeref).name;
        *hash<GraphQLTypeDef> td = all_types{tname};
        if (!td) {
            recordError(sprintf("no type named %y in the schema", tname), path);
            return NOTHING;
        }
        switch (td.kind) {
            case TYPE_SCALAR:
                return completeLeafScalar(result, tname, path);
            case TYPE_ENUM:
                return result.toString();
            case TYPE_OBJECT:
                return executeSelectionSet(td, mergeSelectionSets(fieldGroup), result, path);
            case TYPE_INTERFACE:
            case TYPE_UNION: {
                hash<GraphQLTypeDef> concrete = resolveConcreteType(td, result, path);
                return executeSelectionSet(concrete, mergeSelectionSets(fieldGroup), result, path);
            }
            default:
                recordError(sprintf("cannot complete value of type %y", tname), path);
                return NOTHING;
        }
    }

    #! coerces a leaf scalar result value to its serialized form
    private auto completeLeafScalar(auto result, string tname, list<auto> path) {
        # a structured (hash/list/object) value can never represent a built-in leaf scalar;
        # reject it as a field error rather than silently coercing it (for example int(hash)
        # yields 0 and boolean(hash) yields True, masking a resolver bug as a valid result)
        switch (result.typeCode()) {
            case NT_HASH:
            case NT_LIST:
            case NT_OBJECT: {
                if (tname == "Int" || tname == "Float" || tname == "Boolean"
                        || tname == "String" || tname == "ID") {
                    throw GRAPHQL_EXECUTION_ERROR, sprintf("%s field at path %s cannot represent "
                        "the value %y", tname, pathToString(path), result);
                }
            }
        }
        switch (tname) {
            case "Int": {
                int iv = int(result);
                # the GraphQL Int scalar is a signed 32-bit value; an out-of-range result is an error
                if (iv < Int32Min || iv > Int32Max) {
                    throw GRAPHQL_EXECUTION_ERROR, sprintf("Int field at path %s cannot represent "
                        "the value %d (outside the 32-bit range)", pathToString(path), iv);
                }
                return iv;
            }
            case "Float":
                return float(result);
            case "Boolean":
                return boolean(result);
            case "String":
            case "ID":
                return result.toString();
            default: {
                # custom scalar: use a registered output coercer if present, else serialize as-is
                *code coercer = scalar_coercers{tname}.output;
                return exists coercer ? coercer(result) : result;
            }
        }
    }

    #! resolves the concrete object type for an interface or union result
    private hash<GraphQLTypeDef> resolveConcreteType(hash<GraphQLTypeDef> abstractType,
            auto result, list<auto> path) {
        # the resolver may indicate the concrete type via a "__typename" key in the result
        *string tn;
        if (result.typeCode() == NT_HASH) {
            tn = result.__typename;
        }
        if (exists tn && all_types{tn} && all_types{tn}.kind == TYPE_OBJECT) {
            return all_types{tn};
        }
        throw GRAPHQL_EXECUTION_ERROR, sprintf("unable to resolve concrete type for abstract type "
            "%y at path %s; resolvers for abstract types must return a \"__typename\" key",
            abstractType.name, pathToString(path));
    }

    #! merges the sub-selection sets of a group of fields with the same response key
    private hash<GraphQLSelectionSet> mergeSelectionSets(list<hash<GraphQLField>> fieldGroup) {
        list<hash<GraphQLNode>> selections();
        foreach hash<GraphQLField> f in (fieldGroup) {
            if (exists f.selectionSet) {
                selections += cast<hash<GraphQLSelectionSet>>(f.selectionSet).selections;
            }
        }
        return <GraphQLSelectionSet>{"kind": NK_SELECTION_SET, "selections": selections};
    }

    #! collects and groups the fields of a selection set by response key, expanding fragments
    /** @param objectType the concrete type
        @param selectionSet the selection set
        @param visited fragment names already visited (cycle guard)

        @return an (insertion-ordered) hash of response key -> list of @ref GraphQLField nodes
    */
    private hash<auto> collectFields(hash<GraphQLTypeDef> objectType,
            hash<GraphQLSelectionSet> selectionSet, hash<auto> visited) {
        hash<auto> grouped();
        foreach hash<GraphQLNode> sel in (selectionSet.selections) {
            if (!shouldInclude(sel)) {
                continue;
            }
            switch (sel.kind) {
                case NK_FIELD: {
                    hash<GraphQLField> f = cast<hash<GraphQLField>>(sel);
                    string responseKey = f.alias ?? f.name;
                    grouped{responseKey} = (grouped{responseKey} ?? ()) + f;
                    break;
                }
                case NK_INLINE_FRAGMENT: {
                    hash<GraphQLInlineFragment> inf = cast<hash<GraphQLInlineFragment>>(sel);
                    if (exists inf.typeCondition && !doesTypeApply(inf.typeCondition, objectType)) {
                        break;
                    }
                    hash<auto> subGrouped = collectFields(objectType,
                        cast<hash<GraphQLSelectionSet>>(inf.selectionSet), visited);
                    grouped = mergeGrouped(grouped, subGrouped);
                    break;
                }
                case NK_FRAGMENT_SPREAD: {
                    hash<GraphQLFragmentSpread> fs = cast<hash<GraphQLFragmentSpread>>(sel);
                    if (visited{fs.name}) {
                        break;
                    }
                    *hash<GraphQLFragmentDefinition> frag = fragments{fs.name};
                    if (!frag) {
                        recordError(sprintf("unknown fragment %y", fs.name), ());
                        break;
                    }
                    if (!doesTypeApply(frag.typeCondition, objectType)) {
                        break;
                    }
                    hash<auto> subGrouped = collectFields(objectType,
                        cast<hash<GraphQLSelectionSet>>(frag.selectionSet), visited + {fs.name: True});
                    grouped = mergeGrouped(grouped, subGrouped);
                    break;
                }
            }
        }
        return grouped;
    }

    #! merges two grouped-field maps, preserving order and concatenating field lists
    private hash<auto> mergeGrouped(hash<auto> a, hash<auto> b) {
        foreach string k in (keys b) {
            a{k} = (a{k} ?? ()) + b{k};
        }
        return a;
    }

    #! returns @ref True if the selection's \@skip/\@include directives allow it to be included
    private bool shouldInclude(hash<GraphQLNode> sel) {
        list<hash<GraphQLNode>> directives = getDirectives(sel);
        foreach hash<GraphQLNode> dn in (directives) {
            hash<GraphQLDirective> d = cast<hash<GraphQLDirective>>(dn);
            if (d.name == "skip" || d.name == "include") {
                auto ifVal = getDirectiveIf(d);
                if (d.name == "skip" && ifVal === True) {
                    return False;
                }
                if (d.name == "include" && ifVal === False) {
                    return False;
                }
            }
        }
        return True;
    }

    #! returns the value of a directive's \c "if" argument (resolving variables)
    private auto getDirectiveIf(hash<GraphQLDirective> d) {
        foreach hash<GraphQLNode> an in (d.arguments) {
            hash<GraphQLArgument> a = cast<hash<GraphQLArgument>>(an);
            if (a.name == "if") {
                return GraphQLValue::valueFromAst(a.value, variables);
            }
        }
        return NOTHING;
    }

    #! returns the directive list for any selection node
    private list<hash<GraphQLNode>> getDirectives(hash<GraphQLNode> sel) {
        switch (sel.kind) {
            case NK_FIELD:
                return cast<hash<GraphQLField>>(sel).directives;
            case NK_INLINE_FRAGMENT:
                return cast<hash<GraphQLInlineFragment>>(sel).directives;
            case NK_FRAGMENT_SPREAD:
                return cast<hash<GraphQLFragmentSpread>>(sel).directives;
            default:
                return ();
        }
    }

    #! returns @ref True if a fragment type condition applies to the given object type
    private bool doesTypeApply(string typeCondition, hash<GraphQLTypeDef> objectType) {
        if (typeCondition == objectType.name) {
            return True;
        }
        # object implements the interface named by the condition
        if (inlist(typeCondition, objectType.interfaces)) {
            return True;
        }
        # the condition is a union that includes the object type
        *hash<GraphQLTypeDef> condType = all_types{typeCondition};
        if (condType && condType.kind == TYPE_UNION) {
            return inlist(objectType.name, condType.unionTypes);
        }
        return False;
    }

    #! looks up a field definition on a type, including the introspection meta-fields
    private *hash<GraphQLFieldDef> getFieldDef(hash<GraphQLTypeDef> objectType, string fieldName) {
        # introspection meta-fields on the query root
        if (fieldName == "__schema" || fieldName == "__type") {
            *hash<GraphQLTypeDef> qt = schema.getQueryType();
            if (qt && objectType.name == qt.name) {
                return GraphQLIntrospection::getMetaField(fieldName);
            }
        }
        return objectType.fields{fieldName};
    }

    #! coerces the argument values for a field invocation
    private hash<auto> coerceArgumentValues(hash<GraphQLFieldDef> fieldDef, hash<GraphQLField> field,
            list<auto> path) {
        hash<auto> coerced();
        # index the provided arguments by name
        hash<auto> provided();
        foreach hash<GraphQLNode> an in (field.arguments) {
            hash<GraphQLArgument> a = cast<hash<GraphQLArgument>>(an);
            provided{a.name} = a.value;
        }
        foreach hash<GraphQLInputValueDef> argDef in (fieldDef.args.iterator()) {
            if (exists provided{argDef.name}) {
                hash<GraphQLNode> valueNode = provided{argDef.name};
                if (valueNode.kind == NK_VARIABLE) {
                    string vname = cast<hash<GraphQLVariable>>(valueNode).name;
                    if (exists variables{vname}) {
                        coerced{argDef.name} = variables{vname};
                    } else if (exists argDef.defaultValue) {
                        coerced{argDef.name} = GraphQLValue::valueFromAst(argDef.defaultValue, {});
                    } else if (argDef.type.kind == NK_NON_NULL_TYPE) {
                        # the argument is bound to a variable that was not provided (or was null)
                        # and the argument is non-null with no default
                        throw GRAPHQL_EXECUTION_ERROR, sprintf("required argument %y of field %y was "
                            "supplied the unset variable %y", argDef.name, fieldDef.name, vname);
                    }
                } else {
                    auto raw = GraphQLValue::valueFromAst(valueNode, variables);
                    coerced{argDef.name} = coerceInputValue(raw, argDef.type,
                        sprintf("argument %y", argDef.name));
                }
            } else if (exists argDef.defaultValue) {
                coerced{argDef.name} = GraphQLValue::valueFromAst(argDef.defaultValue, {});
            } else if (argDef.type.kind == NK_NON_NULL_TYPE) {
                throw GRAPHQL_EXECUTION_ERROR, sprintf("required argument %y of field %y is missing",
                    argDef.name, fieldDef.name);
            }
        }
        return coerced;
    }

    #! records an execution error with the given message and path
    recordError(string message, list<auto> path) {
        # build the full literal at once with mixed value types to avoid hash<auto> narrowing
        push errors, path
            ? {"message": message, "path": path}
            : {"message": message};
    }

    #! renders a response path as a slash-separated string for error messages
    static string pathToString(list<auto> path) {
        return path.join("/");
    }
}
}
