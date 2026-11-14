#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# RestSchemaModuleTypes.q (eager|lazy) <output file> <module>...: loads the given REST-schema provider modules
# and writes, as JSON, a structural digest of every action they register; used by the
# RestSchemaModuleLazyTypes-<n>.qtest tests (RestSchemaModuleLazyTypes.qc)

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires json
%requires DataProvider
%requires RestSchemaActions

#! Gives the types and fields reachable from a set of roots a digest of their structure
/** A type graph shares subtypes and is cyclic, and which objects it shares depends on the order its types were
    built in, so it is compared by structure: two types are equal if expanding their fields, element types,
    union members and default field types gives the same (possibly infinite) tree.

    The graph is reduced to its classes of structurally equal nodes by partition refinement, and the digest of a
    root is that of the reduced graph reachable from it, with its nodes numbered in the order they are reached;
    neither depends on object identities or on how the graph is shared.

    Each type or field object is read once.  Nodes are numbered, a node's content is kept once per distinct content,
    classes are numbers, and a digest is computed once per class.
*/
class TypeGraph {
    private {
        #! Node key (\c "t" or \c "f" and the object's unique hash) -> node number
        hash<string, int> nodes = {};

        #! Node number -> the type or field object
        /** Holding every object keeps it alive, so that no other object can be given its address, and so its key,
            while the graph exists
        */
        list<object> objects = ();

        #! Node number -> the number of the node's own content
        list<int> local = ();

        #! Content number -> content
        list<string> contents = ();

        #! Content -> content number
        hash<string, int> content_ids = {};

        #! Node number -> what each of its children is, in order
        list<list<string>> markers = ();

        #! Node number -> the node numbers of its children, in order
        list<list<int>> children = ();

        #! Node number -> structural class, once computed
        *list<int> cls;

        #! Structural class -> digest, once computed
        hash<string, string> digests = {};
    }

    #! Adds a type and everything reachable from it; returns its node number
    /** If reading a type or field throws an exception, the graph is left as it was before the call.
    */
    int addType(AbstractDataProviderType t) {
        int first = objects.size();
        int first_content = contents.size();
        list<string> new_keys = ();
        try {
            int rv = getNode("t", t, \new_keys);
            # nodes are numbered in the order they are found, and each new one is read in that order, once
            for (int n = first; n < objects.size(); ++n) {
                readNode(n, \new_keys);
            }
            remove cls;
            digests = {};
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            nodes -= new_keys;
            content_ids -= extract contents, first_content;
            splice objects, first;
            splice local, first;
            splice markers, first;
            splice children, first;
            rethrow;
        }
    }

    #! Returns the structural digest of the node with the given number
    string getDigest(int node) {
        if (!cls) {
            refine();
        }
        # structurally equal nodes have the same digest
        int root_class = cls[node];
        *string rv = digests{root_class};
        if (exists rv) {
            return rv;
        }
        # number the classes reachable from the root in the order they are reached
        hash<string, int> index = {root_class: 0};
        list<int> order = (node,);
        list<string> lines = ();
        for (int i = 0; i < order.size(); ++i) {
            int n = order[i];
            list<int> node_children = children[n];
            list<string> node_markers = markers[n];
            list<string> refs = ();
            for (int j = 0; j < node_children.size(); ++j) {
                int child = node_children[j];
                int c = cls[child];
                *int child_index = index{c};
                if (!exists child_index) {
                    child_index = order.size();
                    index{c} = child_index;
                    push order, child;
                }
                push refs, node_markers[j] + child_index;
            }
            push lines, contents[local[n]] + "->" + refs.join(",");
        }
        return digests{root_class} = lines.join("\n").toSHA256();
    }

    #! Returns the node number of a type (\c "t") or field (\c "f"), giving it the next number if it is new
    private int getNode(string kind, object o, reference<list<string>> new_keys) {
        string k = kind + o.uniqueHash();
        *int n = nodes{k};
        if (exists n) {
            return n;
        }
        n = objects.size();
        nodes{k} = n;
        push new_keys, k;
        push objects, o;
        return n;
    }

    #! Reads the content and the children of the given node, which is the next one to be read
    private readNode(int n, reference<list<string>> new_keys) {
        @assert(local.size() == n);
        object o = objects[n];
        string content;
        list<string> node_markers = ();
        list<int> node_children = ();
        if (o instanceof AbstractDataField) {
            AbstractDataField f = cast<AbstractDataField>(o);
            content = TypeGraph::toJson(f.getLocalInfo() - "unique_key");
            push node_markers, "type";
            push node_children, getNode("t", f.getType(), \new_keys);
        } else {
            AbstractDataProviderType dt = cast<AbstractDataProviderType>(o);
            content = TypeGraph::toJson({
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
            });
            if (*hash<string, AbstractDataField> fields = dt.getFields()) {
                foreach AbstractDataField f in (fields.iterator()) {
                    push node_markers, "field";
                    push node_children, getNode("f", f, \new_keys);
                }
            }
            if (*AbstractDataProviderType et = dt.getElementType()) {
                push node_markers, "element";
                push node_children, getNode("t", et, \new_keys);
            }
            if (dt instanceof QoreUnionDataType) {
                foreach AbstractDataProviderType ut in (cast<QoreUnionDataType>(dt).getUnionTypes()) {
                    push node_markers, "union";
                    push node_children, getNode("t", ut, \new_keys);
                }
            }
            if (dt instanceof HashDataType) {
                if (*AbstractDataProviderType ot = cast<HashDataType>(dt).getDefaultOtherFieldType()) {
                    push node_markers, "other";
                    push node_children, getNode("t", ot, \new_keys);
                }
            }
        }
        *int id = content_ids{content};
        if (!exists id) {
            id = contents.size();
            content_ids{content} = id;
            push contents, content;
        }
        push local, id;
        push markers, node_markers;
        push children, node_children;
    }

    #! Partitions the nodes into classes of structurally equal nodes
    /** The first partition is by content; each iteration then splits the nodes of a class whose children are in
        different classes.  Refinement only splits classes, so the partition is final once an iteration splits none.
        A class is a number, and an iteration numbers the classes it finds in the order it finds them.
    */
    private refine() {
        int count = local.size();
        # content numbers are given in the order nodes are read, so they are the first partition's numbers
        list<int> c = local;
        int classes = contents.size();
        while (True) {
            list<int> next = ();
            hash<string, int> ids = {};
            for (int n = 0; n < count; ++n) {
                list<int> node_children = children[n];
                list<string> node_markers = markers[n];
                string signature = c[n] + "|";
                for (int j = 0; j < node_children.size(); ++j) {
                    signature += node_markers[j] + c[node_children[j]] + ",";
                }
                *int id = ids{signature};
                if (!exists id) {
                    id = ids.size();
                    ids{signature} = id;
                }
                push next, id;
            }
            c = next;
            if (ids.size() == classes) {
                break;
            }
            classes = ids.size();
        }
        cls = c;
    }

    #! Returns a value as JSON, with objects and code replaced by their class name
    /** Most values hold neither, so the value is serialized as it is, and only one that cannot be is made plain
        first
    */
    static string toJson(auto v) {
        try {
            return make_json(v);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "JSON-SERIALIZATION-ERROR") {
                rethrow;
            }
        }
        return make_json(TypeGraph::plain(v));
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
    # the actions as registered, before any output type is resolved: using the app runs its lazy action
    # registrations, which resolve no output type, and the NoInit getter returns what they registered
    DataProviderActionCatalog::getApp(app);
    foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActionsNoInit(app)) {
        info.actions{action.action}.registered_lazy = exists action.get_output_type;
    }
    try {
        # one graph per application: applications share almost no type objects, and the graph of one is freed
        # before the next is read
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
                # node 0 is a valid node number
                "output_type": exists root.output_type ? graph.getDigest(root.output_type) : NOTHING,
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
