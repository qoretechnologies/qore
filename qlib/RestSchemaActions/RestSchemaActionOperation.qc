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
        const LocationOrder = (RSAL_PATH, RSAL_BODY, RSAL_BODY_ROOT, RSAL_QUERY, RSAL_HEADER);
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

        #! Header values dropped by the overlay, which the connection is expected to supply
        /** An ignored header is one the connection owns - an API key, the tenant a Xero connection works
            in - so the action must not ask for it, and the request must still carry it.  These names are
            filled in from the REST client's own default headers when the request is built.
        */
        list<string> connection_headers = ();

        #! The action output type: the success response body, unwrapped
        *AbstractDataProviderType response_type;

        #! The reverse map from action option name to transport location
        hash<string, hash<RestSchemaFlatFieldInfo>> flat_map = {};

        #! Request value conversion tree, keyed by action option name
        hash<string, hash<auto>> request_conv = {};

        #! Response value conversion tree, keyed by response body field name
        hash<string, hash<auto>> response_conv = {};

        #! The HTTP status code of the response published as the output type
        *string response_code;

        #! Path variable names in the order they appear in the URI path template
        list<string> path_var_names = ();
    }

    #! Resolves the manifest entry against the schema and derives the action's types
    /** @param schema the schema owning the operation
        @param info the manifest entry

        @throw REST-SCHEMA-ACTION-ERROR the operation cannot be resolved, the overlay does not apply, or the
        output contract is ambiguous
    */
    constructor(AbstractRestSchemaValidator schema, hash<RestSchemaActionInfo> info) {
        self.schema = schema;
        self.info = info;
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
    list<string> getConnectionHeaders() {
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
        foreach hash<auto> i in (options.pairIterator()) {
            *hash<RestSchemaFlatFieldInfo> fi = flat_map{i.key};
            if (!fi) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("action %y has no option %y; known options: %y",
                    info.action, i.key, sort(keys flat_map));
            }
            auto value = i.value;
            if (*hash<auto> node = request_conv{i.key}) {
                value = RestSchemaActionOperation::convertValue(value, node, True);
            }
            switch (fi.loc) {
                case RSAL_PATH:
                    path_vars{fi.wire_name} = value;
                    break;
                case RSAL_QUERY:
                    req.query{fi.wire_name} = value;
                    break;
                case RSAL_HEADER:
                    req.header{fi.wire_name} = value;
                    break;
                case RSAL_BODY:
                    req.body{fi.wire_name} = value;
                    break;
                case RSAL_BODY_ROOT:
                    req.body = value;
                    break;
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
                case RSAL_BODY: value = req.body{src.wire_name}; break;
                case RSAL_BODY_ROOT: value = req.body; break;
            }
            if (!exists value) {
                continue;
            }
            switch (dv.loc) {
                case RSAL_PATH: path_vars{dv.wire_name} = value; break;
                case RSAL_QUERY: req.query{dv.wire_name} = value; break;
                case RSAL_HEADER: req.header{dv.wire_name} = value; break;
                case RSAL_BODY: req.body{dv.wire_name} = value; break;
            }
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
        # index the reverse map by location and wire name
        hash<string, hash<string, string>> rmap = {};
        map rmap{$1.loc}{$1.wire_name} = $1.name, flat_map.iterator();

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

        map add(RSAL_PATH, $1.key, $1.value), wire.path_vars.pairIterator();
        map add(RSAL_QUERY, $1.key, $1.value), wire.req.query.pairIterator();
        map add(RSAL_HEADER, $1.key, $1.value), wire.req.header.pairIterator();
        if (exists wire.req.body) {
            if (rmap{RSAL_BODY_ROOT}) {
                add(RSAL_BODY_ROOT, "body", wire.req.body);
            } else {
                map add(RSAL_BODY, $1.key, $1.value), wire.req.body.pairIterator();
            }
        }
        return rv;
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
            body = RestSchemaActionOperation::convertValue(body, {"fields": response_conv}, False);
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
        foreach hash<auto> i in (params.pairIterator()) {
            OpenApi3::ParameterBase param = i.value;
            switch (param.inLoc) {
                case RSAL_PATH:
                case RSAL_QUERY:
                case RSAL_HEADER:
                    derived{param.inLoc} += {
                        "wire_name": param.name,
                        # a path variable is part of the URI and is therefore always required
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
                string derived_name = entry.wire_name;
                hash<RestSchemaFieldOverlayInfo> ov = getFieldOverlay(info.fields, loc, derived_name);
                if (ov.ignore) {
                    # a header the action does not ask for is one the connection supplies; the schema may
                    # still require it, and dropping it from the action must not produce a request the
                    # schema rejects
                    if (loc == RSAL_HEADER) {
                        connection_headers += derived_name;
                    }
                    continue;
                }
                if (ov.value_from.val()) {
                    # not an option: the value follows another one and is filled in when the request is
                    # built
                    derived_values += <RestSchemaDerivedValueInfo>{
                        "source": ov.value_from,
                        "loc": loc,
                        "wire_name": derived_name,
                    };
                    continue;
                }
                string name = ov.name ?? derived_name;
                if (exists fields{name}) {
                    throw "REST-SCHEMA-ACTION-ERROR", sprintf("option %y is produced by both the %s and the %s "
                        "value named %y; rename one of them with a location-qualified overlay key "
                        "(ex: \"%s.%s\") giving a \"name\"", name, origin{name}, loc, derived_name, loc,
                        derived_name);
                }
                *hash<auto> node;
                fields{name} = applyFieldOverlay(entry.field, ov, name,
                    sprintf("%s value %y", loc, derived_name), \node);
                if (node) {
                    request_conv{name} = node;
                }
                origin{name} = loc;
                flat_map{name} = <RestSchemaFlatFieldInfo>{
                    "name": name,
                    "loc": loc,
                    "wire_name": loc == RSAL_BODY_ROOT ? "body" : derived_name,
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

    #! Adds the request body's contribution to the derived field map
    private addBodyFields(reference<hash<string, list<hash<auto>>>> derived) {
        *OpenApi3::ParameterObject body = op.body ?? pio.body;
        if (!body || !body.schema) {
            return;
        }
        bool body_required = body.required ?? False;
        AbstractDataField body_field = schema.getFieldFromSchema("body", "REST request body argument",
            body.schema, body_required);
        AbstractDataProviderType body_type = body_field.getType();

        # an object body is flattened to its properties; anything else -- an array or a scalar -- has no
        # properties to lift and is presented whole.  The distinction is the base type, not whether any
        # fields came back: an object body declaring no properties contributes no options at all, and
        # presenting an empty envelope as a "body" option would be worse than useless.  Some schemas
        # declare exactly that on operations that take no input.
        if (!info.flatten_body || body_type.getBaseTypeCode() != NT_HASH) {
            derived{RSAL_BODY_ROOT} += {
                "wire_name": "body",
                "field": body_field,
            };
            return;
        }
        *hash<string, AbstractDataField> body_fields = body_type.getFields();
        if (!body_fields) {
            return;
        }
        foreach hash<auto> i in (body_fields.pairIterator()) {
            AbstractDataField f = i.value;
            if (!body_required && fieldIsRequired(f)) {
                # omitting the body entirely is valid, so a property of it cannot be required at the action
                # level; a partially supplied body is still rejected by the schema's request validation
                f = f.getOrNothingType();
            }
            derived{RSAL_BODY} += {
                "wire_name": i.key,
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
            if (!resp.getSchema()) {
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
            # no success response with a body: the action has no output type
            return;
        }

        AbstractDataField body_field = schema.getFieldFromSchema("body", "REST response body argument",
            resp.getSchema());
        AbstractDataProviderType type = body_field.getType();

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
            if (node.fields) {
                response_conv = node.fields;
            }
        }
        response_type = type;
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
                valid{entry.wire_name} = True;
                valid{i.key + "." + entry.wire_name} = True;
            }
        }
        foreach string key in (keys ovl) {
            if (!valid{key}) {
                throw "REST-SCHEMA-ACTION-ERROR", sprintf("overlay entry %y does not name any value the "
                    "operation declares; addressable values: %y", key, sort(keys valid));
            }
        }
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
        if (ov.codec.val()) {
            hash<RestSchemaWireCodecInfo> codec = RestSchemaActionCodecs::getCodecEx(ov.codec);
            type = codec.type;
            node.codec = codec;
            codec_desc = codec.desc;
        }
        if (ov.type) {
            type = ov.type;
        }
        if (ov.fields) {
            *hash<auto> nested;
            type = applyTypeOverlay(type, ov.fields, ctx, \nested);
            if (nested.fields) {
                node.fields = nested.fields;
            }
        }
        rv.updateFieldType(required ? type.getMandatoryType() : type.getOrNothingType());

        if (exists ov.default_value) {
            rv.setDefaultValue(ov.default_value);
        }
        if (exists ov.allowed_values) {
            rv.setAllowedValues(ov.allowed_values, ov.allowed_values_creatable);
        }

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
        if (ov.short_desc.val() || ov.desc.val() || exists ov.example_value || codec_desc.val()) {
            # AbstractDataField has no setters for descriptions or example values, so rebuild the field
            rv = rebuildField(rv, ov, codec_desc);
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
        foreach hash<auto> i in (tfields.pairIterator()) {
            *hash<RestSchemaFieldOverlayInfo> ov = ovl{i.key};
            if (!ov) {
                new_fields{i.key} = i.value;
                continue;
            }
            if (ov.ignore) {
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
        return cast<HashDataType>(type).copyWithFields(new_fields);
    }

    #! Rebuilds a field with overridden descriptions and example value
    /** @param field the field to rebuild
        @param ov the overlay supplying the overrides
        @param codec_desc a description of any wire conversion, appended to the field description so that the
        difference between the presented value and the wire value is documented where it is read
    */
    private QoreDataField rebuildField(AbstractDataField field, hash<RestSchemaFieldOverlayInfo> ov,
            *string codec_desc) {
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

    #! Converts a value through a conversion tree in either direction
    private static auto convertValue(auto value, hash<auto> node, bool to_wire) {
        if (!exists value) {
            return value;
        }
        # a codec replaces the whole value, so nested conversions only make sense on the action-facing side:
        # convert children first when going out, and last when coming back
        if (!to_wire && node.codec) {
            value = node.codec.from_wire(value);
        }
        if (node.fields) {
            switch (value.typeCode()) {
                case NT_HASH: {
                    foreach hash<auto> i in (node.fields.pairIterator()) {
                        if (value.hasKey(i.key)) {
                            value{i.key} = RestSchemaActionOperation::convertValue(value{i.key}, i.value,
                                to_wire);
                        }
                    }
                    break;
                }
                case NT_LIST: {
                    # the overlay describes the element shape, so it applies to every element
                    list<auto> converted = ();
                    foreach auto element in (value) {
                        converted += RestSchemaActionOperation::convertValue(element, {"fields": node.fields},
                            to_wire);
                    }
                    value = converted;
                    break;
                }
            }
        }
        if (to_wire && node.codec) {
            value = node.codec.to_wire(value);
        }
        return value;
    }
}
}
