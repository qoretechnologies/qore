# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaActionDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! Executes one schema-derived action
/** Presents the flattened request type built by
    @ref RestSchemaActions::RestSchemaActionOperation "RestSchemaActionOperation", rebuilds the transport
    request from the options it is given, and publishes the success response body - not the HTTP envelope -
    as its response.
*/
public class RestSchemaActionDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "RestSchemaActionDataProvider",
            "supports_request": True,
            "supports_children": False,
        };
    }

    private {
        #! The action set this action belongs to
        RestSchemaActionSet action_set;

        #! The operation executed by this provider
        RestSchemaActionOperation op;

        #! The REST client used for API calls
        *AbstractRestClient rest;

        #! The underlying schema request provider
        /** Built in the constructor rather than on first use, so that the object holds no mutable state and
            can be shared between threads without synchronization
        */
        RestSchemaRequestDataProvider req_dp;
    }

    #! Creates the object from the arguments
    /** @param action_set the action set the action belongs to
        @param op the operation to execute
        @param rest the REST client to use for API calls
    */
    constructor(RestSchemaActionSet action_set, RestSchemaActionOperation op, *AbstractRestClient rest) {
        self.action_set = action_set;
        self.op = op;
        self.rest = rest;
        req_dp = new RestSchemaRequestDataProvider(action_set.getSchema(), rest, op.getPath(),
            op.getPathItem(), op.getOperation());
    }

    #! Returns the action ID
    string getName() {
        return op.getAction();
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return op.getInfo().display_name;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return op.getInfo().short_desc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return op.getInfo().desc;
    }

    #! Returns the operation executed by this provider
    RestSchemaActionOperation getActionOperation() {
        return op;
    }

    #! Returns the flattened request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return op.getRequestType();
    }

    #! Returns the action output type: the success response body
    private *AbstractDataProviderType getResponseTypeImpl() {
        return op.getResponseType();
    }

    #! Makes the API call, holding back any value that is already in wire form from the type check
    /** A @ref RestSchemaActions::RestSchemaRawValue "RestSchemaRawValue" carries a value the caller has
        already rendered into what the API takes - a record table's filter expression - and the option's
        published type describes the **curated** shape the action offers instead, so validating one against
        the other would reject exactly the value that is correct.

        Every other value is checked as before, and the wire-form values are put back afterwards so that
        @ref RestSchemaActions::RestSchemaActionOperation::unflatten() "unflatten()" places them without
        applying the option's codec.

        @param req the request options
        @param request_options the request options for the transport

        @return the response body
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH) {
            return AbstractDataProvider::doRequest(req, request_options);
        }
        hash<auto> opts = req;
        # "auto!" because the wire-form values are of every type
        hash<auto!> raw = {};
        map raw{$1.key} = $1.value, opts.pairIterator(), $1.value instanceof hash<RestSchemaRawValue>;
        if (!raw) {
            return AbstractDataProvider::doRequest(req, request_options);
        }
        checkRequest();
        auto checked = opts - (keys raw);
        *AbstractDataProviderType request_type = getRequestTypeWithData(checked);
        if (request_type) {
            try {
                checked = request_type.acceptsValue(checked);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "INVALID-REQUEST", sprintf("invalid request argument for request %y: %s: %s",
                    getName(), ex.err, ex.desc), getWrappedExceptionArg(ex);
            }
        }
        return doRequestImpl(checked + raw, validateRequestOptions(request_options));
    }

    #! Makes the API call and returns the response body
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> wire = op.unflatten(req);
        wire.req = addConnectionHeaders(wire.req);
        # path variables travel as a request option, not as part of the request body
        *hash<auto> resp = req_dp.doRequest(wire.req, request_options + {
            "path_vars": wire.path_vars,
        });
        return op.processResponse(resp.body);
    }

    #! Fills in the headers the connection owns
    /** A header the overlay dropped with
        @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ignore "ignore" is one the connection supplies,
        not one the action asks for - an API key, or the tenant a Xero connection works in.  The schema may
        still declare it required, and the client adds its own default headers only when the request is
        actually sent, so a request validated against the schema in between would be rejected for a value
        the action was never supposed to carry.

        Taking it from the client's default headers is what makes \c ignore mean what it says.  An
        optional header the client does not supply is left out; a **required** one is reported here,
        naming the connection option that supplies it, because the alternative is a schema type error
        about a value the user never saw and could not have set on the action.

        @param wire_req the transport request

        @return the request with any connection-owned headers filled in

        @throw REST-CONNECTION-OPTION-ERROR the connection does not supply a header the schema requires
    */
    private hash<auto> addConnectionHeaders(hash<auto> wire_req) {
        list<hash<RestSchemaConnectionHeaderInfo>> conn_headers = op.getConnectionHeaders();
        if (!conn_headers || !rest) {
            return wire_req;
        }
        # HTTP header names are case-insensitive, and a schema and a client need not spell one the same way
        hash<string, auto> lower = {};
        map lower{$1.key.lwr()} = $1.value, (rest.getDefaultHeaders() ?? {}).pairIterator();
        # "auto!" because the header values are not all of one type once a caller's own are merged in
        hash<auto!> hdr = wire_req.header ?? {};
        foreach hash<RestSchemaConnectionHeaderInfo> ch in (conn_headers) {
            if (exists hdr{ch.wire_name}) {
                continue;
            }
            auto value = lower{ch.wire_name.lwr()};
            if (exists value) {
                hdr{ch.wire_name} = value;
                continue;
            }
            if (ch.required) {
                throw "REST-CONNECTION-OPTION-ERROR", sprintf("action %y requires the %y header, which the "
                    "connection supplies rather than the action, and this connection does not send it%s",
                    op.getAction(), ch.wire_name, ch.connection_option.val()
                        ? sprintf("; set the %y option on the connection", ch.connection_option)
                        : "");
            }
        }
        if (hdr) {
            wire_req.header = hdr;
        }
        return wire_req;
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        hash<string, bool> rv = {};
        map rv{$1} = True, getReferenceDataNames();
        return rv ?: NOTHING;
    }

    #! Populates a dropdown by executing the action that lists its values
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        # an application split across documents declares a dropdown once, in the set whose schema describes
        # the listing, and other sets reach it as a peer
        *hash<RestSchemaReferenceDataInfo> rd = action_set.getReferenceDataDecl(type);
        *RestSchemaActionSet owner = action_set.getReferenceDataSet(type);
        if (!rd || !owner) {
            return;
        }
        on_error rethrow $1.err, sprintf("%s (while retrieving reference data %y for action %y via action %y)",
            $1.desc, type, getName(), rd.action);

        # a dropdown scoped by another option -- the branches of the selected project -- cannot be listed
        # until that option has a value.  An empty list is the honest answer then: listing everything would
        # offer values that do not belong to the chosen scope
        # "auto!" because the fixed options are typically all of one type -- a page size, say -- and a
        # narrowed hash would reject the forwarded value
        hash<auto!> opts = rd.options ?? {};
        foreach hash<auto> i in (rd.options_from.pairIterator()) {
            auto val = action_opts{i.value};
            if (!exists val) {
                return;
            }
            opts{i.key} = val;
        }

        # the listing runs against the schema that declares it, because the request URI takes its prefix
        # from that schema's base path
        RestSchemaActionDataProvider lister = new RestSchemaActionDataProvider(owner,
            owner.getOperationEx(rd.action), rest);
        # a collection an API will not list whole is read once per partition; see
        # RestSchemaReferenceDataInfo::option_sets
        list<auto> records = ();
        foreach hash<auto> variant in (rd.option_sets ?* ({},)) {
            auto resp = lister.doRequest(opts + variant);
            foreach string name in (rd.records_path) {
                if (!exists resp) {
                    break;
                }
                if (resp.typeCode() != NT_HASH) {
                    throw "REFERENCE-DATA-ERROR", sprintf("cannot follow records path %y: the value at %y has "
                        "type %y, not a hash", rd.records_path, name, resp.type());
                }
                resp = resp{name};
            }
            if (!exists resp) {
                continue;
            }
            if (resp.typeCode() != NT_LIST) {
                throw "REFERENCE-DATA-ERROR", sprintf("expected a list of records, but got type %y (records "
                    "path: %y)", resp.type(), rd.records_path);
            }
            records += resp;
        }
        if (!records) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        # a listing often carries the same value on many records -- the owner of each of a user's
        # repositories, say -- and two dropdown entries with the same value are indistinguishable to
        # whoever is choosing one, so the first occurrence wins and the rest are dropped
        hash<string, bool> seen = {};
        foreach auto record in (records) {
            if (record.typeCode() != NT_HASH) {
                throw "REFERENCE-DATA-ERROR", sprintf("expected every record to be a hash, but element %d has "
                    "type %y", $#, record.type());
            }
            auto value = RestSchemaActionDataProvider::getRecordValue(record, rd.value_field);
            if (!exists value) {
                continue;
            }
            string key = sprintf("%y", value);
            if (seen{key}) {
                continue;
            }
            seen{key} = True;
            # many APIs allow an object's human-readable name to be unset; showing the ID is better than
            # showing an empty row.  The test is whether the name has a value, not whether the field is
            # present: an API that answers "" for an unset name is as common as one that omits it, and
            # Intercom does exactly that for an untitled conversation - a dropdown of blank rows is no more
            # usable than a dropdown of missing ones
            auto display = rd.display_field.val()
                ? RestSchemaActionDataProvider::getRecordValue(record, rd.display_field)
                : NOTHING;
            rv += <AllowedValueInfo>{
                "value": value,
                "display_name": (display.val() ? display : value).toString(),
                "desc": rd.desc,
            };
        }
        return rv ?: NOTHING;
    }

    #! Returns a record value named by a possibly-nested field path
    /** @param record the record
        @param path the field name, or a \c "." separated path into a nested record

        @return the value, or @ref nothing if the path does not resolve

        @note A path that runs into a non-hash returns @ref nothing rather than throwing: a record that
        does not carry the field is skipped, which is the same treatment as one that carries it unset.
    */
    private static auto getRecordValue(hash<auto> record, string path) {
        if (path.find(".") == -1) {
            return record{path};
        }
        auto node = record;
        foreach string name in (split(".", path)) {
            if (node.typeCode() != NT_HASH) {
                return;
            }
            node = node{name};
        }
        return node;
    }

    #! Returns the names of the reference data referenced by this action's options
    private list<string> getReferenceDataNames() {
        hash<string, bool> rv = {};
        *hash<string, AbstractDataField> fields = op.getRequestFields();
        foreach hash<auto> i in (fields.pairIterator()) {
            *hash<auto> attr = i.value.getAttributes();
            map rv{$1} = True, (attr.ref_data, attr.element_ref_data), $1.val();
        }
        return keys rv;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
