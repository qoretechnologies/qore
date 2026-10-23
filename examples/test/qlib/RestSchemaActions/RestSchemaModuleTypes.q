#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# RestSchemaModuleTypes.q (eager|lazy) <output file> <module>...: loads the given REST-schema provider modules
# and writes, as JSON, a structural digest of every action they register; used by RestSchemaModuleLazyTypes.qtest

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires json
%requires ../../../../qlib/DataProvider
%requires ../../../../qlib/RestSchemaActions

#! Gives the types and fields reachable from a set of roots a digest of their structure
/** A type graph shares subtypes and is cyclic, and which objects it shares depends on the order its types were
    built in, so it is compared by structure: two types are equal if expanding their fields, element types,
    union members and default field types gives the same (possibly infinite) tree.

    The graph is reduced to its classes of structurally equal nodes by partition refinement, and the digest of a
    root is that of the reduced graph reachable from it, with its nodes numbered in the order they are reached;
    neither depends on object identities or on how the graph is shared.
*/
class TypeGraph {
    private {
        #! Node key -> the node's own content
        hash<string, string> local = {};

        #! Node key -> the keys of its children, in order, each prefixed with what the child is
        hash<string, list<string>> kids = {};

        #! Node key -> the key of the node, without the prefix, by child entry
        hash<string, list<string>> kid_keys = {};

        #! Node key -> structural class, once computed
        *hash<string, string> cls;
    }

    #! Adds a type and everything reachable from it; returns its key
    string addType(AbstractDataProviderType t) {
        string key = "t" + t.uniqueHash();
        # an untyped list, so that the literal does not narrow its element type to the root's class
        list<auto> stack = ();
        push stack, t;
        while (stack) {
            auto o = pop stack;
            string k = (o instanceof AbstractDataField ? "f" : "t") + o.uniqueHash();
            if (exists local{k}) {
                continue;
            }
            # children are objects, each preceded by a marker saying what it is
            list<auto> children = ();
            if (o instanceof AbstractDataField) {
                AbstractDataField f = o;
                local{k} = make_json(TypeGraph::plain(f.getLocalInfo() - "unique_key"));
                children += ("type", f.getType());
            } else {
                AbstractDataProviderType dt = o;
                local{k} = make_json(TypeGraph::plain({
                    "class": dt.className(),
                    "name": dt.getName(),
                    "display_name": dt.getDisplayName(),
                    "short_desc": dt.getShortDescription(),
                    "desc": dt.getDesc(),
                    "base_type": dt.getBaseTypeName(),
                    "mandatory": dt.isMandatory(),
                    "options": dt.getOptions(),
                    "tags": dt.getTags(),
                    "constraints": dt.getConstraints(),
                    "default_value": dt.getDefaultValue(),
                    "path": dt.getPath(),
                    "allowed_values": dt.getAllowedValues(),
                    "element_allowed_values": dt.getElementAllowedValues(),
                }));
                if (*hash<string, AbstractDataField> fields = dt.getFields()) {
                    foreach AbstractDataField f in (fields.iterator()) {
                        children += ("field", f);
                    }
                }
                if (*AbstractDataProviderType et = dt.getElementType()) {
                    children += ("element", et);
                }
                if (dt instanceof QoreUnionDataType) {
                    foreach AbstractDataProviderType ut in (cast<QoreUnionDataType>(dt).getUnionTypes()) {
                        children += ("union", ut);
                    }
                }
                if (dt instanceof HashDataType) {
                    if (*AbstractDataProviderType ot = cast<HashDataType>(dt).getDefaultOtherFieldType()) {
                        children += ("other", ot);
                    }
                }
            }
            list<string> markers = ();
            list<string> child_keys = ();
            for (int i = 0; i < children.size(); i += 2) {
                auto c = children[i + 1];
                push markers, children[i];
                push child_keys, (c instanceof AbstractDataField ? "f" : "t") + c.uniqueHash();
                push stack, c;
            }
            kids{k} = markers;
            kid_keys{k} = child_keys;
        }
        remove cls;
        return key;
    }

    #! Returns the structural digest of the node with the given key
    string getDigest(string key) {
        if (!cls) {
            refine();
        }
        # number the classes reachable from the root in the order they are reached
        hash<string, int> index = {cls{key}: 0};
        list<string> order = (key,);
        list<string> rv = ();
        for (int i = 0; i < order.size(); ++i) {
            string k = order[i];
            list<string> refs = ();
            for (int j = 0; j < kid_keys{k}.size(); ++j) {
                string child = kid_keys{k}[j];
                string c = cls{child};
                if (!exists index{c}) {
                    index{c} = order.size();
                    push order, child;
                }
                push refs, kids{k}[j] + index{c};
            }
            push rv, local{k} + "->" + refs.join(",");
        }
        return rv.join("\n").toSHA256();
    }

    #! Partitions the nodes into classes of structurally equal nodes
    /** Refinement only splits classes, so the partition is final once an iteration splits none.
    */
    private refine() {
        hash<string, string> c = map {$1.key: $1.value.toSHA256()}, local.pairIterator();
        int classes = (map {$1: True}, c.iterator()).size();
        while (True) {
            hash<string, string> next = {};
            foreach hash<auto> i in (c.pairIterator()) {
                list<string> refs = ();
                for (int j = 0; j < kid_keys{i.key}.size(); ++j) {
                    push refs, kids{i.key}[j] + c{kid_keys{i.key}[j]};
                }
                next{i.key} = (i.value + "|" + refs.join(",")).toSHA256();
            }
            int next_classes = (map {$1: True}, next.iterator()).size();
            c = next;
            if (next_classes == classes) {
                break;
            }
            classes = next_classes;
        }
        cls = c;
    }

    #! Returns a value with objects and code replaced by their class name
    static auto plain(auto v) {
        switch (v.typeCode()) {
            case NT_HASH:
                return map {$1.key: TypeGraph::plain($1.value)}, v.pairIterator();
            case NT_LIST:
                return map TypeGraph::plain($1), v;
            case NT_OBJECT:
                return "<" + v.className() + ">";
            case NT_CLOSURE:
            case NT_CALLREF:
                return "<code>";
        }
        return v;
    }
}

string mode = shift ARGV;
string output = shift ARGV;
if (mode == "eager") {
    # resolved at run time, so that the script also runs where the override does not exist
    call_static_method("RestSchemaActions::RestSchemaActionSet", "setEagerResponseTypes", True);
}

hash<string, bool> before = map {$1.name: True}, DataProviderActionCatalog::getAllAppsNoInit();
hash<string, string> app_modules = {};
foreach string mod in (ARGV) {
    load_module(mod);
    foreach hash<DataProviderAppInfo> app in (DataProviderActionCatalog::getAllAppsNoInit()) {
        if (!before{app.name}) {
            before{app.name} = True;
            app_modules{app.name} = mod;
        }
    }
}

hash<auto> rv = {"apps": {}};
foreach hash<auto> i in (app_modules.pairIterator()) {
    string app = i.key;
    hash<auto> info = {"module": i.value, "actions": {}};
    # the actions as registered, before any output type is resolved
    foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActionsNoInit(app)) {
        info.actions{action.action}.registered_lazy = exists action.get_output_type;
    }
    try {
        TypeGraph graph();
        list<hash<auto>> roots = ();
        foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActions(app)) {
            hash<auto> root = {
                "action": action.action,
                "metadata": make_json(TypeGraph::plain(action - ("output_type", "get_output_type",
                    "get_dynamic_options", "options"))),
                "options": {},
            };
            if (action.output_type) {
                root.output_type = graph.addType(action.output_type);
            }
            foreach hash<auto> o in (action.options.pairIterator()) {
                root.options{o.key} = {
                    "info": make_json(TypeGraph::plain(o.value - "type")),
                    "type": graph.addType(o.value.type),
                };
            }
            push roots, root;
        }
        foreach hash<auto> root in (roots) {
            hash<auto> options = {};
            foreach hash<auto> o in (root.options.pairIterator()) {
                options{o.key} = (o.value.info + graph.getDigest(o.value.type)).toSHA256();
            }
            info.actions{root.action} += {
                "metadata": root.metadata.toSHA256(),
                "output_type": root.output_type ? graph.getDigest(root.output_type) : NOTHING,
                "options": options,
            };
        }
    } catch (hash<ExceptionInfo> ex) {
        info.error = sprintf("%s: %s", ex.err, ex.desc);
    }
    rv.apps{app} = info;
}

File f();
f.open2(output, O_CREAT | O_WRONLY | O_TRUNC);
f.write(make_json(rv));
f.close();
