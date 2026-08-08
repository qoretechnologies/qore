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
    request from the options it is given, and publishes the success response body — not the HTTP envelope —
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

    #! Makes the API call and returns the response body
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> wire = op.unflatten(req);
        # path variables travel as a request option, not as part of the request body
        *hash<auto> resp = req_dp.doRequest(wire.req, request_options + {
            "path_vars": wire.path_vars,
        });
        return op.processResponse(resp.body);
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        hash<string, bool> rv = {};
        map rv{$1} = True, getReferenceDataNames();
        return rv ?: NOTHING;
    }

    #! Populates a dropdown by executing the action that lists its values
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        *hash<RestSchemaReferenceDataInfo> rd = action_set.getReferenceDataDecls(){type};
        if (!rd) {
            return;
        }
        on_error rethrow $1.err, sprintf("%s (while retrieving reference data %y for action %y via action %y)",
            $1.desc, type, getName(), rd.action);

        RestSchemaActionDataProvider lister = new RestSchemaActionDataProvider(action_set,
            action_set.getOperationEx(rd.action), rest);
        auto resp = lister.doRequest(rd.options);
        foreach string name in (rd.records_path) {
            if (!exists resp) {
                break;
            }
            if (resp.typeCode() != NT_HASH) {
                throw "REFERENCE-DATA-ERROR", sprintf("cannot follow records path %y: the value at %y has type "
                    "%y, not a hash", rd.records_path, name, resp.type());
            }
            resp = resp{name};
        }
        if (!exists resp) {
            return;
        }
        if (resp.typeCode() != NT_LIST) {
            throw "REFERENCE-DATA-ERROR", sprintf("expected a list of records, but got type %y (records path: "
                "%y)", resp.type(), rd.records_path);
        }
        list<hash<AllowedValueInfo>> rv = ();
        foreach auto record in (resp) {
            if (record.typeCode() != NT_HASH) {
                throw "REFERENCE-DATA-ERROR", sprintf("expected every record to be a hash, but element %d has "
                    "type %y", $#, record.type());
            }
            if (!exists record{rd.value_field}) {
                continue;
            }
            # many APIs allow an object's human-readable name to be unset; showing the ID is better than
            # showing an empty row
            auto display = rd.display_field.val() ? record{rd.display_field} : NOTHING;
            rv += <AllowedValueInfo>{
                "value": record{rd.value_field},
                "display_name": (display ?? record{rd.value_field}).toString(),
                "desc": rd.desc,
            };
        }
        return rv ?: NOTHING;
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
