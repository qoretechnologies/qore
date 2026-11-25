# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderMessageFormat.qc describes delivery action message formats

/** DataProviderMessageFormat.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DataProvider {

#! Provider-owned message formats for a particular delivery action
/** Each key in @ref DataProviderActionBaseInfo::message_formats identifies an exact variant. Use
    `default` for an action with one contract. The consumer chooses the variant from the actual
    destination and authorization context. Each variant maps stable format IDs to forms. A missing
    declaration is unknown; an empty variant declares no forms.

    A form names its `content` option path. Optional paths are `subject`, `fallback`, `selector`,
    `mentions` and `link_preview`. A selector or control declares its fixed `_value`. The `native`
    string identifies a structured payload schema and its `native_type` is `list` or `hash`. Set `plain_alternative` to true only when the
    same submission transmits the fallback text, or false for a known single representation. Omit
    unknown capabilities. A positive `limit` requires a `unit` such as `characters` or `bytes`.

    Paths are dot-separated option/field names. The independent option metadata `message_role` grants
    one role: `text` (content or fallback), `native`, `subject`, `selector`, `mentions` or
    `link_preview`. Each enclosing hash must grant the `container` role. A nested field stores its grant
    in its field attributes. Only mark fields owned by message presentation. Recipient, connection,
    credential and webhook options are outside this contract. Apply a renderer's values only to the
    paths in an installed, checked form. Labels come from the ordinary localized option and allowed-value
    projections. Rendering, routing and disclosure decisions belong to the consumer.

    Save @ref selectForm() with the design and call @ref checkSavedForm() on the selected action before
    use. Selecting any replacement form requires an explicit decision.
*/
public class DataProviderMessageFormat {
    private {
        const PathRoles = {
            "content": "text", "fallback": "text", "subject": "subject", "selector": "selector",
            "mentions": "mentions", "link_preview": "link_preview",
        };
        const FormKeys = (keys PathRoles) + ("native", "native_type", "plain_alternative", "selector_value",
            "mentions_value", "link_preview_value", "limit", "unit");
    }

    #! Checks all forms against the static options of a registering action
    static validate(hash<auto> action) {
        foreach hash<auto> variant in (action.message_formats.pairIterator()) {
            string id = sprintf("%s/%s [%s]", action.app, action.action, variant.key);
            if (!variant.key || variant.value.typeCode() != NT_HASH) {
                throw "MESSAGE-FORMAT-ERROR", id + ": expected a named variant containing forms";
            }
            hash<auto> selectors;
            foreach hash<auto> entry in (variant.value.pairIterator()) {
                if (!entry.key || entry.value.typeCode() != NT_HASH) {
                    throw "MESSAGE-FORMAT-ERROR", id + ": expected a named message form";
                }
                checkForm(id + "/" + entry.key, action, entry.value);
                if (entry.value.selector) {
                    string path = entry.value.selector;
                    string value = entry.value.selector_value;
                    if (exists selectors{path}{value}) {
                        throw "MESSAGE-FORMAT-ERROR", sprintf("%s: %s and %s declare the same selector value",
                            id, selectors{path}{value}, entry.key);
                    }
                    selectors{path}{value} = entry.key;
                }
            }
        }
    }

    #! Returns a snapshot of the verified form from a registered action or the selected detailed indexed action
    static hash<auto> selectForm(hash<auto> action, string variant, string format) {
        string id = sprintf("%s/%s [%s]/%s", action.app, action.action, variant, format);
        auto form = action.message_formats{variant}{format};
        if (form.typeCode() != NT_HASH) {
            throw "MESSAGE-FORMAT-UNSUPPORTED", id + ": form unavailable";
        }
        checkForm(id, action, form);
        return {"app": action.app, "action": action.action, "variant": variant, "format": format, "form": form};
    }

    #! Checks a saved form against the installed action and variant, identifying the first changed feature
    static hash<auto> checkSavedForm(hash<auto> action, string variant, hash<auto> saved) {
        string id = sprintf("%s/%s [%s]/%s", action.app, action.action, variant, saved.format);
        if (saved.app !== action.app || saved.action !== action.action || saved.variant !== variant) {
            throw "MESSAGE-FORMAT-UNSUPPORTED", id + ": action or variant changed";
        }
        hash<auto> current = selectForm(action, variant, saved.format);
        foreach string key in (keys (current.form + saved.form)) {
            if (current.form{key} !== saved.form{key}) {
                throw "MESSAGE-FORMAT-UNSUPPORTED", sprintf("%s: %s changed", id, key);
            }
        }
        return current;
    }

    #! Applies a checked design's rendered fields to a copy of the caller's action options
    /** The renderer supplies content, subject or fallback and may request a declared mentions or link_preview
        control with True. Selector and control values always come from the installed form. A change to the saved
        form raises an exception before returning any options. This method does not execute the action.
    */
    static hash<auto> bindForm(hash<auto> action, string variant, hash<auto> saved, hash<auto> options,
            hash<auto> rendered) {
        hash<auto> form = checkSavedForm(action, variant, saved).form;
        string id = sprintf("%s/%s [%s]/%s", action.app, action.action, variant, saved.format);
        if (!exists rendered.content || (form.fallback && !exists rendered.fallback)) {
            throw "MESSAGE-FORMAT-ERROR", id + ": required content or fallback is absent";
        }
        foreach hash<auto> entry in (rendered.pairIterator()) {
            string key = entry.key;
            if (!PathRoles.hasKey(key) || key == "selector" || !form{key}) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("%s: cannot bind %s", id, key);
            }
            auto value = entry.value;
            if (key == "mentions" || key == "link_preview") {
                if (value !== True) {
                    throw "MESSAGE-FORMAT-ERROR", sprintf("%s: expected True to apply %s", id, key);
                }
                value = form{key + "_value"};
            } else if (key == "content" && form.native) {
                if (value.type() != form.native_type) {
                    throw "MESSAGE-FORMAT-ERROR", id + ": expected structured content";
                }
            } else if (value.typeCode() != NT_STRING) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("%s: expected text for %s", id, key);
            }
            options = writePath(options, form{key}.split("."), 0, value);
        }
        if (form.selector) {
            options = writePath(options, form.selector.split("."), 0, form.selector_value);
        }
        return options;
    }

    private static hash<auto> writePath(hash<auto> options, list<string> parts, int i, auto value) {
        string key = parts[i];
        # An incoming hash of recipient strings may need to hold a structured body or a boolean control.
        hash<auto!> bound = {};
        bound += options;
        if (i + 1 == parts.size()) {
            bound{key} = value;
        } else {
            if (exists options{key} && options{key}.typeCode() != NT_HASH) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("cannot bind a field inside %s", key);
            }
            bound{key} = writePath(options{key} ?? {}, parts, i + 1, value);
        }
        return bound;
    }

    private static checkForm(string id, hash<auto> action, hash<auto> form) {
        if (!form.content || (form - FormKeys)) {
            throw "MESSAGE-FORMAT-ERROR", id + ": expected content and recognized presentation fields";
        }
        if ((exists form.native || exists form.native_type) && (form.native.typeCode() != NT_STRING || !form.native
                || (form.native_type !== "hash" && form.native_type !== "list"))) {
            throw "MESSAGE-FORMAT-ERROR", id + ": a native schema requires a list or hash payload type";
        }
        if ((exists form.plain_alternative && form.plain_alternative.typeCode() != NT_BOOLEAN)
                || (form.plain_alternative && !form.fallback) || (form.plain_alternative === False && form.fallback)) {
            throw "MESSAGE-FORMAT-ERROR", id + ": inconsistent fallback declaration";
        }
        if ((exists form.unit || exists form.limit) && (form.limit.typeCode() != NT_INT || form.limit <= 0
                || form.unit.typeCode() != NT_STRING || !form.unit)) {
            throw "MESSAGE-FORMAT-ERROR", id + ": a limit requires a positive integer and a unit";
        }
        hash<string, bool> paths;
        foreach hash<auto> entry in (PathRoles.pairIterator()) {
            string key = entry.key;
            bool fixed = key == "selector" || key == "mentions" || key == "link_preview";
            auto path = form{key};
            auto value = form{key + "_value"};
            if (fixed && ((exists path) != (exists value))) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("%s: %s requires a path and fixed value", id, key);
            }
            if (!exists path) {
                continue;
            }
            if (path.typeCode() != NT_STRING || path !~ /^[a-zA-Z0-9_-]+(\.[a-zA-Z0-9_-]+)*$/) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("%s: invalid %s option path", id, key);
            }
            foreach string other in (keys paths) {
                if (other == path || path.startsWith(other + ".") || other.startsWith(path + ".")) {
                    throw "MESSAGE-FORMAT-ERROR", sprintf("%s: overlapping %s and %s", id, other, path);
                }
            }
            paths{path} = True;
            string role = key == "content" && form.native ? "native" : entry.value;
            checkPath(id, action, path, role, value, form.native_type);
        }
    }

    private static checkPath(string id, hash<auto> action, string path, string role, auto value, *string native_type) {
        list<string> parts = path.split(".");
        if (!action.hasKey("options")) {
            throw "MESSAGE-FORMAT-ERROR", id + ": restore the selected action's detailed options";
        }
        hash<auto> opt = action.options{parts[0]} ?? {};
        if (action.metadata.connection_alternative.option === parts[0] || action.path_vars.hasKey(parts[0])) {
            throw "MESSAGE-FORMAT-ERROR", sprintf("%s: %s is a routing option", id, path);
        }
        *AbstractDataProviderType type = opt.hasKey("type") ? opt.type : NOTHING;
        if (!type && opt.hasKey("type_info")) {
            type = AbstractDataProviderType::get(opt.type_info);
        }
        for (int i = 0; i < parts.size(); ++i) {
            string required_role = i + 1 == parts.size() ? role : "container";
            if (!type || opt.sensitive || opt.readonly || opt.disabled || opt.ui_type == "password"
                    || opt.metadata.message_role !== required_role) {
                throw "MESSAGE-FORMAT-ERROR", sprintf("%s: %s lacks a writable %s declaration", id, path,
                    required_role);
            }
            if (i + 1 < parts.size()) {
                *AbstractDataField field = type.getField(parts[i + 1]);
                if (!field) {
                    throw "MESSAGE-FORMAT-ERROR", sprintf("%s: field %s is absent", id, path);
                }
                opt = field.getPresentationInfo() + field.getAttributes();
                type = field.getDeclaredType();
            }
        }
        string base = type.getBaseTypeName();
        bool compatible;
        switch (role) {
            case "native":
                compatible = base == native_type || base == "auto" || base == "any";
                break;
            case "mentions":
            case "link_preview":
                compatible = base == "bool" && value.typeCode() == NT_BOOLEAN;
                break;
            default:
                compatible = base == "string";
        }
        if (!compatible) {
            throw "MESSAGE-FORMAT-ERROR", sprintf("%s: %s type %s does not support %s", id, path, base, role);
        }
        if (role == "selector" && (value.typeCode() != NT_STRING
                || !(map True, opt.allowed_values, $1.value === value))) {
            throw "MESSAGE-FORMAT-ERROR", sprintf("%s: selector %s has no declared value %y", id, path, value);
        }
    }
}
}
