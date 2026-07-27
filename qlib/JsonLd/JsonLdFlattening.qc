# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdFlattening.qc @brief JSON-LD Flattening Algorithm (W3C 4.6)

/*  JsonLdFlattening.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! JSON-LD Flattening Algorithm
/** Implements the Flattening Algorithm (W3C Section 4.6) and
    Node Map Generation Algorithm (W3C Section 4.6.2).

    @since json 1.12
*/
public class JsonLdFlattening {
    public {
        #! Blank node generator
        JsonLdBlankNodeGenerator blankNodeGenerator;
    }

    #! Creates a new flattening processor
    constructor() {
        blankNodeGenerator = new JsonLdBlankNodeGenerator();
    }

    #! Flattens an expanded JSON-LD document
    /** @param expanded the expanded document (list of node objects)
        @return a flattened document with \@graph containing all nodes

        @since json 1.12
    */
    hash<auto> flatten(list<auto> expanded) {
        # Generate node map
        hash<auto!> node_map = {"@default": {}};
        foreach auto item in (expanded) {
            generateNodeMap(item, \node_map, "@default");
        }

        # Build flattened output from default graph
        list<auto> flattened = ();
        hash<auto> default_graph = node_map."@default";
        foreach string id in (sort(keys default_graph)) {
            hash<auto!> node();
            node += default_graph{id};

            # If there's a named graph for this id, add it
            if (exists node_map{id}) {
                list<auto> graph_nodes = ();
                hash<auto> graph = node_map{id};
                foreach string gid in (sort(keys graph)) {
                    hash<auto> gnode = graph{gid};
                    if (gnode.size() > 1 || !exists gnode."@id") {
                        push graph_nodes, gnode;
                    }
                }
                if (graph_nodes.size() > 0) {
                    node."@graph" = graph_nodes;
                }
            }

            if (node.size() > 1 || !exists node."@id") {
                push flattened, node;
            }
        }

        return {"@graph": flattened};
    }

    #! Generates the node map from an expanded element
    /** Implements the Node Map Generation Algorithm (W3C Section 4.6.2)

        @param element the expanded element
        @param node_map reference to the node map
        @param active_graph the active graph name
        @param active_subject the active subject (node being built)
        @param active_property the active property
        @param list_ reference to the list being built (for \@list)

        @since json 1.12
    */
    generateNodeMap(auto element, reference<hash<auto!>> node_map, string active_graph,
            *hash<auto> active_subject, *string active_property,
            *reference<list<auto>> list_) {
        # 1. If element is an array
        if (element.typeCode() == NT_LIST) {
            foreach auto item in (element) {
                generateNodeMap(item, \node_map, active_graph, active_subject,
                    active_property, \list_);
            }
            return;
        }

        # Element must be a hash
        if (element.typeCode() != NT_HASH) {
            return;
        }

        hash<auto> elem = element;

        # Ensure graph exists in node map
        if (!exists node_map{active_graph}) {
            node_map{active_graph} = {};
        }

        # 2. If element has @type, rename blank node types (only for node objects)
        if (exists elem."@type" && !isValueObject(elem)) {
            list<auto> types = elem."@type".typeCode() == NT_LIST
                ? elem."@type" : (elem."@type",);
            list<auto> new_types = ();
            foreach auto t in (types) {
                if (t.typeCode() == NT_STRING && isBlankNodeIdentifier(t)) {
                    push new_types, blankNodeGenerator.getId(t);
                } else {
                    push new_types, t;
                }
            }
            elem."@type" = new_types;
        }

        # 3. If element is a value object
        if (isValueObject(elem)) {
            if (exists list_) {
                push list_, elem;
            } else if (exists active_subject && exists active_property) {
                addNodeProperty(\node_map, active_graph, active_subject."@id",
                    active_property, elem);
            }
            return;
        }

        # 4. If element is a list object
        if (isListObject(elem)) {
            list<auto> result_list = ();
            generateNodeMap(elem."@list", \node_map, active_graph, active_subject,
                active_property, \result_list);
            hash<auto> list_result = {"@list": result_list};
            if (exists list_) {
                push list_, list_result;
            } else if (exists active_subject && exists active_property) {
                addNodeProperty(\node_map, active_graph, active_subject."@id",
                    active_property, list_result);
            }
            return;
        }

        # 5. Element is a node object
        # 5.1 Get or assign @id
        # If @id is explicitly null (hasKey but no value), skip this node
        if (elem.hasKey("@id") && !exists elem."@id") {
            return;
        }
        string id;
        if (exists elem."@id") {
            id = elem."@id";
            if (isBlankNodeIdentifier(id)) {
                id = blankNodeGenerator.getId(id);
            }
            remove elem."@id";
        } else {
            id = blankNodeGenerator.generate();
        }

        # 5.2 Ensure node exists in the graph
        if (!exists node_map{active_graph}{id}) {
            hash<auto!> new_node = {"@id": id};
            node_map{active_graph}{id} = new_node;
        }

        # 5.3 Add reference to active subject
        hash<auto> reference = {"@id": id};
        if (exists list_) {
            push list_, reference;
        } else if (exists active_subject && exists active_property) {
            addNodeProperty(\node_map, active_graph, active_subject."@id",
                active_property, reference);
        }

        # 5.4 Get the node
        # 5.5 Process @type
        if (exists elem."@type") {
            list<auto> types = elem."@type".typeCode() == NT_LIST
                ? elem."@type" : (elem."@type",);
            foreach auto t in (types) {
                addNodeProperty(\node_map, active_graph, id, "@type", t);
            }
            remove elem."@type";
        }

        # 5.6 Process @index
        if (exists elem."@index") {
            # Check for conflicting indexes
            if (exists node_map{active_graph}{id}."@index"
                    && node_map{active_graph}{id}."@index" != elem."@index") {
                throwJsonLdError(JSONLD_ERR_COLLIDING_KEYWORDS,
                    sprintf("conflicting indexes for node %y", id));
            }
            node_map{active_graph}{id}."@index" = elem."@index";
            remove elem."@index";
        }

        # 5.7 Process @reverse — the referenced nodes are the SUBJECTS
        if (exists elem."@reverse") {
            hash<auto> reverse = elem."@reverse";
            foreach string prop in (keys reverse) {
                list<auto> values = reverse{prop}.typeCode() == NT_LIST
                    ? reverse{prop} : (reverse{prop},);
                foreach auto val in (values) {
                    # For @reverse: the val is the subject, current node (id) is the object
                    # Generate the node map for the referenced node
                    # For nodes without @id, generateNodeMap will assign one via generate()
                    generateNodeMap(val, \node_map, active_graph);
                    # Find the assigned ID by checking the node map
                    string ref_id;
                    if (val.typeCode() == NT_HASH) {
                        hash<auto> val_hash = val;
                        if (exists val_hash."@id") {
                            string orig_id = val_hash."@id";
                            # getId returns the mapped ID
                            ref_id = isBlankNodeIdentifier(orig_id)
                                ? blankNodeGenerator.getId(orig_id) : orig_id;
                        } else {
                            # Node had no @id — find the ID that was just assigned
                            # by looking at the last generated ID in the node map
                            # Find keys in the graph that match generated IDs
                            hash<auto> graph = node_map{active_graph};
                            # The generated ID for this node is the highest ID that
                            # has properties matching the val's expanded form
                            foreach string nid in (sort(keys graph)) {
                                if (isBlankNodeIdentifier(nid) && graph{nid}.typeCode() == NT_HASH
                                        && ((graph{nid})).size() > 1) {
                                    # Check if this node has properties from val_hash
                                    bool matches = True;
                                    foreach string vk in (keys val_hash) {
                                        if (vk == "@reverse") {
                                            continue;
                                        }
                                        if (!graph{nid}.hasKey(vk) && vk != "@id") {
                                            matches = False;
                                            break;
                                        }
                                    }
                                    if (matches) {
                                        ref_id = nid;
                                    }
                                }
                            }
                        }
                    }
                    # Add a reference from the referenced node to the current node
                    if (exists ref_id) {
                        addNodeProperty(\node_map, active_graph, ref_id,
                            prop, {"@id": id});
                    }
                }
            }
            remove elem."@reverse";
        }

        # 5.8 Process @graph
        if (exists elem."@graph") {
            # Per W3C spec step 6.7: ensure the node is in the default graph
            # so that named graph nodes appear at the top level in flattened output
            if (!exists node_map."@default"{id}) {
                node_map."@default"{id} = {"@id": id};
            }
            generateNodeMap(elem."@graph", \node_map, id);
            remove elem."@graph";
        }

        # 5.9 Process @included
        if (exists elem."@included") {
            generateNodeMap(elem."@included", \node_map, active_graph);
            remove elem."@included";
        }

        # 5.10 Process remaining properties
        foreach string prop in (sort(keys elem)) {
            auto value = elem{prop};
            if (isJsonLdKeyword(prop)) {
                continue;
            }
            list<auto> values = value.typeCode() == NT_LIST ? value : (value,);
            if (values.empty()) {
                # Preserve empty arrays (from @set containers)
                if (!exists node_map{active_graph}{id}{prop}) {
                    node_map{active_graph}{id}{prop} = ();
                }
            }
            foreach auto val in (values) {
                generateNodeMap(val, \node_map, active_graph, {"@id": id}, prop);
            }
        }
    }

    #! Adds a property value to a node in the node map (avoiding duplicates)
    private addNodeProperty(reference<hash<auto!>> node_map, string graph, string subject_id,
            string property, auto value) {
        if (!exists node_map{graph}{subject_id}) {
            hash<auto!> new_node = {"@id": subject_id};
            node_map{graph}{subject_id} = new_node;
        }

        # Work on the node map directly to avoid type narrowing issues
        auto existing = node_map{graph}{subject_id}{property};
        if (!exists existing) {
            node_map{graph}{subject_id}{property} = (value,);
        } else {
            # Avoid duplicates (except @list objects, which are always unique per spec)
            bool is_list_obj = value.typeCode() == NT_HASH && isListObject(value);
            if (existing.typeCode() == NT_LIST) {
                bool found = False;
                if (!is_list_obj) {
                    foreach auto e in (existing) {
                        # Type-aware comparison: {"@value": true} != {"@value": 1}
                        if (e == value) {
                            # Extra check: if both are hashes with @value, compare types
                            if (e.typeCode() == NT_HASH && value.typeCode() == NT_HASH
                                    && e.hasKey("@value") && value.hasKey("@value")
                                    && e."@value".typeCode() != value."@value".typeCode()) {
                                continue;  # Different native types — not a duplicate
                            }
                            found = True;
                            break;
                        }
                    }
                }
                if (!found) {
                    list<auto!> updated = existing;
                    push updated, value;
                    node_map{graph}{subject_id}{property} = updated;
                }
            } else {
                if (is_list_obj || existing != value) {
                    node_map{graph}{subject_id}{property} = (existing, value);
                }
            }
        }
    }
}

} # namespace JsonLd
