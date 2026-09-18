# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaActionOperation.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! One schema operation resolved and presented as an application action
/** This class owns everything about an action that does not depend on a REST client: it locates the
    operation in the schema, flattens the transport groupings into a single form, applies the UX overlay, and
    derives the request and response types.  A
    @ref RestSchemaActions::RestSchemaActionDataProvider "RestSchemaActionDataProvider" then adds a client
    and executes it.

    @section rsa_flattening Request Flattening

    A schema-derived REST request type groups parameters by where they travel: \c query and \c header become
    nested hashes, the request body becomes a \c body field, and path variables do not appear in the request
    type at all - they are supplied through the \c path_vars request option.  None of that is meaningful to
    somebody filling in an action form, so this class lifts every value to a single flat level and keeps a
    reverse map (@ref getFlatMap()) so that @ref unflatten() can rebuild the exact transport request.

    Fields are presented in transport order - path variables, then body properties, then query arguments,
    then headers - which puts the values that identify the object first and the rarely-used transport
    controls last.  @ref RestSchemaActions::RestSchemaActionInfo::field_order "field_order" overrides this.

    Flattening is conservative: it removes transport groupings, never meaningful domain objects.  A body
    property that is itself an object stays an object.

    @note When the request body is optional, the requiredness of the properties lifted out of it is relaxed
    to optional, because a request that omits the body entirely is valid; @ref unflatten() sends no body at
    all when no body-located option was supplied.  A partially supplied body is still rejected, by the
    schema's own request validation, with a precise error.

    @section rsa_collisions Collisions

    Two transport locations can carry the same name - a path variable \c id and a body property \c id, say.
    Rather than silently choose one, the collision is reported, and the manifest resolves it by renaming
    either side through a location-qualified overlay key (\c "body.id").
*/
public class RestSchemaActionOperation {
    public {
        #! The order in which transport locations contribute fields to the action request
        /** The host key comes first: it names the object the request is addressed to, and until it has a
            value there is no host for the request to go to at all.
        */
        const LocationOrder = (RSAL_HOST, RSAL_PATH, RSAL_BODY, RSAL_BODY_ROOT, RSAL_QUERY, RSAL_HEADER);
    }

    private {
        #! The schema owning the operation
        AbstractRestSchemaValidator schema;

        #! The manifest entry for this action
        hash<RestSchemaActionInfo> info;

        #! The path item object for the operation
        OpenApi3::PathItemObject pio;

        #! The operation object
        OpenApi3::OperationObject op;

        #! The flattened request type with the overlay applied
        *AbstractDataProviderType request_type;

        #! Values sent but not asked for, each following another option
        /** @see @ref RestSchemaActions::RestSchemaFieldOverlayInfo::value_from "value_from"
        */
        list<hash<RestSchemaDerivedValueInfo>> derived_values = ();

        #! The envelope the body options are rebuilt into, if the manifest lifted them out of one
        /** @see @ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap"
        */
        *list<string> request_wrap;

        #! The fixed values that envelope carries beside the lifted body
        list<hash<RestSchemaRequestWrapValueInfo>> request_wrap_fixed = ();

        #! The operation declares a **required** object body whose properties were flattened into options
        /** An operation can declare a required body every property of which is optional, and then the
            minimal valid request carries an empty object rather than no body at all: Pinecone's index
            statistics take an optional metadata filter and nothing else, so the ordinary call - "how
            many records are in this index" - supplies no option and must still send \c {}.

            Sending nothing there fails the schema's own request validation before the request is made,
            which reads as the action being broken.  An **optional** body is unaffected and still sends
            nothing when nothing is supplied.
        */
        bool body_required_object = False;

        #! Values assembled from options published in their place
        /** @see @ref RestSchemaActions::RestSchemaFieldOverlayInfo::lift "lift"
        */
        list<hash<RestSchemaLiftedValueInfo>> lifted_values = ();

        #! Header values dropped by the overlay, which the connection is expected to supply
        /** An ignored header is one the connection owns - an API key, the tenant a Xero connection works
            in - so the action must not ask for it, and the request must still carry it.  These are
            filled in from the REST client's own default headers when the request is built, and a
            required one the connection does not send is reported as the connection problem it is.
        */
        list<hash<RestSchemaConnectionHeaderInfo>> connection_headers = ();

        #! The action output type: the success response body, unwrapped
        *AbstractDataProviderType response_type;

        #! The reverse map from action option name to transport location
        hash<string, hash<RestSchemaFlatFieldInfo>> flat_map = {};

        #! Request value conversion tree, keyed by action option name
        hash<string, hash<auto>> request_conv = {};

        #! Response value conversion tree for the response body
        *hash<auto> response_conv;

        #! The HTTP status code of the response published as the output type
        *string response_code;

        #! Path variable names in the order they appear in the URI path template
        list<string> path_var_names = ();

        #! The host source this action takes its request host from, if any
        *hash<RestSchemaHostSourceInfo> host_source;

        #! The published name of the synthesized host key option, if this action has one
        /** Not the same as @ref RestSchemaActions::RestSchemaHostSourceInfo::key_option "key_option" when
            an overlay renamed it, which is what the request carries.
        */
        *string host_key_option;
    }

    #! Resolves the manifest entry against the schema and derives the action's types
    /** @param schema the schema owning the operation
        @param info the manifest entry
        @param host_source the host source named by
        @ref RestSchemaActions::RestSchemaActionInfo::host_from "host_from", resolved by the action set;
        required whenever the manifest entry names one, because the action gains an option from it

        @throw REST-SCHEMA-ACTION-ERROR the operation cannot be resolved, the overlay does not apply, or the
        output contract is ambiguous
    */
    constructor(AbstractRestSchemaValidator schema, hash<RestSchemaActionInfo> info,
            *hash<RestSchemaHostSourceInfo> host_source) {
        self.schema = schema;
        self.info = info;
        self.host_source = host_source;
        on_error rethrow $1.err, sprintf("%s (while building action %y from \"%s %s\")", $1.desc, info.action,
            info.method.upr(), info.path);

        if (!(schema instanceof OpenApi3Schema)) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("schema class %y is not supported; application actions "
                "can currently only be generated from OpenAPI 3 schemas (class \"OpenApi3Schema\")",
                schema.className());
        }
        resolveOperation();
        buildRequestType();
        buildResponseType();
    }

    #! Returns the stable action ID
    string getAction() {
        return info.action;
    }

    #! Returns the manifest entry
    hash<RestSchemaActionInfo> getInfo() {
        return info;
    }

    #! Returns the host source this action takes its request host from, if any
    *hash<RestSchemaHostSourceInfo> getHostSource() {
        return host_source;
    }

    #! Returns the published name of the option naming the object whose host the request goes to, if any
    /** The option is synthesized from the host source rather than derived from the schema, so it is the
        one option an action can have that the schema knows nothing about; it is consumed when the host is
        resolved and never sent.

        @return the option name, or @ref nothing when the action sends to the connection's own host

        @see @ref RestSchemaActions::RSAL_HOST "RSAL_HOST"
    */
    *string getHostKeyOption() {
        return host_key_option;
    }

    #! Returns the HTTP method in UPPER case
    string getMethod() {
        return info.method.upr();
    }

    #! Returns the OpenAPI path template
    string getPath() {
        return info.path;
    }

    #! Returns the schema operation object
    OpenApi3::OperationObject getOperation() {
        return op;
    }

    #! Returns the schema path item object
    OpenApi3::PathItemObject getPathItem() {
        return pio;
    }

    #! Returns the flattened request type, or @ref nothing if the action takes no input
    *AbstractDataProviderType getRequestType() {
        return request_type;
    }

    #! Returns the action output type, or @ref nothing if the operation returns no body
    *AbstractDataProviderType getResponseType() {
        return response_type;
    }

    #! Returns the HTTP status code of the response published as the output type, if any
    *string getResponseCode() {
        return response_code;
    }

    #! Returns the header values the connection is expected to supply
    /** These are the headers the overlay dropped with
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ignore "ignore".  Dropping one hides it from the
        action, which is the point, but the schema may still declare it required - so the request is filled
        in from the REST client's own default headers rather than being sent without it.

        @see @ref RestSchemaActions::RestSchemaActionDataProvider "RestSchemaActionDataProvider"
    */
    list<hash<RestSchemaConnectionHeaderInfo>> getConnectionHeaders() {
        return connection_headers;
    }

    #! Returns the reverse map from action option name to transport location
    hash<string, hash<RestSchemaFlatFieldInfo>> getFlatMap() {
        return flat_map;
    }

    #! Returns the action's request fields in presentation order
    *hash<string, AbstractDataField> getRequestFields() {
        return request_type ? request_type.getFields() : NOTHING;
    }

    #! Converts flat action options into the transport request
    /** @param options the action options, keyed by action option name

        @return a hash with the following keys:
        - \c req: the request to pass to
          @ref RestSchemaDataProvider::RestSchemaRequestDataProvider "RestSchemaRequestDataProvider"
        - \c path_vars: the URI path variables to pass as a request option

        @throw REST-SCHEMA-ACTION-ERROR an unknown option was supplied or a path variable is missing
    */
    hash<auto> unflatten(*hash<auto> options) {
        hash<auto> req = {};
        hash<auto> path_vars = {};

        # every option is validated before anything is assembled, because the values assembled for lifted
        # options below are placed in this hash and would otherwise be indistinguishable from supplied ones
        foreach string name in (keys options) {
            if (!exists flat_map{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y has no option %y; known options: %y",
                    info.action, name, sort(keys flat_map));
            }
        }
        validateOptionGroups(options);
        # a value whose properties were published in its place is assembled from them here, so that it
        # travels as the single value the schema declares
        foreach hash<RestSchemaLiftedValueInfo> lv in (lifted_values) {
            hash<auto!> assembled = {};
            foreach hash<auto> i in (lv.options.pairIterator()) {
                if (options.hasKey(i.value)) {
                    assembled{i.key} = options{i.value};
                    options -= i.value;
                }
            }
            if (!assembled) {
                continue;
            }
            auto value = assembled;
            if (lv.conv) {
                value = RestSchemaActionOperation::convertValue(value, lv.conv, True);
                # a codec that converts to nothing is saying there is nothing to send
                if (!exists value) {
                    continue;
                }
            }
            placeRequestValue(\req, \path_vars, lv.loc, lv.wire_name, lv.wire_path, value);
        }

        foreach hash<auto> i in (options.pairIterator()) {
            hash<RestSchemaFlatFieldInfo> fi = flat_map{i.key};
            auto value = i.value;
            if (value instanceof hash<RestSchemaRawValue>) {
                # the value is already what the wire takes - a record table's rendered query is - so the
                # option's codec is not applied to it a second time
                placeRequestValue(\req, \path_vars, fi.loc, fi.wire_name, fi.wire_path,
                    cast<hash<RestSchemaRawValue>>(value).value);
                continue;
            }
            if (*hash<auto> node = request_conv{i.key}) {
                value = RestSchemaActionOperation::convertValue(value, node, True);
                # a codec that converts to nothing is saying there is nothing to send: placing it anyway
                # would put an empty value in the request, which a schema that constrains the argument
                # rejects - and an option the caller never filled in would be making the call fail
                if (!exists value) {
                    continue;
                }
            }
            placeRequestValue(\req, \path_vars, fi.loc, fi.wire_name, fi.wire_path, value);
        }
        # an operation declaring a required object body must send one even when no property of it was
        # supplied: the minimal valid request is an empty object, and sending no body at all fails the
        # schema's own request validation before the request is made
        if (body_required_object && !exists req.body) {
            req.body = {};
        }

        # the values the envelope fixes are added once some part of the body has been supplied; an action
        # whose body is entirely optional and left unset sends no body at all
        if (request_wrap_fixed && exists req.body) {
            foreach hash<RestSchemaRequestWrapValueInfo> fv in (request_wrap_fixed) {
                req.body = RestSchemaActionOperation::setNested(req.body, fv.path, fv.name, fv.value);
            }
        }

        # a value that follows another option is filled in once every supplied option has been placed, so
        # that it carries exactly what the request will send rather than what the caller typed
        foreach hash<RestSchemaDerivedValueInfo> dv in (derived_values) {
            *hash<RestSchemaFlatFieldInfo> src = flat_map{dv.source};
            if (!src) {
                continue;
            }
            auto value;
            switch (src.loc) {
                case RSAL_PATH: value = path_vars{src.wire_name}; break;
                case RSAL_QUERY: value = req.query{src.wire_name}; break;
                case RSAL_HEADER: value = req.header{src.wire_name}; break;
                case RSAL_BODY:
                    value = RestSchemaActionOperation::getNested(req.body, src.wire_path, src.wire_name);
                    break;
                case RSAL_BODY_ROOT: value = req.body; break;
            }
            if (!exists value) {
                continue;
            }
            placeRequestValue(\req, \path_vars, dv.loc, dv.wire_name, dv.wire_path, value);
        }

        # every path variable must resolve, or the request would be made against a URI path still containing
        # a literal "{name}" placeholder
        foreach string name in (path_var_names) {
            if (!exists path_vars{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y is missing a value for URI path variable "
                    "%y in path %y", info.action, name, info.path);
            }
        }
        return {
            "req": req,
            "path_vars": path_vars,
        };
    }

    #! Converts a transport request back into flat action options
    /** The inverse of @ref unflatten(); given valid action options,
        \c flatten(unflatten(options)) returns the original options, except for values a wire codec converts.

        @param wire a hash as returned by @ref unflatten()

        @return the action options

        @throw REST-SCHEMA-ACTION-ERROR the request contains a value the action does not declare
    */
    hash<auto> flatten(hash<auto> wire) {
        # index the reverse map by location and wire name; an option published in another value's place
        # does not travel on its own, so it is not addressable here - the value it is assembled into is
        hash<string, hash<string, string>> rmap = {};
        map rmap{$1.loc}{$1.wire_name} = $1.name, flat_map.iterator(), !$1.lift_into.val();

        hash<auto> rv = {};
        code add = sub (string loc, string wire_name, auto value) {
            *string name = rmap{loc}{wire_name};
            if (!name) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y does not declare a %s value named %y; "
                    "declared %s values: %y", info.action, loc, wire_name, loc, sort(keys rmap{loc}));
            }
            if (*hash<auto> node = request_conv{name}) {
                value = RestSchemaActionOperation::convertValue(value, node, False);
            }
            rv{name} = value;
        };

        # a lifted value is read from the request and split back into the options published in its place,
        # before the locations it came from are walked, so that it is not reported as undeclared there
        hash<string, hash<string, bool>> lifted_seen = {};
        foreach hash<RestSchemaLiftedValueInfo> lv in (lifted_values) {
            lifted_seen{lv.loc}{lv.wire_name} = True;
            auto value;
            switch (lv.loc) {
                case RSAL_QUERY: value = wire.req.query{lv.wire_name}; break;
                case RSAL_HEADER: value = wire.req.header{lv.wire_name}; break;
                case RSAL_PATH: value = wire.path_vars{lv.wire_name}; break;
                case RSAL_BODY:
                    value = RestSchemaActionOperation::getNested(wire.req.body, lv.wire_path, lv.wire_name);
                    break;
            }
            if (!exists value) {
                continue;
            }
            if (lv.conv) {
                value = RestSchemaActionOperation::convertValue(value, lv.conv, False);
            }
            if (value.typeCode() != NT_HASH) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y publishes the properties of %s value %y "
                    "as options, but the request carries %y there rather than an object", info.action, lv.loc,
                    lv.wire_name, value.type());
            }
            foreach hash<auto> i in (value.pairIterator()) {
                *string opt = lv.options{i.key};
                if (!opt) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y does not publish a property named %y "
                        "of %s value %y; published properties: %y", info.action, i.key, lv.loc, lv.wire_name,
                        sort(keys lv.options));
                }
                rv{opt} = i.value;
            }
        }

        code addUnlifted = sub (string loc, string wire_name, auto value) {
            if (lifted_seen{loc}{wire_name}) {
                return;
            }
            add(loc, wire_name, value);
        };
        map addUnlifted(RSAL_PATH, $1.key, $1.value), wire.path_vars.pairIterator();
        map addUnlifted(RSAL_QUERY, $1.key, $1.value), wire.req.query.pairIterator();
        map addUnlifted(RSAL_HEADER, $1.key, $1.value), wire.req.header.pairIterator();
        if (exists wire.req.body) {
            if (rmap{RSAL_BODY_ROOT}) {
                add(RSAL_BODY_ROOT, "body", wire.req.body);
            } else {
                # the body may be nested, because the manifest can lift the action's options out of a fixed
                # envelope; each value is looked up by the full path it was found at
                hash<string, string> at_path = getBodyPathMap();
                hash<string, bool> envelope = getBodyEnvelopePaths();
                code walk = sub (auto node, list<string> path) {
                    foreach hash<auto> i in (node.pairIterator()) {
                        list<string> here = path + i.key;
                        string key = here.join(".");
                        if (exists at_path{key}) {
                            # a lifted value has already been split into the options published in its place
                            if (at_path{key}.val()) {
                                if (*hash<auto> cnode = request_conv{at_path{key}}) {
                                    rv{at_path{key}} = RestSchemaActionOperation::convertValue(i.value, cnode,
                                        False);
                                } else {
                                    rv{at_path{key}} = i.value;
                                }
                            }
                            continue;
                        }
                        if (envelope{key} && i.value.typeCode() == NT_HASH) {
                            walk(i.value, here);
                            continue;
                        }
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y does not declare a %s value named "
                            "%y; declared %s values: %y", info.action, RSAL_BODY, key, RSAL_BODY,
                            sort(keys at_path));
                    }
                };
                walk(wire.req.body, ());
            }
        }
        return rv;
    }

    #! Returns the option name carried by each body path, empty for a lifted or fixed value
    /** A lifted value and a value the envelope fixes are not options, but they are declared, so they map
        to an empty name rather than being absent: absent is what makes a value undeclared.
    */
    private hash<string, string> getBodyPathMap() {
        hash<string, string> rv = {};
        foreach hash<RestSchemaFlatFieldInfo> fi in (flat_map.iterator()) {
            if (fi.loc != RSAL_BODY || fi.lift_into.val()) {
                continue;
            }
            rv{((fi.wire_path ?? ()) + fi.wire_name).join(".")} = fi.name;
        }
        map rv{(($1.wire_path ?? ()) + $1.wire_name).join(".")} = "", lifted_values.iterator(),
            $1.loc == RSAL_BODY;
        map rv{($1.path + $1.name).join(".")} = "", request_wrap_fixed.iterator();
        return rv;
    }

    #! Returns every envelope level a body value sits below, as a set of dotted paths
    /** Each level of each path is included, so that walking a nested body can tell an envelope level it is
        expected to descend through from a value nobody declared.
    */
    private hash<string, bool> getBodyEnvelopePaths() {
        hash<string, bool> rv = {};
        code add = sub (*list<string> path) {
            list<string> here = ();
            foreach string level in (path) {
                here += level;
                rv{here.join(".")} = True;
            }
        };
        map add($1.wire_path), flat_map.iterator(), $1.loc == RSAL_BODY && !$1.lift_into.val();
        map add($1.wire_path), lifted_values.iterator(), $1.loc == RSAL_BODY;
        map add($1.path), request_wrap_fixed.iterator();
        return rv;
    }

    #! Places a value in the transport request at its location, descending into the body when it is nested
    private placeRequestValue(reference<hash<auto>> req, reference<hash<auto>> path_vars, string loc,
            string wire_name, *list<string> wire_path, auto value) {
        switch (loc) {
            case RSAL_PATH:
                path_vars{wire_name} = value;
                break;
            case RSAL_QUERY:
                req.query{wire_name} = value;
                break;
            case RSAL_HEADER:
                req.header{wire_name} = value;
                break;
            case RSAL_BODY:
                req.body = RestSchemaActionOperation::setNested(req.body ?? {}, wire_path, wire_name, value);
                break;
            case RSAL_BODY_ROOT:
                req.body = value;
                break;
            case RSAL_HOST:
                # the host key is not sent: it was consumed when the request host was resolved
                break;
        }
    }

    #! Enforces the same nonempty alternatives and exclusions advertised by the action's form
    private validateOptionGroups(*hash<auto> options) {
        hash<string, bool> groups;
        foreach hash<auto> entry in (getRequestFields().pairIterator()) {
            *hash<auto> attr = entry.value.getAttributes();
            bool supplied = exists options{entry.key} && options{entry.key}.val();
            foreach string group in (attr.required_groups) {
                groups{group} = groups{group} || supplied;
            }
            if (supplied) {
                foreach string other in (attr.exclusive_with) {
                    if (exists options{other} && options{other}.val()) {
                        throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y: options %y and %y cannot "
                            "be supplied together", info.action, entry.key, other);
                    }
                }
            }
        }
        foreach hash<auto> group in (groups.pairIterator()) {
            if (!group.value) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y: at least one option in required "
                    "group %y must have a value", info.action, group.key);
            }
        }
    }

    #! Returns the hash with the value set at the given path below it, creating the levels it names
    private static hash<auto> setNested(hash<auto> h, *list<string> path, string name, auto value) {
        if (!path) {
            # "auto!" so that a hash whose values happen to share a type does not narrow and then reject
            # a value of another type
            hash<auto!> rv = h;
            rv{name} = value;
            return rv;
        }
        string head = path[0];
        hash<auto> below = {};
        if (h{head}.typeCode() == NT_HASH) {
            below = h{head};
        }
        hash<auto!> rv = h;
        rv{head} = RestSchemaActionOperation::setNested(below, path.size() > 1 ? path[1..] : NOTHING, name,
            value);
        return rv;
    }

    #! Returns the value at the given path below the hash, or @ref nothing if any level is missing
    private static auto getNested(auto h, *list<string> path, string name) {
        foreach string level in (path) {
            if (h.typeCode() != NT_HASH) {
                return;
            }
            h = h{level};
        }
        return h.typeCode() == NT_HASH ? h{name} : NOTHING;
    }

    #! Applies response unwrapping and any response wire codecs to a response body
    /** @param body the success response body

        @return the value to publish as the action's output
    */
    auto processResponse(auto body) {
        foreach string name in (info.response_unwrap) {
            if (!exists body) {
                break;
            }
            if (body.typeCode() != NT_HASH) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y cannot unwrap response field %y: the "
                    "enclosing value has type %y, not a hash (unwrap path: %y)", info.action, name,
                    body.type(), info.response_unwrap);
            }
            body = body{name};
        }
        if (response_conv && exists body) {
            body = RestSchemaActionOperation::convertValue(body, response_conv, False);
        }
        return body;
    }

    #! Resolves the manifest entry to a path item and operation in the schema
    private resolveOperation() {
        OpenApi3Schema openapi3 = cast<OpenApi3Schema>(schema);
        *OpenApi3::PathItemObject p;
        *string match_error;
        if (openapi3.paths) {
            try {
                p = openapi3.paths.tryMatch(info.path);
            } catch (hash<ExceptionInfo> ex) {
                # an unmatched path component is reported as a validation error rather than by returning
                # nothing; the detail says which components exist, so it is worth keeping
                if (ex.err != "SCHEMA-VALIDATION-ERROR") {
                    rethrow;
                }
                match_error = ex.desc;
            }
        }
        if (!p) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("path %y is not declared in the schema%s", info.path,
                match_error.val() ? ": " + match_error : "");
        }
        pio = p;
        string method = info.method.lwr();
        hash<string, bool> methods = (map {$1: True}, pio.getMethods()) ?? {};
        if (!methods{method}) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("path %y declares no %y operation; declared operations: "
                "%y", info.path, info.method.upr(), (map $1.upr(), sort(keys methods)));
        }
        op = pio.getOperation(method, info.path);

        # the operationId locates an operation and is checked for drift, but is never action identity; a
        # rename upstream must be reported here rather than silently renaming an action in use
        if (info.operation_id.val() && op.operationId != info.operation_id) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("the schema declares operationId %y for \"%s %s\", but the "
                "manifest expects %y; this is upstream schema drift: confirm the operation is still the same "
                "one and update the manifest", op.operationId, info.method.upr(), info.path, info.operation_id);
        }

        # record path variables in the order they appear in the path template
        path_var_names = info.path =~ x/\{([^}]+)\}/g ?? ();
    }

    #! Builds the flattened request type with the overlay applied
    private buildRequestType() {
        *hash<string, OpenApi3::ParameterBase> params = pio.parameters + op.parameters;

        # collect the derived fields per location, in transport order
        hash<string, list<hash<auto>>> derived = {};
        addHostKeyField(\derived);
        foreach hash<auto> i in (params.pairIterator()) {
            OpenApi3::ParameterBase param = i.value;
            switch (param.inLoc) {
                case RSAL_PATH:
                case RSAL_QUERY:
                case RSAL_HEADER:
                    derived{param.inLoc} += {
                        "wire_name": param.name,
                        # a path variable is part of the URI and is therefore always required
                        "required": param.inLoc == RSAL_PATH || param.required ? True : False,
                        "field": schema.getFieldFromSchema(param.name, param.desc, param.schema,
                            param.inLoc == RSAL_PATH || param.required),
                    };
                    break;
            }
        }
        addBodyFields(\derived);

        # a path variable declared in the path template but not described by any parameter object would
        # otherwise be silently unfillable
        hash<string, bool> have_path = (map {$1.wire_name: True}, derived{RSAL_PATH}) ?? {};
        foreach string name in (path_var_names) {
            if (!have_path{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("URI path %y contains variable %y, but the schema "
                    "declares no path parameter with that name; declared path parameters: %y", info.path, name,
                    (map $1.wire_name, derived{RSAL_PATH}));
            }
        }

        # apply the overlay, resolve renames, and detect collisions
        hash<string, AbstractDataField> fields = {};
        hash<string, string> origin = {};
        foreach string loc in (LocationOrder) {
            foreach hash<auto> entry in (derived{loc}) {
                # a value lifted out of an envelope is addressed in the overlay by its path within the
                # body, so that two levels of the envelope can both declare a property of the same name
                string derived_name = entry.ov_name ?? entry.wire_name;
                hash<RestSchemaFieldOverlayInfo> ov = getFieldOverlay(info.fields, loc, derived_name);
                if (entry.base_overlay) {
                    # the host source describes its key option once, for every action that names it; an
                    # action's own overlay refines that rather than replacing it, so that a manifest does
                    # not repeat the same display name and dropdown on two dozen actions
                    ov = RestSchemaActionOperation::mergeOverlay(entry.base_overlay, ov);
                }
                if (ov.ignore || entry.read_only) {
                    # a header the action does not ask for is one the connection supplies; the schema may
                    # still require it, and dropping it from the action must not produce a request the
                    # schema rejects
                    if (loc == RSAL_HEADER) {
                        connection_headers += <RestSchemaConnectionHeaderInfo>{
                            "wire_name": derived_name,
                            "required": entry.required ?? False,
                            "connection_option": ov.connection_option,
                        };
                    }
                    continue;
                }
                if (ov.value_from.val()) {
                    # not an option: the value follows another one and is filled in when the request is
                    # built
                    derived_values += <RestSchemaDerivedValueInfo>{
                        "source": ov.value_from,
                        "loc": loc,
                        "wire_name": entry.wire_name,
                        "wire_path": entry.wire_path,
                    };
                    continue;
                }
                string name = ov.name ?? entry.wire_name;
                if (!ov.lift && exists fields{name}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("option %y is produced by both the %s and the %s "
                        "value named %y; rename one of them with a location-qualified overlay key "
                        "(ex: \"%s.%s\") giving a \"name\"", name, origin{name}, loc, derived_name, loc,
                        derived_name);
                }
                *hash<auto> node;
                AbstractDataField field = applyFieldOverlay(entry.field, ov, name,
                    sprintf("%s value %y", loc, derived_name), \node);
                if (ov.lift) {
                    liftFieldOptions(field, loc, entry, node, sprintf("%s value %y", loc, derived_name),
                        \fields, \origin);
                    continue;
                }
                fields{name} = field;
                if (node) {
                    request_conv{name} = node;
                }
                origin{name} = loc;
                if (loc == RSAL_HOST) {
                    host_key_option = name;
                }
                flat_map{name} = <RestSchemaFlatFieldInfo>{
                    "name": name,
                    "loc": loc,
                    "wire_name": loc == RSAL_BODY_ROOT ? "body" : entry.wire_name,
                    "wire_path": entry.wire_path,
                    "codec": ov.codec,
                };
            }
        }

        # every overlay entry must resolve, so that an upstream rename cannot silently drop an override
        checkOverlayTargets(info.fields, derived, keys fields);

        # a value that follows an option the action does not have could never be filled in, and the
        # request would go out missing something the API requires
        foreach hash<RestSchemaDerivedValueInfo> dv in (derived_values) {
            if (!exists flat_map{dv.source}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the %s value %y takes its value from option %y, "
                    "which this action does not declare; options: %y", dv.loc, dv.wire_name, dv.source,
                    sort(keys flat_map));
            }
        }

        if (!fields) {
            return;
        }
        fields = orderFields(fields);
        bool required = False;
        foreach AbstractDataField f in (fields.values()) {
            if (fieldIsRequired(f)) {
                required = True;
                break;
            }
        }
        request_type = new HashDataType(required ? AutoHashType : AutoHashOrNothingType,
            sprintf("%s: request", info.action), fields);
    }

    #! Adds the option naming the object whose host the request goes to, for an action that has a host source
    /** The option is synthesized rather than derived, because the operation does not describe it: an API
        that shards its data plane per object addresses the object with the **host**, so its description
        declares no parameter for it anywhere.  Without this the action would offer no way to say which
        object it is for, and the host could never be resolved.

        It is a required string, presented first, and carries whatever presentation the host source
        declares - a display name, a description, and usually a
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ref_data "ref_data" dropdown backed by the same
        API's listing, so that the user picks the object by name and never sees a host at all.

        A schema that happens to declare a value of the same name collides with it, and is reported exactly
        as any other collision between two locations is.

        @param derived the derived field map, added to here
    */
    private addHostKeyField(reference<hash<string, list<hash<auto>>>> derived) {
        if (!info.host_from.val()) {
            return;
        }
        if (!host_source) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y takes its host from host source %y, but no "
                "such host source was supplied; it has to be declared by this action set or one of its "
                "peers", info.action, info.host_from);
        }
        hash<RestSchemaFieldOverlayInfo> base = host_source.key_overlay ?? <RestSchemaFieldOverlayInfo>{};
        if (!base.desc.val()) {
            base.desc = host_source.desc ?? sprintf("Names the object whose host this request is sent to; "
                "resolved by the `%s` action.", host_source.action);
        }
        if (!exists base.required) {
            base.required = True;
        }
        derived{RSAL_HOST} += {
            "wire_name": host_source.key_option,
            "required": True,
            "base_overlay": base,
            "field": new QoreDataField(host_source.key_option, base.desc,
                AbstractDataProviderTypeMap."string"),
        };
    }

    #! Returns an overlay with every value the second one actually sets applied over the first
    /** A hashdecl always carries every key, so a plain hash addition would let the unset keys of the
        refining overlay erase the values of the base one.

        @param base the overlay to start from
        @param over the overlay refining it

        @return the merged overlay
    */
    private static hash<RestSchemaFieldOverlayInfo> mergeOverlay(hash<RestSchemaFieldOverlayInfo> base,
            hash<RestSchemaFieldOverlayInfo> over) {
        # "auto!" because the overlay's values are of every type
        hash<auto!> rv = base;
        foreach hash<auto> i in (over.pairIterator()) {
            if (exists i.value) {
                rv{i.key} = i.value;
            }
        }
        return cast<hash<RestSchemaFieldOverlayInfo>>(rv);
    }

    #! Adds the request body's contribution to the derived field map
    private addBodyFields(reference<hash<string, list<hash<auto>>>> derived) {
        *OpenApi3::ParameterObject body = op.body ?? pio.body;
        if (!body || !body.schema) {
            return;
        }
        bool body_required = body.required ?? False;
        AbstractDataField body_field = op.getRequestBodyField(pio);
        AbstractDataProviderType body_type = body_field.getType();
        *hash<string, AbstractDataField> union_fields;
        if (info.flatten_body && body_type instanceof QoreUnionDataType) {
            union_fields = RestSchemaActionOperation::getUnionBodyFields(body_type);
        }

        # an object body is flattened to its properties; anything else -- an array or a scalar -- has no
        # properties to lift and is presented whole.  The distinction is the base type, not whether any
        # fields came back: an object body declaring no properties contributes no options at all, and
        # presenting an empty envelope as a "body" option would be worse than useless.  Some schemas
        # declare exactly that on operations that take no input.
        if (!info.flatten_body || body_type.getBaseTypeCode() != NT_HASH
                || (body_type instanceof QoreUnionDataType && !union_fields)) {
            derived{RSAL_BODY_ROOT} += {
                "wire_name": "body",
                "field": body_field,
            };
            return;
        }
        # a required object body still has to be sent when every property of it is optional and none was
        # supplied; see body_required_object
        body_required_object = body_required;

        # a body wrapped in a fixed envelope is lifted from inside it rather than from its top level; see
        # RestSchemaActionInfo::request_unwrap
        auto landing_schema = body.schema;
        if (info.request_unwrap) {
            body_type = buildRequestUnwrap(body.schema, body_type, \landing_schema, \derived, body_required);
            union_fields = body_type instanceof QoreUnionDataType
                ? RestSchemaActionOperation::getUnionBodyFields(body_type) : NOTHING;
        }

        *hash<string, AbstractDataField> body_fields = union_fields ?? body_type.getFields();
        if (!body_fields) {
            return;
        }
        hash<string, bool> fixed = getFixedProperties(landing_schema);
        foreach hash<auto> i in (body_fields.pairIterator()) {
            auto property = RestSchemaActionOperation::getSchemaProperty(landing_schema, i.key);
            if (property.readOnly) {
                # Response-owned values must not become editable request options, even when an API
                # reuses the same object schema for creating and retrieving a resource. Keep the
                # declaration addressable so existing manifests may still explicitly ignore it.
                derived{RSAL_BODY} += {
                    "wire_name": i.key,
                    "wire_path": request_wrap,
                    "field": i.value,
                    "read_only": True,
                };
                continue;
            }
            if (fixed{i.key}) {
                # a discriminator the level the options were lifted to fixes is filled in rather than
                # published, exactly as one beside the path is: there is nothing for a caller to choose,
                # and publishing it as a required option would make every existing call fail
                request_wrap_fixed += <RestSchemaRequestWrapValueInfo>{
                    "path": request_wrap ?? (),
                    "name": i.key,
                    "value": RestSchemaActionOperation::getSchemaEnum(
                        RestSchemaActionOperation::getSchemaProperty(landing_schema, i.key))[0],
                };
                continue;
            }
            AbstractDataField f = i.value;
            if (!body_required && fieldIsRequired(f)) {
                # omitting the body entirely is valid, so a property of it cannot be required at the action
                # level; a partially supplied body is still rejected by the schema's request validation
                f = f.getOrNothingType();
            }
            # a property at the level the options were lifted to is "the body" as far as the action is
            # concerned, so it is addressed by its own name; only a property of an outer envelope level
            # needs the path to tell it apart from one of the same name further in
            derived{RSAL_BODY} += {
                "wire_name": i.key,
                "wire_path": request_wrap,
                "field": f,
            };
        }
    }

    #! Collects the editable fields of object alternatives without choosing a request variant
    /** Only fields required in every alternative remain required. Branch-specific defaults are omitted
        so that opening a form cannot select a model or add options belonging to a different model.
        The original composed schema still validates the complete, reassembled request before I/O.
        Mixed scalar/object and open-object unions remain whole body options.
    */
    private static *hash<string, AbstractDataField> getUnionBodyFields(AbstractDataProviderType type) {
        if (!(type instanceof QoreUnionDataType)) {
            return type.getBaseTypeCode() == NT_HASH ? type.getFields() : NOTHING;
        }
        list<AbstractDataProviderType> branches = cast<QoreUnionDataType>(type).getUnionTypes();
        hash<string, list<AbstractDataField>> members = {};
        foreach AbstractDataProviderType branch in (branches) {
            *hash<string, AbstractDataField> fields = RestSchemaActionOperation::getUnionBodyFields(branch);
            if (!fields) {
                return;
            }
            foreach hash<auto> f in (fields.pairIterator()) {
                members{f.key} += f.value;
            }
        }
        hash<string, AbstractDataField> rv = {};
        foreach hash<auto> member in (members.pairIterator()) {
            list<AbstractDataField> fields = member.value;
            AbstractDataField first = fields[0];
            hash<auto!> cfg = first.getConfigInfo();
            list<AbstractDataProviderType> types = map $1.getType(), fields;
            bool same_type = True;
            bool required = fields.size() == branches.size();
            bool same_default = required;
            bool finite = True;
            list<hash<AllowedValueInfo>> allowed = ();
            hash<string, bool> seen = {};
            foreach AbstractDataField field in (fields) {
                same_type = same_type && field.getType() == first.getType();
                required = required && fieldIsRequired(field);
                same_default = same_default && field.getDefaultValue() == first.getDefaultValue();
                finite = finite && !!field.getAllowedValues() && !field.getAllowedValuesCreatable();
                foreach hash<AllowedValueInfo> value in (field.getAllowedValues() ?? ()) {
                    string key = make_json(value.value);
                    if (!seen{key}) {
                        seen{key} = True;
                        allowed += value;
                    }
                }
                if (field.getAttributes().sensitive) {
                    cfg.attr.sensitive = True;
                }
            }
            @assert(types.size() > 0);
            AbstractDataProviderType field_type = (types.size() == 1 || same_type)
                ? first.getType() : new QoreUnionDataType(types);
            cfg.type = required ? field_type : field_type.getOrNothingType();
            cfg.attr.required = required;
            cfg.default_value = same_default ? first.getDefaultValue() : NOTHING;
            cfg.allowed_values = finite ? allowed : NOTHING;
            cfg.allowed_values_creatable = !finite;
            # Element restrictions from just one branch must not constrain every alternative.
            if (!same_type) {
                remove cfg{"element_allowed_values", "element_allowed_values_creatable", "example_value"};
            }
            rv{member.key} = new QoreDataField(cfg);
        }
        return rv;
    }

    #! Returns the properties of a schema object that are required and permit exactly one value
    /** Only meaningful under @ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap":
        an application that does not lift its body out of an envelope has not declared that the body is one,
        so a fixed property of it is still the caller's to send.
    */
    private hash<string, bool> getFixedProperties(auto level_schema) {
        hash<string, bool> rv = {};
        if (!info.request_unwrap) {
            return rv;
        }
        hash<string, bool> req = RestSchemaActionOperation::getSchemaRequired(level_schema);
        foreach hash<auto> i in ((RestSchemaActionOperation::getSchemaProperties(level_schema) ?? {})
                .pairIterator()) {
            if (req{i.key} && RestSchemaActionOperation::getSchemaEnum(i.value).size() == 1) {
                rv{i.key} = True;
            }
        }
        return rv;
    }

    #! Returns a schema object's declared properties, whatever schema class it is
    private static *hash<auto> getSchemaProperties(auto s) {
        return exists s.properties && s.properties.typeCode() == NT_HASH ? s.properties : NOTHING;
    }

    #! Returns a schema object's named property
    private static auto getSchemaProperty(auto s, string name) {
        *hash<auto> props = RestSchemaActionOperation::getSchemaProperties(s);
        return props{name};
    }

    #! Returns a schema object's required property names as a set
    private static hash<string, bool> getSchemaRequired(auto s) {
        hash<string, bool> rv = {};
        auto req = s.required;
        switch (req.typeCode()) {
            case NT_HASH:
                # some schema classes carry "required" as a set already
                map rv{$1} = True, keys req;
                break;
            case NT_LIST:
                map rv{$1.toString()} = True, req;
                break;
        }
        return rv;
    }

    #! Returns the values a schema object permits, if it fixes them
    /** A schema class stores these in \c enumValues; a plain deserialized schema hash spells it \c "enum".
        The two are read separately because accessing a member an object does not declare throws.
    */
    private static *softlist<auto> getSchemaEnum(auto s) {
        auto e = s.typeCode() == NT_HASH ? (s.enumValues ?? s."enum") : s.enumValues;
        switch (e.typeCode()) {
            case NT_HASH:
                return keys e;
            case NT_LIST:
                return e;
        }
    }

    #! Descends the request body's envelope and records how to rebuild it
    /** @param body_schema the request body's schema
        @param body_type the type derived from it
        @param landing_schema receives the schema at the end of the unwrap path
        @param derived receives the derived request fields
        @param body_required whether the request body is required

        @return the type at the end of the unwrap path, whose fields become the action's body options

        @throw REST-SCHEMA-ACTION-ERROR the path does not resolve, or a required sibling of the path cannot
        be filled in automatically and the manifest does not supply it

        @see @ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap"
    */
    private AbstractDataProviderType buildRequestUnwrap(auto body_schema, AbstractDataProviderType body_type,
            reference<auto> landing_schema, reference<hash<string, list<hash<auto>>>> derived,
            bool body_required) {
        AbstractDataProviderType t = body_type;
        auto s = body_schema;
        list<string> walked = ();
        foreach string name in (info.request_unwrap) {
            *hash<string, AbstractDataField> fields = t.getFields();
            *AbstractDataField f = fields{name};
            if (!f) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y cannot lift the request body through "
                    "%y: the value at %y declares no such property; it declares %y (unwrap path: %y)",
                    info.action, name, walked ? walked.join(".") : "the body root",
                    fields ? sort(keys fields) : (), info.request_unwrap);
            }
            AbstractDataProviderType next = f.getType();
            if (next.getBaseTypeCode() != NT_HASH) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y cannot lift the request body through "
                    "%y: that property has type %y, not an object (unwrap path: %y)", info.action, name,
                    next.getName(), info.request_unwrap);
            }
            # every property beside the path is accounted for, or the rebuilt body would be missing
            # something the API accepts in a way only the API would report
            recordUnwrapSiblings(s, t, name, walked, \derived, body_required);
            walked += name;
            s = RestSchemaActionOperation::getSchemaProperty(s, name);
            t = next;
        }
        request_wrap = info.request_unwrap;
        landing_schema = s;
        return t;
    }

    #! Accounts for the values a level of the request envelope carries beside the unwrap path
    /** A property the schema declares required with exactly one permitted value is filled in here, because
        there is nothing for a caller to choose - JSON:API's \c type discriminator is always this shape.
        Every other property becomes an option of the action at the level it belongs to, so that lifting the
        body does not withdraw part of the request contract: JSON:API carries the identity of the object
        being changed as \c data.id beside the attributes, and an update that could not send it would not
        work at all.
    */
    private recordUnwrapSiblings(auto level_schema, AbstractDataProviderType level_type, string next,
            list<string> walked, reference<hash<string, list<hash<auto>>>> derived, bool body_required) {
        *hash<auto> props = RestSchemaActionOperation::getSchemaProperties(level_schema);
        if (!props) {
            return;
        }
        *hash<string, AbstractDataField> tfields = level_type.getFields();
        hash<string, bool> req = RestSchemaActionOperation::getSchemaRequired(level_schema);
        foreach hash<auto> i in (props.pairIterator()) {
            if (i.key == next) {
                continue;
            }
            *softlist<auto> allowed = RestSchemaActionOperation::getSchemaEnum(i.value);
            if (req{i.key} && allowed.size() == 1) {
                request_wrap_fixed += <RestSchemaRequestWrapValueInfo>{
                    "path": walked,
                    "name": i.key,
                    "value": allowed[0],
                };
                continue;
            }
            *AbstractDataField f = tfields{i.key};
            if (!f) {
                # the schema declares the property but the derived type does not carry it, so there is no
                # type to publish an option with; the manifest cannot address what is not there
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y lifts the request body through %y, and "
                    "the envelope at %y declares %y beside it, which the derived type does not carry; it "
                    "carries %y (unwrap path: %y)", info.action, next,
                    walked ? walked.join(".") : "the body root", i.key,
                    tfields ? sort(keys tfields) : (), info.request_unwrap);
            }
            # a property of an envelope level is only ever as required as the body itself
            if (!body_required && fieldIsRequired(f)) {
                f = f.getOrNothingType();
            }
            derived{RSAL_BODY} += {
                "wire_name": i.key,
                "wire_path": walked,
                "ov_name": (walked + i.key).join("."),
                "field": f,
            };
        }
    }

    #! Builds the action output type from the selected success response
    private buildResponseType() {
        RestSchemaValidator::AbstractResponsesObject responses = op.getResponses();
        *hash<string, RestSchemaValidator::AbstractResponseObject> resp_hash = responses.getResponses();

        # success responses that carry a body; a response without one (204, say) cannot be an output contract
        list<string> codes = sort(map $1.key, resp_hash.pairIterator(),
            $1.key =~ /^2/ && $1.value && $1.value.getSchema());

        *RestSchemaValidator::AbstractResponseObject resp;
        if (info.response_code.val()) {
            resp = resp_hash{info.response_code};
            if (!resp) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the manifest selects response code %y, which the "
                    "operation does not declare; declared response codes: %y", info.response_code,
                    sort(keys resp_hash));
            }
            if (!resp.getSchema() && !info.response_envelope) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the manifest selects response code %y, which "
                    "declares no response body and therefore cannot provide an output type",
                    info.response_code);
            }
            response_code = info.response_code;
        } else if (codes.size() > 1) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("the operation declares %d success responses with a body "
                "(%y), so a single output contract cannot be derived; set \"response_code\" in the manifest to "
                "select one", codes.size(), codes);
        } else if (codes) {
            response_code = codes[0];
            resp = resp_hash{response_code};
        } else {
            if (info.response_envelope) {
                response_type = makeResponseEnvelopeType(NOTHING);
            }
            return;
        }

        if (!resp.getSchema()) {
            response_type = makeResponseEnvelopeType(NOTHING);
            return;
        }

        RestSchemaValidator::AbstractSchemaObject response_schema = resp.getSchema();
        if (info.response_content_type.val()) {
            if (!(resp instanceof OpenApi3::ResponseObject)
                    || !cast<OpenApi3::ResponseObject>(resp).contentTypes{info.response_content_type}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the manifest selects response media type %y, "
                    "which response %y does not declare", info.response_content_type, response_code);
            }
            *OpenApi3::SchemaObject selected = cast<OpenApi3::ResponseObject>(resp)
                .getContentSchema(info.response_content_type);
            if (!selected) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("response media type %y declares no schema",
                    info.response_content_type);
            }
            response_schema = selected;
        }
        AbstractDataField body_field = schema.getFieldFromSchema("body", "REST response body argument",
            response_schema);
        AbstractDataProviderType type = body_field.getType();
        # Large direction-neutral graphs (for example Stripe's expandable objects) can retain their
        # shared cached types. Copy only when readOnly/writeOnly makes validation direction significant.
        if (schema instanceof OpenApi3::OpenApi3Schema
                && cast<OpenApi3::OpenApi3Schema>(schema).hasDirectionalProperties()) {
            type = OpenApi3::OpenApi3HashDataType::forResponse(type);
        }

        foreach string name in (info.response_unwrap) {
            *hash<string, AbstractDataField> tfields = type.getFields();
            if (!tfields{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot unwrap response field %y: the response type "
                    "%y has no such field; fields: %y (unwrap path: %y)", name, type.getName(),
                    tfields ? sort(keys tfields) : (), info.response_unwrap);
            }
            type = tfields{name}.getType();
        }

        if (info.response_fields) {
            *hash<auto> node;
            type = applyTypeOverlay(type, info.response_fields, "response body", \node);
            response_conv = node;
        }
        response_type = info.response_envelope ? makeResponseEnvelopeType(type) : type;
    }

    #! Builds a typed HTTP envelope without inventing a body for a bodyless response
    private AbstractDataProviderType makeResponseEnvelopeType(*AbstractDataProviderType body_type) {
        HashDataType result("RestSchemaResponseEnvelope");
        result.addQoreFields({
            "status_code": {"type": IntType, "display_name": "HTTP Status",
                "desc": "HTTP response status code."},
            "headers": {"type": AutoHashType, "display_name": "Response Headers",
                "desc": "Response headers, including job locations and retry guidance."},
            "body": {"type": body_type ?? AbstractDataProviderType::get(new Type("nothing")),
                "display_name": "Response Body", "desc": "The typed response body, when present."},
        });
        return result;
    }

    #! Applies nested field overrides to each branch of a union that declares them
    /** A union's branches are alternative shapes for the same value, so an entry belongs to every branch
        that declares it and to no others; an entry no branch declares is a manifest error, exactly as it is
        for an object.

        A wire conversion is permitted only when every branch produces the same conversion tree. For
        example, a timestamp shared by complete responses and stream chunks can use one date codec.
        Branch-specific conversions are refused because selecting a conversion by field name would be
        ambiguous. Presentation-only overlays may still apply to a subset of the branches.

        @param type the union type
        @param ovl the nested field overrides
        @param ctx a description of the value for error messages
        @param node the common conversion applied to the assembled value, if any

        @return the union rebuilt over the overlaid branches

        @throw REST-SCHEMA-ACTION-ERROR an entry names no branch's field, or conversions differ across branches
    */
    private AbstractDataProviderType applyUnionOverlay(DataProvider::QoreUnionDataType type,
            hash<string, hash<RestSchemaFieldOverlayInfo>> ovl, string ctx, reference<*hash<auto>> node) {
        hash<string, bool> matched = {};
        list<AbstractDataProviderType> branches = ();
        list<*hash<auto>> conversions = ();
        foreach AbstractDataProviderType branch in (type.getUnionTypes()) {
            *hash<string, AbstractDataField> bfields = branch.getFields();
            hash<string, hash<RestSchemaFieldOverlayInfo>> bovl = ovl{keys (bfields ?? {})};
            if (!bovl) {
                branches += branch;
                push conversions, NOTHING;
                continue;
            }
            map matched{$1} = True, keys bovl;
            *hash<auto> bnode;
            branches += applyTypeOverlay(branch, bovl,
                sprintf("%s alternative %y", ctx, branch.getName()), \bnode);
            push conversions, bnode;
        }
        foreach *hash<auto> conversion in (conversions) {
            if (conversion != conversions[0]) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the overlay for %s requires different wire "
                    "conversions across union alternatives; every alternative must use the same conversion",
                    ctx);
            }
        }
        foreach string key in (keys ovl) {
            if (!matched{key}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("overlay entry %y does not name a field of any "
                    "alternative of %s; alternatives: %y", key, ctx,
                    (map $1.getName(), type.getUnionTypes()));
            }
        }
        node = conversions[0];
        if (node) {
            hash<auto> conversion = node;
            return new RestSchemaConvertedUnionDataType(type, branches, conversion);
        }
        return type.copyWithUnionTypes(branches);
    }

    #! Returns the overlay for a derived field, preferring a location-qualified key
    private hash<RestSchemaFieldOverlayInfo> getFieldOverlay(*hash<string, hash<RestSchemaFieldOverlayInfo>> ovl,
            string loc, string derived_name) {
        return ovl{loc + "." + derived_name} ?? ovl{derived_name} ?? <RestSchemaFieldOverlayInfo>{};
    }

    #! Verifies that every overlay entry names a field the action actually has
    private checkOverlayTargets(*hash<string, hash<RestSchemaFieldOverlayInfo>> ovl,
            hash<string, list<hash<auto>>> derived, *list<string> final_names) {
        if (!ovl) {
            return;
        }
        hash<string, bool> valid = {};
        foreach hash<auto> i in (derived.pairIterator()) {
            foreach hash<auto> entry in (i.value) {
                string name = entry.ov_name ?? entry.wire_name;
                valid{name} = True;
                valid{i.key + "." + name} = True;
            }
        }
        foreach string key in (keys ovl) {
            if (!valid{key}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("overlay entry %y does not name any value the "
                    "operation declares; addressable values: %y", key, sort(keys valid));
            }
        }
    }

    #! Publishes an object-valued field's properties as options in place of the field itself
    /** @param field the field the overlay has already been applied to
        @param loc the transport location the value travels in
        @param entry the derived field entry
        @param node the conversion applied to the assembled value, if the overlay gave it a codec or
        overrode any of its properties
        @param ctx a description of the field for error messages
        @param fields the action's fields, added to here
        @param origin the location each field came from, added to here

        @throw REST-SCHEMA-ACTION-ERROR the value is not an object, declares no properties, or one of them
        collides with an option the action already has

        @see @ref RestSchemaActions::RestSchemaFieldOverlayInfo::lift "lift"
    */
    private liftFieldOptions(AbstractDataField field, string loc, hash<auto> entry, *hash<auto> node,
            string ctx, reference<hash<string, AbstractDataField>> fields,
            reference<hash<string, string>> origin) {
        AbstractDataProviderType type = field.getType();
        if (type.getBaseTypeCode() != NT_HASH) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot publish the properties of %s as options: it has "
                "type %y, which is not an object; a codec or a type override can present it as one",
                ctx, type.getName());
        }
        *hash<string, AbstractDataField> tfields = type.getFields();
        if (!tfields) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot publish the properties of %s as options: it "
                "declares none", ctx);
        }
        # a property of a value that may be omitted entirely cannot be required at the action level, for the
        # same reason a property of an optional body cannot be
        bool required = fieldIsRequired(field);
        hash<string, string> opts = {};
        foreach hash<auto> i in (tfields.pairIterator()) {
            string name = i.value.getName();
            if (exists fields{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("option %y is produced by both the %s value of that "
                    "name and property %y of %s; rename one of them", name, origin{name}, i.key, ctx);
            }
            AbstractDataField f = i.value;
            if (!required && fieldIsRequired(f)) {
                f = f.getOrNothingType();
            }
            fields{name} = f;
            origin{name} = loc;
            opts{i.key} = name;
            flat_map{name} = <RestSchemaFlatFieldInfo>{
                "name": name,
                "loc": loc,
                "wire_name": i.key,
                "lift_into": entry.wire_name,
            };
        }
        lifted_values += <RestSchemaLiftedValueInfo>{
            "name": entry.wire_name,
            "loc": loc,
            "wire_name": entry.wire_name,
            "wire_path": entry.wire_path,
            "conv": node,
            "options": opts,
        };
    }

    #! Applies a field overlay to a derived field and returns the field to publish
    private AbstractDataField applyFieldOverlay(AbstractDataField field, hash<RestSchemaFieldOverlayInfo> ov,
            string name, string ctx, reference<*hash<auto>> node) {
        on_error rethrow $1.err, sprintf("%s (while applying the overlay for %s)", $1.desc, ctx);

        bool required = fieldIsRequired(field);
        if (exists ov.required) {
            if (ov.required) {
                required = True;
            } else if (required) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the overlay makes %s optional, but the schema "
                    "declares it required; an action may present a narrower contract than the API, never a "
                    "wider one", ctx);
            }
        }

        # the schema-derived field is cached and shared, so always work on a copy; renaming produces one too
        AbstractDataField base = field.getName() == name && !ov.display_name.val()
            ? field.copy()
            : field.getRenamedField(name, ov.display_name);
        if (!(base instanceof QoreDataField)) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot apply an overlay to %s: field class %y does not "
                "support type replacement", ctx, base.className());
        }
        QoreDataField rv = cast<QoreDataField>(base);

        AbstractDataProviderType type = rv.getType();
        *string codec_desc;
        # a codec or a type override replaces the schema-derived type, which makes the schema's own allowed
        # values and default value descriptions of the *wire* value rather than of what the action presents
        bool type_replaced = False;
        if (ov.codec.val()) {
            hash<RestSchemaWireCodecInfo> codec = RestSchemaActionCodecs::getCodecEx(ov.codec);
            type = codec.type;
            node.codec = codec;
            codec_desc = codec.desc;
            type_replaced = True;
        }
        if (ov.type) {
            type = ov.type;
            type_replaced = True;
        }
        if (ov.fields) {
            *hash<auto> nested;
            type = applyTypeOverlay(type, ov.fields, ctx, \nested);
            if (nested.fields) {
                node.fields = nested.fields;
            }
            if (nested.merge) {
                node.merge = nested.merge;
            }
        }
        # Presence and nullability are separate schema constraints. Preserve a schema-derived
        # nullable type on a required property; only a deliberate narrowing or replacement changes it.
        rv.updateFieldType(!required ? type.getOrNothingType()
            : (type_replaced || (ov.required && !fieldIsRequired(field))) ? type.getMandatoryType() : type);

        # attributes are what DataProviderActionCatalog::getActionOptionFromFields() reads, so requiredness
        # derived from the type has to be published there as well
        hash<auto> attr = {};
        if (required) {
            attr += {
                "required": True,
                "preselected": True,
            };
        }
        if (ov.preselected) {
            attr.preselected = True;
        }
        map attr{$1.key} = $1.value, ov{"sensitive", "required_groups", "exclusive_with", "ref_data",
            "element_ref_data", "depends_on", "has_dependents", "on_change", "validation_regex",
            "ui_type", "supports_custom_values"}.pairIterator(), exists $1.value;
        if (ov.attr) {
            attr += ov.attr;
        }
        if (attr) {
            rv.setAttributes(attr);
        }
        if (type_replaced || ov.short_desc.val() || ov.desc.val() || exists ov.example_value
            || codec_desc.val()) {
            # AbstractDataField has no setters for descriptions or example values, so rebuild the field
            rv = rebuildField(rv, ov, codec_desc, type_replaced);
        }

        # the overlay's own allowed values and default describe what the action presents, so they are set
        # after any rebuild that dropped the schema's, which describe what the wire carries
        if (exists ov.default_value) {
            rv.setDefaultValue(ov.default_value);
        }
        if (exists ov.allowed_values) {
            rv.setAllowedValues(ov.allowed_values, ov.allowed_values_creatable);
        }
        if (exists ov.element_allowed_values) {
            rv.setElementAllowedValues(ov.element_allowed_values, ov.element_allowed_values_creatable);
        }
        return rv;
    }

    #! Applies nested field overrides to a derived type without replacing the type
    private AbstractDataProviderType applyTypeOverlay(AbstractDataProviderType type,
            hash<string, hash<RestSchemaFieldOverlayInfo>> ovl, string ctx, reference<*hash<auto>> node) {
        # a list type carries its shape in its element type
        if (type.getBaseTypeCode() == NT_LIST) {
            *AbstractDataProviderType etype = type.getElementType();
            if (!etype) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot override fields of %s: it is a list with no "
                    "element type", ctx);
            }
            *hash<auto> enode;
            AbstractDataProviderType new_etype = applyTypeOverlay(etype, ovl, ctx + " element", \enode);
            node = enode;
            return new ListDataType(type.getName(), new_etype, !type.isMandatory());
        }

        # a discriminated body is a union of one object per kind, and the field being overridden belongs to
        # whichever branches declare it: a webhook channel's signing secret is a field of the webhook branch
        # and of no other
        if (type instanceof DataProvider::QoreUnionDataType) {
            return applyUnionOverlay(cast<DataProvider::QoreUnionDataType>(type), ovl, ctx, \node);
        }

        *hash<string, AbstractDataField> tfields = type.getFields();
        if (!tfields) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot override fields of %s: type %y has no fields",
                ctx, type.getName());
        }
        if (!(type instanceof HashDataType)) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot override fields of %s: type %y has class %y, "
                "which does not support field replacement", ctx, type.getName(), type.className());
        }
        hash<string, AbstractDataField> new_fields = {};
        hash<string, hash<auto>> conv = {};
        hash<string, bool> renamed = {};
        list<hash<auto>> merged = ();
        foreach hash<auto> i in (tfields.pairIterator()) {
            *hash<RestSchemaFieldOverlayInfo> ov = ovl{i.key};
            if (!ov) {
                new_fields{i.key} = i.value;
                continue;
            }
            if (ov.ignore) {
                continue;
            }
            if (ov.merge) {
                mergeFieldUp(i.value, ov, i.key, ctx, \new_fields, \conv, \merged);
                continue;
            }
            string name = ov.name ?? i.key;
            if (exists new_fields{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("the overlay renames %s field %y to %y, which "
                    "collides with another field of the same type", ctx, i.key, name);
            }
            *hash<auto> fnode;
            new_fields{name} = applyFieldOverlay(i.value, ov, name, sprintf("%s field %y", ctx, i.key), \fnode);
            if (fnode) {
                conv{name} = fnode;
            }
            if (name != i.key) {
                renamed{i.key} = True;
            }
        }
        foreach string key in (keys ovl) {
            if (!tfields{key}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("overlay entry %y does not name a field of %s; "
                    "fields: %y", key, ctx, sort(keys tfields));
            }
        }
        if (renamed) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("the overlay renames nested %s field(s) %y; nested fields "
                "cannot be renamed, because the wire name would be lost", ctx, sort(keys renamed));
        }
        if (conv) {
            node.fields = conv;
        }
        if (merged) {
            node.merge = merged;
        }
        return cast<HashDataType>(type).copyWithFields(new_fields);
    }

    #! Publishes an object-valued field's properties beside it rather than inside it
    /** @param field the field to merge up
        @param ov the overlay for it
        @param key the field's name
        @param ctx a description of the enclosing type for error messages
        @param new_fields the enclosing type's fields, added to here
        @param conv the enclosing type's conversion tree, added to here
        @param merged the merges recorded on the enclosing type, added to here

        @throw REST-SCHEMA-ACTION-ERROR the field is not an object, declares no properties, or one of them
        collides with a property the enclosing object already has

        @see @ref RestSchemaActions::RestSchemaFieldOverlayInfo::merge "merge"
    */
    private mergeFieldUp(AbstractDataField field, hash<RestSchemaFieldOverlayInfo> ov, string key, string ctx,
            reference<hash<string, AbstractDataField>> new_fields, reference<hash<string, hash<auto>>> conv,
            reference<list<hash<auto>>> merged) {
        *hash<auto> fnode;
        AbstractDataField applied = applyFieldOverlay(field, ov - "merge", key,
            sprintf("%s field %y", ctx, key), \fnode);
        AbstractDataProviderType type = applied.getType();
        if (type.getBaseTypeCode() != NT_HASH) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot merge %s field %y into the object that "
                "contains it: it has type %y, which is not an object", ctx, key, type.getName());
        }
        *hash<string, AbstractDataField> mfields = type.getFields();
        if (!mfields) {
            throw "REST-SCHEMA-ACTION-ERROR", sprintf("cannot merge %s field %y into the object that "
                "contains it: it declares no properties", ctx, key);
        }
        foreach hash<auto> i in (mfields.pairIterator()) {
            if (exists new_fields{i.key}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("merging %s field %y into the object that "
                    "contains it would produce two properties named %y", ctx, key, i.key);
            }
            new_fields{i.key} = i.value;
        }
        # the merged properties sit at the enclosing level now, so any conversion declared for one of them
        # is re-keyed there too, and the tree stays a description of the shape it is applied to
        map conv{$1.key} = $1.value, (fnode.fields ?? {}).pairIterator();
        merged += {
            "name": key,
            "fields": keys mfields,
        };
    }

    #! Rebuilds a field with overridden descriptions and example value
    /** @param field the field to rebuild
        @param ov the overlay supplying the overrides
        @param codec_desc a description of any wire conversion, appended to the field description so that the
        difference between the presented value and the wire value is documented where it is read
        @param type_replaced whether the overlay replaces the field type
    */
    private QoreDataField rebuildField(AbstractDataField field, hash<RestSchemaFieldOverlayInfo> ov,
            *string codec_desc, *bool type_replaced) {
        hash<DataFieldInfo> fi = field.getLocalInfo();
        hash<auto!> h = {
            "name": field.getName(),
            "display_name": ov.display_name ?? fi.display_name,
            "type": field.getType(),
            "attr": fi.attr,
        };
        *string desc = ov.desc ?? fi.desc;
        if (codec_desc.val()) {
            desc = (desc.val() ? desc + "\n\n" : "") + "This value is " + codec_desc + ".";
        }
        if (desc.val()) {
            h.desc = desc;
        }
        if (ov.short_desc.val() || fi.short_desc.val()) {
            h.short_desc = ov.short_desc ?? fi.short_desc;
        }
        if (exists ov.example_value) {
            h.example_value = ov.example_value;
        }
        # a value the schema fixed for the wire form cannot be assigned to a type that replaced it, and
        # would be describing something the action no longer presents
        if (!type_replaced) {
            if (exists field.getDefaultValue()) {
                h.default_value = field.getDefaultValue();
            }
            if (fi.allowed_values) {
                h.allowed_values = fi.allowed_values;
                h.allowed_values_creatable = fi.allowed_values_creatable;
            }
            if (fi.element_allowed_values) {
                h.element_allowed_values = fi.element_allowed_values;
                h.element_allowed_values_creatable = fi.element_allowed_values_creatable;
            }
        }
        return new QoreDataField(h);
    }

    #! Returns the request fields in presentation order
    private hash<string, AbstractDataField> orderFields(hash<string, AbstractDataField> fields) {
        if (!info.field_order) {
            return fields;
        }
        hash<string, AbstractDataField> rv = {};
        foreach string name in (info.field_order) {
            if (!exists fields{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("\"field_order\" names option %y, which the action "
                    "does not have; options: %y", name, sort(keys fields));
            }
            if (exists rv{name}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("\"field_order\" names option %y more than once",
                    name);
            }
            rv{name} = fields{name};
        }
        map rv{$1.key} = $1.value, fields.pairIterator(), !exists rv{$1.key};
        return rv;
    }

    #! Returns @ref True if the field is required
    /** Requiredness reaches a derived field two ways: a mandatory type, and a \c "required" attribute set by
        the schema loader for object-valued and composed schemas.
    */
    private static bool fieldIsRequired(AbstractDataField field) {
        return field.getType().isMandatory() || (field.getAttributes().required ?? False);
    }

    #! Converts a value through an immutable operation conversion tree
    /** Used by schema actions and converted union types to retain the same wire contract.
        @param value the value to convert
        @param node the conversion tree constructed from field overlays
        @param to_wire convert to wire form when true, to action form otherwise
        @return the converted value
        @throw REST-SCHEMA-CODEC-ERROR a codec cannot convert the value
    */
    static auto convertValue(auto value, hash<auto> node, bool to_wire) {
        if (!exists value) {
            return value;
        }
        *hash<RestSchemaWireCodecInfo> codec = node.codec.typeCode() == NT_STRING
            ? RestSchemaActionCodecs::getCodecEx(node.codec) : node.codec;
        # a codec replaces the whole value, so nested conversions only make sense on the action-facing side:
        # convert children first when going out, and last when coming back
        if (!to_wire && codec) {
            value = codec.from_wire(value);
        }
        if (node.fields || node.merge) {
            switch (value.typeCode()) {
                case NT_HASH:
                    value = RestSchemaActionOperation::convertObject(value, node, to_wire);
                    break;
                case NT_LIST: {
                    # the overlay describes the element shape, so it applies to every element; the codec
                    # applied to the list itself must not be applied again to each of them
                    hash<auto> enode = node - "codec";
                    list<auto> converted = ();
                    foreach auto element in (value) {
                        converted += RestSchemaActionOperation::convertValue(element, enode, to_wire);
                    }
                    value = converted;
                    break;
                }
            }
        }
        if (to_wire && codec) {
            value = codec.to_wire(value);
        }
        return value;
    }

    #! Converts the properties of an object through a conversion tree in either direction
    /** A merged envelope is opened before the properties inside it are converted, and closed after they
        have been converted back, so the conversion tree always describes the shape it is applied to.
    */
    private static hash<auto> convertObject(hash<auto> value, hash<auto> node, bool to_wire) {
        # "auto!" because the converted values need not share a type with the ones they replace
        hash<auto!> rv = value;
        if (!to_wire && node.merge) {
            rv = RestSchemaActionOperation::openMerged(rv, node.merge);
        }
        foreach hash<auto> i in ((node.fields ?? {}).pairIterator()) {
            if (rv.hasKey(i.key)) {
                rv{i.key} = RestSchemaActionOperation::convertValue(rv{i.key}, i.value, to_wire);
            }
        }
        if (to_wire && node.merge) {
            rv = RestSchemaActionOperation::closeMerged(rv, node.merge);
        }
        return rv;
    }

    #! Lifts the properties of each merged envelope into the object that carries it
    private static hash<auto> openMerged(hash<auto> value, list<hash<auto>> merged) {
        # "auto!" because the merged properties need not share a type with the ones already there
        hash<auto!> rv = value;
        foreach hash<auto> m in (merged) {
            if (!rv.hasKey(m.name)) {
                continue;
            }
            auto inner = remove rv{m.name};
            if (inner.typeCode() != NT_HASH) {
                continue;
            }
            map rv{$1.key} = $1.value, inner.pairIterator();
        }
        return rv;
    }

    #! Puts the properties of each merged envelope back inside it
    /** The inverse of @ref openMerged().  Only the properties the envelope declares are moved back, so a
        value carrying something else keeps it where it is rather than having it swept into the envelope.
    */
    private static hash<auto> closeMerged(hash<auto> value, list<hash<auto>> merged) {
        hash<auto!> rv = value;
        foreach hash<auto> m in (merged) {
            hash<auto!> inner = {};
            foreach string name in (m.fields) {
                if (rv.hasKey(name)) {
                    inner{name} = remove rv{name};
                }
            }
            if (inner) {
                rv{m.name} = inner;
            }
        }
        return rv;
    }
}
}
