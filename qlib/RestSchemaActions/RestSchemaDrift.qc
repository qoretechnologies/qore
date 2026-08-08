# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaDrift.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
/** @defgroup RestSchemaDriftKinds REST Schema Drift Kinds
    Classifies one difference between the schema an application was built against and a candidate
    replacement.  The kind is what decides whether an update can be adopted or has to stop for review.
*/
#/@{
#! An exported operation is no longer declared by the schema
public const RSD_OPERATION_REMOVED = "operation-removed";

#! An exported operation's \c operationId changed
/** The operation is still there, but something about it was renamed upstream.  This never renames an
    action — action identity is owned by the manifest — but it is worth knowing about, because a rename
    often accompanies a change in meaning.
*/
public const RSD_OPERATION_RENAMED = "operation-renamed";

#! The schema declares an operation the manifest does not export
/** Reported only for paths the application already exports, so that a new method on a familiar resource
    is noticed; a new endpoint elsewhere is not drift, it is simply out of scope.
*/
public const RSD_OPERATION_ADDED = "operation-added";

#! An option the application exports no longer exists
public const RSD_OPTION_REMOVED = "option-removed";

#! The schema declares an option the action did not previously have
public const RSD_OPTION_ADDED = "option-added";

#! An option moved to a different transport location
public const RSD_OPTION_MOVED = "option-moved";

#! An option's type changed
public const RSD_OPTION_TYPE_CHANGED = "option-type-changed";

#! An option became required, or stopped being required
public const RSD_OPTION_REQUIREDNESS_CHANGED = "option-requiredness-changed";

#! An option's allowed values changed
public const RSD_OPTION_VALUES_CHANGED = "option-values-changed";

#! The request body content type changed
public const RSD_CONTENT_TYPE_CHANGED = "content-type-changed";

#! The response published as the action's output changed
public const RSD_RESPONSE_CHANGED = "response-changed";

#! Only documentation changed
public const RSD_DOCUMENTATION_CHANGED = "documentation-changed";

#! An overlay entry no longer names anything the operation declares
public const RSD_OVERLAY_ORPHANED = "overlay-orphaned";

#! Drift kinds that are safe to adopt without changing the manifest
/** An addition or a documentation change cannot invalidate an action that already works; everything else
    changes a contract somebody may be relying on.
*/
public const RestSchemaCompatibleDrift = {
    RSD_OPERATION_ADDED: True,
    RSD_OPTION_ADDED: True,
    RSD_DOCUMENTATION_CHANGED: True,
};
#/@}

#! One difference between two schemas, as it affects an exported action
public hashdecl RestSchemaDriftInfo {
    #! What kind of difference this is
    /** @see @ref RestSchemaDriftKinds
    */
    string kind;

    #! The action affected
    string action;

    #! The option affected, if the difference is about one
    *string option;

    #! A description of the difference, suitable for a review report
    string desc;

    #! The value in the schema the application was built against
    auto old_value;

    #! The value in the candidate schema
    auto new_value;

    #! Whether this difference can be adopted without changing the manifest
    bool compatible;
}

#! Compares an action set against a candidate schema and reports what changed
/** "Staying aligned with an API" has to mean a reproducible reviewed update, not an uncontrolled runtime
    one.  This class produces the report that makes such a review possible: given the action set an
    application ships and a newer schema, it says exactly what would change about every exported action.

    Compatible additions can then be adopted with little effort, while a breaking change stops the update
    until somebody has looked at it.

    @par Example:
    @code{.py}
list<hash<RestSchemaDriftInfo>> drift = RestSchemaDrift::compare(MyApp::ActionSet, new_schema);
if (*list<hash<RestSchemaDriftInfo>> breaking = RestSchemaDrift::getBreaking(drift)) {
    printf("%s\n", RestSchemaDrift::getReport(breaking));
    exit(1);
}
    @endcode
*/
public class RestSchemaDrift {
    #! Compares an action set against a candidate schema
    /** @param action_set the action set the application ships
        @param new_schema the candidate schema

        @return every difference affecting an exported action, in manifest order

        @note An operation that fails to resolve at all against the candidate schema is reported as
        @ref RSD_OPERATION_REMOVED "removed" rather than throwing, so that one broken operation does not
        hide the rest of the report.
    */
    static list<hash<RestSchemaDriftInfo>> compare(RestSchemaActionSet action_set,
            AbstractRestSchemaValidator new_schema) {
        list<hash<RestSchemaDriftInfo>> rv = ();
        foreach string action in (action_set.getActionNames()) {
            RestSchemaActionOperation old_op = action_set.getOperationEx(action);
            hash<RestSchemaActionInfo> info = old_op.getInfo();

            RestSchemaActionOperation new_op;
            try {
                new_op = new RestSchemaActionOperation(new_schema, info);
            } catch (hash<ExceptionInfo> ex) {
                # an operationId change is drift in its own right, not a failure to resolve, so it is
                # retried without the check to see whether the operation is otherwise still there
                if (info.operation_id.val()) {
                    hash<RestSchemaActionInfo> relaxed = info;
                    remove relaxed.operation_id;
                    try {
                        new_op = new RestSchemaActionOperation(new_schema, relaxed);
                        rv += <RestSchemaDriftInfo>{
                            "kind": RSD_OPERATION_RENAMED,
                            "action": action,
                            "desc": sprintf("the schema now declares operationId %y for \"%s %s\"; the "
                                "manifest expects %y", new_op.getOperation().operationId, info.method.upr(),
                                info.path, info.operation_id),
                            "old_value": info.operation_id,
                            "new_value": new_op.getOperation().operationId,
                            "compatible": False,
                        };
                    } catch (hash<ExceptionInfo> ex2) {
                        rv += RestSchemaDrift::removed(action, info, ex2.desc);
                        continue;
                    }
                } else {
                    rv += RestSchemaDrift::removed(action, info, ex.desc);
                    continue;
                }
            }
            rv += RestSchemaDrift::compareOperation(action, old_op, new_op);
        }
        return rv;
    }

    #! Returns only the differences that cannot be adopted without review
    static *list<hash<RestSchemaDriftInfo>> getBreaking(list<hash<RestSchemaDriftInfo>> drift) {
        return (map $1, drift, !$1.compatible) ?: NOTHING;
    }

    #! Returns a human-readable report of the given differences
    /** @param drift the differences to report

        @return a report suitable for printing during a schema update
    */
    static string getReport(*list<hash<RestSchemaDriftInfo>> drift) {
        if (!drift) {
            return "no drift detected\n";
        }
        # grouped by action, because that is the unit somebody reviews
        hash<string, list<hash<RestSchemaDriftInfo>>> by_action = {};
        map by_action{$1.action} += $1, drift;

        string rv = sprintf("%d difference(s) affecting %d action(s):\n", drift.size(), by_action.size());
        foreach hash<auto> i in (by_action.pairIterator()) {
            rv += sprintf("\n%s:\n", i.key);
            foreach hash<RestSchemaDriftInfo> d in (i.value) {
                rv += sprintf("  [%s] %s%s\n", d.compatible ? "ok" : "BREAKING", d.option.val()
                    ? d.option + ": "
                    : "", d.desc);
            }
        }
        return rv;
    }

    #! Returns a drift entry for an operation that no longer resolves
    private static hash<RestSchemaDriftInfo> removed(string action, hash<RestSchemaActionInfo> info,
            string reason) {
        return <RestSchemaDriftInfo>{
            "kind": RSD_OPERATION_REMOVED,
            "action": action,
            "desc": sprintf("\"%s %s\" no longer resolves: %s", info.method.upr(), info.path, reason),
            "old_value": sprintf("%s %s", info.method.upr(), info.path),
            "compatible": False,
        };
    }

    #! Compares one operation resolved against both schemas
    private static list<hash<RestSchemaDriftInfo>> compareOperation(string action,
            RestSchemaActionOperation old_op, RestSchemaActionOperation new_op) {
        list<hash<RestSchemaDriftInfo>> rv = ();

        *hash<string, AbstractDataField> old_fields = old_op.getRequestFields();
        *hash<string, AbstractDataField> new_fields = new_op.getRequestFields();
        hash<string, hash<RestSchemaFlatFieldInfo>> old_flat = old_op.getFlatMap();
        hash<string, hash<RestSchemaFlatFieldInfo>> new_flat = new_op.getFlatMap();

        foreach hash<auto> i in (old_fields.pairIterator()) {
            *AbstractDataField nf = new_fields{i.key};
            if (!nf) {
                rv += <RestSchemaDriftInfo>{
                    "kind": RSD_OPTION_REMOVED,
                    "action": action,
                    "option": i.key,
                    "desc": sprintf("option %y is no longer declared", i.key),
                    "old_value": i.value.getType().getName(),
                    "compatible": False,
                };
                continue;
            }
            rv += RestSchemaDrift::compareField(action, i.key, i.value, nf, old_flat{i.key}, new_flat{i.key});
        }
        foreach string name in (keys new_fields) {
            if (!exists old_fields{name}) {
                rv += <RestSchemaDriftInfo>{
                    "kind": RSD_OPTION_ADDED,
                    "action": action,
                    "option": name,
                    "desc": sprintf("option %y is new", name),
                    "new_value": new_fields{name}.getType().getName(),
                    "compatible": True,
                };
            }
        }

        # an overlay entry that no longer resolves would silently drop a deliberate presentation choice;
        # building the operation against the new schema already refuses that, so reaching here means every
        # entry still resolves -- except where the overlay names a field that is now gone, which the
        # option-removed entries above report
        rv += RestSchemaDrift::compareRequestContentTypes(action, old_op, new_op);
        rv += RestSchemaDrift::compareResponses(action, old_op, new_op);
        return rv;
    }

    #! Compares one option across schemas
    private static list<hash<RestSchemaDriftInfo>> compareField(string action, string name,
            AbstractDataField old_field, AbstractDataField new_field, *hash<RestSchemaFlatFieldInfo> old_loc,
            *hash<RestSchemaFlatFieldInfo> new_loc) {
        list<hash<RestSchemaDriftInfo>> rv = ();

        if (old_loc.loc != new_loc.loc || old_loc.wire_name != new_loc.wire_name) {
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_OPTION_MOVED,
                "action": action,
                "option": name,
                "desc": sprintf("option %y moved from %s %y to %s %y", name, old_loc.loc, old_loc.wire_name,
                    new_loc.loc, new_loc.wire_name),
                "old_value": sprintf("%s:%s", old_loc.loc, old_loc.wire_name),
                "new_value": sprintf("%s:%s", new_loc.loc, new_loc.wire_name),
                "compatible": False,
            };
        }

        string old_type = old_field.getType().getName();
        string new_type = new_field.getType().getName();
        if (old_type != new_type) {
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_OPTION_TYPE_CHANGED,
                "action": action,
                "option": name,
                "desc": sprintf("option %y changed type from %y to %y", name, old_type, new_type),
                "old_value": old_type,
                "new_value": new_type,
                "compatible": False,
            };
        }

        bool old_req = old_field.getType().isMandatory() || (old_field.getAttributes().required ?? False);
        bool new_req = new_field.getType().isMandatory() || (new_field.getAttributes().required ?? False);
        if (old_req != new_req) {
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_OPTION_REQUIREDNESS_CHANGED,
                "action": action,
                "option": name,
                "desc": sprintf("option %y is now %s", name, new_req ? "required" : "optional"),
                "old_value": old_req,
                "new_value": new_req,
                # a newly required option breaks callers that omit it; relaxing one does not
                "compatible": !new_req,
            };
        }

        *list<auto> old_values = map $1.value, old_field.getAllowedValues();
        *list<auto> new_values = map $1.value, new_field.getAllowedValues();
        if (old_values != new_values) {
            *list<auto> removed = map $1, old_values, !(new_values ?? ()).contains($1);
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_OPTION_VALUES_CHANGED,
                "action": action,
                "option": name,
                "desc": sprintf("option %y allowed values changed from %y to %y", name, old_values,
                    new_values),
                "old_value": old_values,
                "new_value": new_values,
                # gaining a value is compatible; losing one rejects requests that used to work
                "compatible": !removed,
            };
        }

        if (old_field.getDescription() != new_field.getDescription()) {
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_DOCUMENTATION_CHANGED,
                "action": action,
                "option": name,
                "desc": sprintf("option %y documentation changed", name),
                "compatible": True,
            };
        }
        return rv;
    }

    #! Compares the request content types accepted by an operation
    private static list<hash<RestSchemaDriftInfo>> compareRequestContentTypes(string action,
            RestSchemaActionOperation old_op, RestSchemaActionOperation new_op) {
        # an operation with no request body declares no content types at all
        list<string> old_ct = sort(keys old_op.getOperation().consumes) ?? ();
        list<string> new_ct = sort(keys new_op.getOperation().consumes) ?? ();
        if (old_ct == new_ct) {
            return ();
        }
        return (<RestSchemaDriftInfo>{
            "kind": RSD_CONTENT_TYPE_CHANGED,
            "action": action,
            "desc": sprintf("request content types changed from %y to %y", old_ct, new_ct),
            "old_value": old_ct,
            "new_value": new_ct,
            "compatible": False,
        },);
    }

    #! Compares the response published as the action's output
    private static list<hash<RestSchemaDriftInfo>> compareResponses(string action,
            RestSchemaActionOperation old_op, RestSchemaActionOperation new_op) {
        list<hash<RestSchemaDriftInfo>> rv = ();
        if (old_op.getResponseCode() != new_op.getResponseCode()) {
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_RESPONSE_CHANGED,
                "action": action,
                "desc": sprintf("the response published as the output changed from %y to %y",
                    old_op.getResponseCode(), new_op.getResponseCode()),
                "old_value": old_op.getResponseCode(),
                "new_value": new_op.getResponseCode(),
                "compatible": False,
            };
        }
        *list<string> old_out = old_op.getResponseType() ? sort(keys old_op.getResponseType().getFields()) : NOTHING;
        *list<string> new_out = new_op.getResponseType() ? sort(keys new_op.getResponseType().getFields()) : NOTHING;
        if (old_out != new_out) {
            *list<string> removed = map $1, old_out, !(new_out ?? ()).contains($1);
            rv += <RestSchemaDriftInfo>{
                "kind": RSD_RESPONSE_CHANGED,
                "action": action,
                "desc": sprintf("the output fields changed; removed: %y, added: %y", removed,
                    (map $1, new_out, !(old_out ?? ()).contains($1))),
                "old_value": old_out,
                "new_value": new_out,
                # a response that gained fields still satisfies everyone reading the old ones
                "compatible": !removed,
            };
        }
        return rv;
    }
}
}
