# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdRdf.qc @brief JSON-LD to/from RDF and N-Quads serialization

/*  JsonLdRdf.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! JSON-LD RDF serialization
/** Implements toRdf (W3C 4.8), fromRdf (W3C 4.9), and N-Quads
    serialization/parsing.

    @since json 1.12
*/
public class JsonLdRdf {
    private {
        #! Blank node generator for list serialization
        JsonLdBlankNodeGenerator blankNodeGenerator;

        #! Processing options
        hash<JsonLdOptions> options;
    }

    #! Creates a new RDF processor
    constructor(hash<JsonLdOptions> opts) {
        blankNodeGenerator = new JsonLdBlankNodeGenerator("_:b");
        options = opts;
    }

    #! Converts expanded JSON-LD to an RDF dataset
    /** @param expanded the expanded JSON-LD document
        @return the RDF dataset

        @since json 1.12
    */
    hash<RdfDataset> toRdf(list<auto> expanded) {
        hash<RdfDataset> dataset();

        # Generate node map first
        JsonLdFlattening flattener();
        hash<auto!> node_map = {"@default": {}};
        foreach auto item in (expanded) {
            flattener.generateNodeMap(item, \node_map, "@default");
        }

        # Use the flattener's blank node generator for RDF serialization
        # to avoid blank node ID collisions
        blankNodeGenerator = flattener.blankNodeGenerator;

        # Process each graph
        foreach string graph_name in (sort(keys node_map)) {
            # Skip non-well-formed graph names
            if (graph_name != "@default" && !isWellFormedIriForRdf(graph_name)) {
                continue;
            }
            hash<auto> graph = node_map{graph_name};
            foreach string subject_id in (sort(keys graph)) {
                hash<auto> node = graph{subject_id};
                processNodeToRdf(\dataset, node, graph_name);
            }
        }

        return dataset;
    }

    #! Converts an RDF dataset to expanded JSON-LD
    /** @param dataset the RDF dataset
        @return the expanded JSON-LD

        @since json 1.12
    */
    list<auto> fromRdf(hash<RdfDataset> dataset) {
        hash<auto!> graph_map = {"@default": {}};

        # Process default graph
        processQuadsToJsonLd(dataset.defaultGraph, \graph_map, "@default");

        # Process named graphs
        foreach string graph_name in (keys dataset.namedGraphs) {
            processQuadsToJsonLd(dataset.namedGraphs{graph_name}, \graph_map, graph_name);
        }

        # Find blank nodes that appear in multiple graphs (shared across graphs)
        # Track both node definitions and references
        hash<string, hash<string, bool>> bn_graphs = {};
        foreach string gn in (keys graph_map) {
            if (graph_map{gn}.typeCode() != NT_HASH) {
                continue;
            }
            foreach string nid in (keys graph_map{gn}) {
                # Node defined in this graph
                if (isBlankNodeIdentifier(nid)) {
                    if (!exists bn_graphs{nid}) {
                        bn_graphs{nid} = {};
                    }
                    bn_graphs{nid}{gn} = True;
                }
                # Also check for blank node references in values
                if (graph_map{gn}{nid}.typeCode() == NT_HASH) {
                    foreach string prop in (keys graph_map{gn}{nid}) {
                        auto vals = graph_map{gn}{nid}{prop};
                        if (vals.typeCode() == NT_LIST) {
                            foreach auto v in (vals) {
                                if (v.typeCode() == NT_HASH && exists v."@id"
                                        && isBlankNodeIdentifier(v."@id")) {
                                    string ref_id = v."@id";
                                    if (!exists bn_graphs{ref_id}) {
                                        bn_graphs{ref_id} = {};
                                    }
                                    bn_graphs{ref_id}{gn} = True;
                                }
                            }
                        }
                    }
                }
            }
        }
        # Collect cross-graph shared blank nodes
        hash<string, bool> shared_bnodes = {};
        foreach string bn in (keys bn_graphs) {
            if (bn_graphs{bn}.size() > 1) {
                shared_bnodes{bn} = True;
            }
        }

        # Reconstruct lists in each graph (skip cross-graph shared nodes)
        foreach string gn in (keys graph_map) {
            reconstructLists(\graph_map{gn}, shared_bnodes);
        }

        # Reconstruct compound literals if rdfDirection is "compound-literal"
        if (options.rdfDirection == "compound-literal") {
            foreach string gn in (keys graph_map) {
                reconstructCompoundLiterals(\graph_map{gn});
            }
        }

        # Ensure named graph identifiers exist in default graph (W3C step 5)
        foreach string gn in (keys graph_map) {
            if (gn == "@default") {
                continue;
            }
            if (!exists graph_map."@default"{gn}) {
                hash<auto!> graph_node = {"@id": gn};
                graph_map."@default"{gn} = graph_node;
            }
        }

        # Build result
        list<auto> result = ();
        hash<auto> default_graph = graph_map."@default";
        foreach string id in (sort(keys default_graph)) {
            hash<auto> node = default_graph{id};

            # Attach named graph content
            bool has_named_graph = exists graph_map{id} && id != "@default";
            if (has_named_graph) {
                list<auto> graph_nodes = ();
                hash<auto> named = graph_map{id};
                foreach string gid in (sort(keys named)) {
                    push graph_nodes, named{gid};
                }
                node."@graph" = graph_nodes;
            }

            # Skip nodes that only have @id and no named graph
            if (node.size() == 1 && exists node."@id" && !has_named_graph) {
                continue;
            }

            push result, node;
        }

        return result;
    }

    #! Reconstructs RDF lists (rdf:first/rdf:rest chains) into \@list objects
    private reconstructLists(reference<auto> graph,
            hash<string, bool> shared_bnodes = {}) {
        if (graph.typeCode() != NT_HASH) {
            return;
        }
        hash<auto> graph_hash = graph;

        # Find all list nodes: nodes that are used as rdf:rest values
        # A list node has rdf:first and rdf:rest properties
        hash<string, bool> referenced_as_rest = {};

        foreach string node_id in (keys graph_hash) {
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;
            # Check if this node references other nodes via rdf:rest
            if (exists nh{RDF_REST}) {
                list<auto> rest_vals = nh{RDF_REST}.typeCode() == NT_LIST
                    ? nh{RDF_REST} : (nh{RDF_REST},);
                foreach auto rv in (rest_vals) {
                    if (rv.typeCode() == NT_HASH && exists rv."@id") {
                        string rest_id = rv."@id";
                        if (rest_id != RDF_NIL) {
                            referenced_as_rest{rest_id} = True;
                        }
                    }
                }
            }
        }

        # Find list heads: nodes with rdf:first+rdf:rest that are NOT referenced
        # as rdf:rest from another node, AND only have @id, rdf:first, rdf:rest, @type(rdf:List)
        hash<string, bool> list_nodes = {};

        foreach string node_id in (keys graph_hash) {
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;
            if (!exists nh{RDF_FIRST} || !exists nh{RDF_REST}) {
                continue;
            }
            # Only blank nodes can be list nodes — named IRI nodes keep raw triples
            # Also skip blank nodes shared across graphs
            if (!isBlankNodeIdentifier(node_id) || shared_bnodes{node_id}) {
                continue;
            }
            # Check this is a pure list node: only @id, rdf:first, rdf:rest,
            # and optionally @type containing only rdf:List
            bool is_pure = True;
            foreach string k in (keys nh) {
                if (k != "@id" && k != RDF_FIRST && k != RDF_REST && k != "@type") {
                    is_pure = False;
                    break;
                }
            }
            # If @type is present, verify it only contains rdf:List
            if (is_pure && exists nh."@type") {
                list<auto> type_vals = nh."@type".typeCode() == NT_LIST
                    ? nh."@type" : (nh."@type",);
                foreach auto tv in (type_vals) {
                    if (tv != RDF_NS + "List") {
                        is_pure = False;
                        break;
                    }
                }
            }
            if (is_pure) {
                # Check single values for rdf:first and rdf:rest
                auto first_v = nh{RDF_FIRST};
                auto rest_v = nh{RDF_REST};
                if (first_v.typeCode() == NT_LIST && first_v.size() != 1) {
                    is_pure = False;
                }
                if (rest_v.typeCode() == NT_LIST && rest_v.size() != 1) {
                    is_pure = False;
                }
            }
            if (is_pure) {
                list_nodes{node_id} = True;
            }
        }

        # Validate list chains: only mark as list nodes if the chain ends at rdf:nil
        hash<string, bool> valid_lists = {};
        foreach string ln in (keys list_nodes) {
            if (!list_nodes{ln}) {
                continue;
            }
            # Follow the chain and verify it ends at rdf:nil
            string cur = ln;
            hash<string, bool> chain_visited = {};
            bool ends_at_nil = False;
            while (exists cur && cur != RDF_NIL) {
                if (chain_visited{cur} || !list_nodes{cur}) {
                    break;
                }
                chain_visited{cur} = True;
                auto cn = graph_hash{cur};
                if (cn.typeCode() != NT_HASH) {
                    break;
                }
                hash<auto> cnh = cn;
                if (exists cnh{RDF_REST} && cnh{RDF_REST}.typeCode() == NT_LIST
                        && cnh{RDF_REST}.size() > 0) {
                    auto rv = cnh{RDF_REST}[0];
                    if (rv.typeCode() == NT_HASH && exists rv."@id") {
                        cur = rv."@id";
                        if (cur == RDF_NIL) {
                            ends_at_nil = True;
                        }
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            if (ends_at_nil) {
                # Mark all nodes in this chain as valid
                foreach string cid in (keys chain_visited) {
                    valid_lists{cid} = True;
                }
            }
        }
        # Replace list_nodes with only validated chains
        list_nodes = valid_lists;

        # For each property in each node, check if its value references a list node
        foreach string node_id in (keys graph_hash) {
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;

            foreach string prop in (keys nh) {
                if (prop == "@id" || prop == "@type") {
                    continue;
                }
                if (!exists nh{prop} || nh{prop}.typeCode() != NT_LIST) {
                    continue;
                }

                list<auto> new_values = ();
                bool modified = False;
                foreach auto val in (nh{prop}) {
                    if (val.typeCode() == NT_HASH && exists val."@id") {
                        string ref_id = val."@id";
                        if (list_nodes{ref_id}) {
                            # This references a list node — extract the list
                            list<auto> list_items = extractListItems(graph_hash, ref_id);
                            push new_values, {"@list": list_items};
                            modified = True;

                            # Mark all nodes in this list chain for removal
                            string cur = ref_id;
                            hash<string, bool> mark_visited = {};
                            while (exists cur && cur != RDF_NIL && list_nodes{cur}
                                    && !mark_visited{cur}) {
                                mark_visited{cur} = True;
                                list_nodes{cur} = False;  # mark as consumed
                                auto cur_node = graph_hash{cur};
                                if (cur_node.typeCode() != NT_HASH) {
                                    break;
                                }
                                hash<auto> cn = cur_node;
                                if (exists cn{RDF_REST} && cn{RDF_REST}.typeCode() == NT_LIST
                                        && cn{RDF_REST}.size() > 0) {
                                    auto rv = cn{RDF_REST}[0];
                                    if (rv.typeCode() == NT_HASH && exists rv."@id") {
                                        cur = rv."@id";
                                    } else {
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }
                            continue;
                        }
                    }
                    push new_values, val;
                }
                if (modified) {
                    graph_hash{node_id}{prop} = new_values;
                }
            }
        }

        # Remove consumed list nodes from the graph
        foreach string list_id in (keys list_nodes) {
            if (list_nodes{list_id} == False) {
                # consumed list node - remove
                remove graph_hash{list_id};
            }
        }

        # Convert rdf:nil references to empty @list objects
        foreach string node_id in (keys graph_hash) {
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;
            foreach string prop in (keys nh) {
                if (prop == "@id" || prop == "@type") {
                    continue;
                }
                if (nh{prop}.typeCode() != NT_LIST) {
                    continue;
                }
                list<auto> new_values = ();
                bool modified = False;
                foreach auto val in (nh{prop}) {
                    if (val.typeCode() == NT_HASH && val."@id" == RDF_NIL) {
                        push new_values, {"@list": ()};
                        modified = True;
                    } else {
                        push new_values, val;
                    }
                }
                if (modified) {
                    graph_hash{node_id}{prop} = new_values;
                }
            }
        }

        graph = graph_hash;
    }

    #! Reconstructs compound literals (rdf:value + rdf:direction) in a graph
    private reconstructCompoundLiterals(reference<auto> graph) {
        if (graph.typeCode() != NT_HASH) {
            return;
        }
        hash<auto> graph_hash = graph;

        # Find compound literal nodes: blank nodes with rdf:value + rdf:direction
        hash<string, bool> cl_nodes = {};
        foreach string node_id in (keys graph_hash) {
            if (!isBlankNodeIdentifier(node_id)) {
                continue;
            }
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;
            if (exists nh{RDF_NS + "value"} && exists nh{RDF_NS + "direction"}) {
                # Check it's a pure compound literal: only @id, rdf:value, rdf:direction, rdf:language
                bool is_cl = True;
                foreach string k in (keys nh) {
                    if (k != "@id" && k != (RDF_NS + "value")
                            && k != (RDF_NS + "direction")
                            && k != (RDF_NS + "language")) {
                        is_cl = False;
                        break;
                    }
                }
                if (is_cl) {
                    cl_nodes{node_id} = True;
                }
            }
        }

        # Replace references to compound literal nodes with value objects
        foreach string node_id in (keys graph_hash) {
            auto node = graph_hash{node_id};
            if (node.typeCode() != NT_HASH) {
                continue;
            }
            hash<auto> nh = node;

            foreach string prop in (keys nh) {
                if (prop == "@id" || prop == "@type") {
                    continue;
                }
                if (nh{prop}.typeCode() != NT_LIST) {
                    continue;
                }

                list<auto> new_values = ();
                bool modified = False;
                foreach auto val in (nh{prop}) {
                    if (val.typeCode() == NT_HASH && exists val."@id") {
                        string ref_id = val."@id";
                        if (cl_nodes{ref_id}) {
                            # Build value object from compound literal
                            hash<auto> cl = graph_hash{ref_id};
                            hash<auto!> value_obj = {};
                            # Get rdf:value
                            auto rdf_val = cl{RDF_NS + "value"};
                            if (rdf_val.typeCode() == NT_LIST && rdf_val.size() > 0) {
                                auto first = rdf_val[0];
                                if (first.typeCode() == NT_HASH && first.hasKey("@value")) {
                                    value_obj."@value" = first."@value";
                                }
                            }
                            # Get rdf:direction
                            auto rdf_dir = cl{RDF_NS + "direction"};
                            if (rdf_dir.typeCode() == NT_LIST && rdf_dir.size() > 0) {
                                auto first = rdf_dir[0];
                                if (first.typeCode() == NT_HASH && first.hasKey("@value")) {
                                    value_obj."@direction" = first."@value";
                                }
                            }
                            # Get rdf:language (optional)
                            auto rdf_lang = cl{RDF_NS + "language"};
                            if (exists rdf_lang && rdf_lang.typeCode() == NT_LIST
                                    && rdf_lang.size() > 0) {
                                auto first = rdf_lang[0];
                                if (first.typeCode() == NT_HASH && first.hasKey("@value")) {
                                    value_obj."@language" = first."@value";
                                }
                            }
                            push new_values, value_obj;
                            modified = True;
                            continue;
                        }
                    }
                    push new_values, val;
                }
                if (modified) {
                    graph_hash{node_id}{prop} = new_values;
                }
            }
        }

        # Remove consumed compound literal nodes from the graph
        foreach string cl_id in (keys cl_nodes) {
            remove graph_hash{cl_id};
        }

        graph = graph_hash;
    }

    #! Extracts list items from a rdf:first/rdf:rest chain
    private list<auto> extractListItems(hash<auto> graph, string head_id) {
        list<auto> items = ();
        string cur = head_id;
        hash<string, bool> visited = {};

        while (exists cur && cur != RDF_NIL) {
            # Cycle detection
            if (visited{cur}) {
                break;
            }
            visited{cur} = True;

            auto node = graph{cur};
            if (node.typeCode() != NT_HASH) {
                break;
            }
            hash<auto> nh = node;
            if (exists nh{RDF_FIRST}) {
                list<auto> first_vals = nh{RDF_FIRST}.typeCode() == NT_LIST
                    ? nh{RDF_FIRST} : (nh{RDF_FIRST},);
                if (first_vals.size() > 0) {
                    auto first_item = first_vals[0];
                    # Check if first value is rdf:nil → empty nested @list
                    if (first_item.typeCode() == NT_HASH
                            && first_item."@id" == RDF_NIL) {
                        push items, {"@list": ()};
                    }
                    # Check if first value references another list node → nested @list
                    else if (first_item.typeCode() == NT_HASH
                            && exists first_item."@id"
                            && isBlankNodeIdentifier(first_item."@id")
                            && exists graph{first_item."@id"}
                            && graph{first_item."@id"}.typeCode() == NT_HASH
                            && exists ((graph{first_item."@id"})){RDF_FIRST}
                            && exists ((graph{first_item."@id"})){RDF_REST}) {
                        string inner_id = first_item."@id";
                        list<auto> inner_items = extractListItems(graph, inner_id);
                        push items, {"@list": inner_items};
                    } else {
                        push items, first_item;
                    }
                }
            }
            # Follow rdf:rest
            if (exists nh{RDF_REST} && nh{RDF_REST}.typeCode() == NT_LIST
                    && nh{RDF_REST}.size() > 0) {
                auto rest = nh{RDF_REST}[0];
                if (rest.typeCode() == NT_HASH && exists rest."@id") {
                    cur = rest."@id";
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        return items;
    }

    #! Serializes an RDF dataset to N-Quads format
    /** @param dataset the RDF dataset
        @return the N-Quads string

        @since json 1.12
    */
    static string toNQuads(hash<RdfDataset> dataset) {
        list<string> lines = ();

        # Default graph quads
        foreach hash<RdfQuad> quad in (dataset.defaultGraph) {
            push lines, serializeQuad(quad);
        }

        # Named graph quads
        foreach string graph_name in (sort(keys dataset.namedGraphs)) {
            foreach hash<RdfQuad> quad in (dataset.namedGraphs{graph_name}) {
                push lines, serializeQuad(quad);
            }
        }

        if (lines.empty()) {
            return "";
        }
        return (foldl $1 + "\n" + $2, sort(lines)) + "\n";
    }

    #! Parses N-Quads format to an RDF dataset
    /** @param nquads the N-Quads string
        @return the RDF dataset

        @since json 1.12
    */
    static hash<RdfDataset> fromNQuads(string nquads) {
        hash<RdfDataset> dataset();
        list<string> lines = nquads.split("\n");

        int line_num = 0;
        foreach string line in (lines) {
            ++line_num;
            line = trim(line);
            if (line == "" || line =~ /^#/) {
                continue;
            }

            hash<RdfQuad> quad = parseNQuadLine(line, line_num);
            if (exists quad.graphName) {
                string gn = quad.graphName.value;
                if (!exists dataset.namedGraphs{gn}) {
                    dataset.namedGraphs{gn} = ();
                }
                push dataset.namedGraphs{gn}, quad;
            } else {
                push dataset.defaultGraph, quad;
            }
        }

        return dataset;
    }

    #! Checks if an IRI is well-formed (no whitespace, valid scheme)
    private static bool isWellFormedIriForRdf(string iri) {
        if (isBlankNodeIdentifier(iri)) {
            return True;
        }
        # IRIs must not contain whitespace or control characters
        if (iri =~ /[\x00-\x20<>"{}|\\^`]/) {
            return False;
        }
        # IRIs must not contain multiple fragments (##)
        int first_hash = index(iri, "#");
        if (first_hash >= 0) {
            int second_hash = index(iri, "#", first_hash + 1);
            if (second_hash >= 0) {
                return False;
            }
        }
        return isAbsoluteIri(iri);
    }

    #! Processes a node object into RDF quads
    private processNodeToRdf(reference<hash<RdfDataset>> dataset, hash<auto> node,
            string graph_name) {
        string subject_id = node."@id";
        if (!exists subject_id) {
            return;
        }

        # Skip nodes with malformed IRIs
        if (!isWellFormedIriForRdf(subject_id)) {
            return;
        }

        hash<RdfTerm> subject = makeTermFromId(subject_id);

        foreach string property in (sort(keys node)) {
            if (property == "@id") {
                continue;
            }

            if (property == "@type") {
                list<auto> types = node."@type".typeCode() == NT_LIST
                    ? node."@type" : (node."@type",);
                foreach auto t in (types) {
                    if (t.typeCode() == NT_STRING && isWellFormedIriForRdf(t)) {
                        addQuad(\dataset, subject,
                            <RdfTerm>{"termType": RdfTermType::Iri, "value": RDF_TYPE},
                            makeTermFromId(t),
                            graph_name);
                    }
                }
                continue;
            }

            if (property == "@graph" || property == "@index") {
                continue;
            }

            # Process @reverse: generate triples with swapped subject/object
            if (property == "@reverse") {
                if (node."@reverse".typeCode() == NT_HASH) {
                    hash<auto> rev = node."@reverse";
                    foreach string rev_prop in (sort(keys rev)) {
                        if (!isAbsoluteIri(rev_prop)) {
                            continue;
                        }
                        hash<RdfTerm> rev_predicate = <RdfTerm>{
                            "termType": RdfTermType::Iri,
                            "value": rev_prop,
                        };
                        list<auto> rev_values = rev{rev_prop}.typeCode() == NT_LIST
                            ? rev{rev_prop} : (rev{rev_prop},);
                        foreach auto rv in (rev_values) {
                            if (rv.typeCode() == NT_HASH && exists rv."@id") {
                                # Swap: the referenced node is the SUBJECT, current node is OBJECT
                                hash<RdfTerm> rev_subject = makeTermFromId(
                                    rv."@id");
                                addQuad(\dataset, rev_subject, rev_predicate, subject,
                                    graph_name);
                            }
                        }
                    }
                }
                continue;
            }

            # Skip non-IRI properties
            if (!isAbsoluteIri(property) && !isBlankNodeIdentifier(property)) {
                continue;
            }

            # Skip blank node predicates unless produceGeneralizedRdf is true
            if (isBlankNodeIdentifier(property) && !options.produceGeneralizedRdf) {
                continue;
            }

            # Skip malformed predicate IRIs
            if (!isWellFormedIriForRdf(property)) {
                continue;
            }

            hash<RdfTerm> predicate = makeTermFromId(property);

            list<auto> values = node{property}.typeCode() == NT_LIST
                ? node{property} : (node{property},);

            foreach auto val in (values) {
                if (val.typeCode() != NT_HASH) {
                    continue;
                }
                hash<auto> val_hash = val;

                if (isValueObject(val_hash)) {
                    # Skip values with malformed language tags
                    if (exists val_hash."@language"
                            && val_hash."@language" =~ /[\x00-\x20]/) {
                        continue;
                    }
                    # Handle compound-literal rdfDirection
                    if (exists val_hash."@direction"
                            && options.rdfDirection == "compound-literal") {
                        # Create blank node with rdf:value, rdf:language, rdf:direction
                        string cl_id = blankNodeGenerator.generate();
                        hash<RdfTerm> cl_node = <RdfTerm>{
                            "termType": RdfTermType::BlankNode,
                            "value": cl_id,
                        };
                        addQuad(\dataset, subject, predicate, cl_node, graph_name);
                        # rdf:value
                        addQuad(\dataset, cl_node,
                            <RdfTerm>{"termType": RdfTermType::Iri,
                                "value": RDF_NS + "value"},
                            <RdfTerm>{"termType": RdfTermType::Literal,
                                "value": string(val_hash."@value"),
                                "datatype": XSD_STRING},
                            graph_name);
                        # rdf:direction
                        addQuad(\dataset, cl_node,
                            <RdfTerm>{"termType": RdfTermType::Iri,
                                "value": RDF_NS + "direction"},
                            <RdfTerm>{"termType": RdfTermType::Literal,
                                "value": val_hash."@direction",
                                "datatype": XSD_STRING},
                            graph_name);
                        # rdf:language (if present)
                        if (exists val_hash."@language") {
                            addQuad(\dataset, cl_node,
                                <RdfTerm>{"termType": RdfTermType::Iri,
                                    "value": RDF_NS + "language"},
                                <RdfTerm>{"termType": RdfTermType::Literal,
                                    "value": val_hash."@language",
                                    "datatype": XSD_STRING},
                                graph_name);
                        }
                    } else {
                        hash<RdfTerm> object = valueToRdfTerm(val_hash);
                        addQuad(\dataset, subject, predicate, object, graph_name);
                    }
                } else if (isListObject(val_hash)) {
                    hash<RdfTerm> list_head = listToRdf(\dataset, val_hash."@list", graph_name);
                    addQuad(\dataset, subject, predicate, list_head, graph_name);
                } else if (exists val_hash."@id") {
                    # Skip malformed object IRIs
                    if (!isWellFormedIriForRdf(val_hash."@id")) {
                        continue;
                    }
                    hash<RdfTerm> object = makeTermFromId(val_hash."@id");
                    addQuad(\dataset, subject, predicate, object, graph_name);
                }
            }
        }
    }

    #! Converts a value object to an RDF literal term
    private hash<RdfTerm> valueToRdfTerm(hash<auto> value) {
        auto val = value."@value";
        # @type may be a string or (from expansion) a list - handle both
        *string type;
        if (value."@type".typeCode() == NT_LIST) {
            type = value."@type"[0];
        } else {
            type = value."@type";
        }
        *string lang = value."@language";

        if (type == "@json") {
            # Fix surrogate pairs from Qore's JSON parser before canonicalization
            auto fixed_val = fixSurrogatePairs(val);
            # Canonical JSON: sorted keys; handle null
            string json_val = serializeJsonLiteral(canonicalizeJson(fixed_val));
            return <RdfTerm>{
                "termType": RdfTermType::Literal,
                "value": json_val,
                "datatype": RDF_JSON,
            };
        }

        if (exists lang) {
            *string dir = value."@direction";
            if (exists dir && options.rdfDirection == "i18n-datatype") {
                # Encode language + direction in datatype IRI
                string dt = RDF_DIRECTION + lang + "_" + dir;
                return <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": string(val),
                    "datatype": dt,
                };
            }
            return <RdfTerm>{
                "termType": RdfTermType::Literal,
                "value": string(val),
                "datatype": RDF_LANGSTRING,
                "language": lang,
            };
        }

        # Handle @direction without @language
        if (exists value."@direction" && options.rdfDirection == "i18n-datatype") {
            string dt = RDF_DIRECTION + "_" + value."@direction";
            return <RdfTerm>{
                "termType": RdfTermType::Literal,
                "value": string(val),
                "datatype": dt,
            };
        }

        if (exists type) {
            # Use canonical format for known types and native values
            string str_val;
            if (type == XSD_BOOLEAN && val.typeCode() == NT_BOOLEAN) {
                str_val = val ? "true" : "false";
            } else if (type == XSD_DOUBLE) {
                # XSD double type: always use canonical double format
                str_val = canonicalDouble(float(val));
            } else if (val.typeCode() == NT_FLOAT || val.typeCode() == NT_NUMBER) {
                # Native float values use canonical double format
                str_val = canonicalDouble(float(val));
            } else if (type == XSD_INTEGER && val.typeCode() == NT_INT) {
                str_val = string(val);
            } else if (val.typeCode() == NT_BOOLEAN) {
                str_val = val ? "true" : "false";
            } else {
                str_val = string(val);
            }
            return <RdfTerm>{
                "termType": RdfTermType::Literal,
                "value": str_val,
                "datatype": type,
            };
        }

        # Infer type from native value
        switch (val.typeCode()) {
            case NT_BOOLEAN:
                return <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": val ? "true" : "false",
                    "datatype": XSD_BOOLEAN,
                };
            case NT_INT:
                return <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": string(val),
                    "datatype": XSD_INTEGER,
                };
            case NT_FLOAT:
            case NT_NUMBER: {
                # If the float value is exactly representable as an integer, use integer
                float fv = float(val);
                if (fv == float(int(fv)) && fv >= -1e20 && fv <= 1e20) {
                    return <RdfTerm>{
                        "termType": RdfTermType::Literal,
                        "value": string(int(fv)),
                        "datatype": XSD_INTEGER,
                    };
                }
                return <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": canonicalDouble(fv),
                    "datatype": XSD_DOUBLE,
                };
            }
            default:
                return <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": string(val),
                    "datatype": XSD_STRING,
                };
        }
    }

    #! Converts a JSON-LD \@list to RDF list structure
    private hash<RdfTerm> listToRdf(reference<hash<RdfDataset>> dataset, list<auto> items,
            string graph_name) {
        if (items.empty()) {
            return <RdfTerm>{"termType": RdfTermType::Iri, "value": RDF_NIL};
        }

        list<string> bnodes = ();
        for (int i = 0; i < items.size(); ++i) {
            push bnodes, blankNodeGenerator.generate();
        }

        hash<RdfTerm> first_pred = <RdfTerm>{"termType": RdfTermType::Iri, "value": RDF_FIRST};
        hash<RdfTerm> rest_pred = <RdfTerm>{"termType": RdfTermType::Iri, "value": RDF_REST};
        hash<RdfTerm> nil_term = <RdfTerm>{"termType": RdfTermType::Iri, "value": RDF_NIL};

        for (int i = 0; i < items.size(); ++i) {
            hash<RdfTerm> bnode = <RdfTerm>{
                "termType": RdfTermType::BlankNode,
                "value": bnodes[i],
            };

            auto item = items[i];
            hash<RdfTerm> object;
            if (item.typeCode() == NT_HASH) {
                hash<auto> item_hash = item;
                if (isValueObject(item_hash)) {
                    object = valueToRdfTerm(item_hash);
                } else if (isListObject(item_hash)) {
                    # Nested list: recursively convert to RDF list
                    object = listToRdf(\dataset, item_hash."@list", graph_name);
                } else if (exists item_hash."@id") {
                    if (!isWellFormedIriForRdf(item_hash."@id")) {
                        # Skip rdf:first but still add rdf:rest
                        hash<RdfTerm> rest_val;
                        if (i + 1 < items.size()) {
                            rest_val = <RdfTerm>{
                                "termType": RdfTermType::BlankNode,
                                "value": bnodes[i + 1],
                            };
                        } else {
                            rest_val = nil_term;
                        }
                        addQuad(\dataset, bnode, rest_pred, rest_val, graph_name);
                        continue;
                    }
                    object = makeTermFromId(item_hash."@id");
                } else {
                    continue;
                }
            } else {
                object = <RdfTerm>{
                    "termType": RdfTermType::Literal,
                    "value": string(item),
                    "datatype": XSD_STRING,
                };
            }

            addQuad(\dataset, bnode, first_pred, object, graph_name);

            hash<RdfTerm> rest_val;
            if (i + 1 < items.size()) {
                rest_val = <RdfTerm>{
                    "termType": RdfTermType::BlankNode,
                    "value": bnodes[i + 1],
                };
            } else {
                rest_val = nil_term;
            }
            addQuad(\dataset, bnode, rest_pred, rest_val, graph_name);
        }

        return <RdfTerm>{
            "termType": RdfTermType::BlankNode,
            "value": bnodes[0],
        };
    }

    #! Creates an RDF term from a node ID
    private static hash<RdfTerm> makeTermFromId(string id) {
        if (isBlankNodeIdentifier(id)) {
            return <RdfTerm>{"termType": RdfTermType::BlankNode, "value": id};
        }
        return <RdfTerm>{"termType": RdfTermType::Iri, "value": id};
    }

    #! Adds a quad to the dataset
    private static addQuad(reference<hash<RdfDataset>> dataset, hash<RdfTerm> subject,
            hash<RdfTerm> predicate, hash<RdfTerm> object, string graph_name) {
        hash<RdfQuad> quad = <RdfQuad>{
            "subject": subject,
            "predicate": predicate,
            "object": object,
        };

        if (graph_name != "@default") {
            quad.graphName = makeTermFromId(graph_name);
            if (!exists dataset.namedGraphs{graph_name}) {
                dataset.namedGraphs{graph_name} = ();
            }
            push dataset.namedGraphs{graph_name}, quad;
        } else {
            push dataset.defaultGraph, quad;
        }
    }

    #! Processes RDF quads into the JSON-LD graph map
    private processQuadsToJsonLd(list<hash<RdfQuad>> quads,
            reference<hash<auto!>> graph_map, string graph_name) {
        if (!exists graph_map{graph_name}) {
            graph_map{graph_name} = {};
        }

        foreach hash<RdfQuad> quad in (quads) {
            string subject_id = quad.subject.value;

            if (!exists graph_map{graph_name}{subject_id}) {
                hash<auto!> subject_node = {"@id": subject_id};
                graph_map{graph_name}{subject_id} = subject_node;
            }

            # Reference the node hash via a local variable for cleaner access
            reference<auto> node = \graph_map{graph_name}{subject_id};

            # rdf:type -> @type
            if (quad.predicate.value == RDF_TYPE && quad.object.termType == RdfTermType::Iri
                    && !options.useRdfType) {
                auto existing_type = node."@type";
                if (!exists existing_type) {
                    node."@type" = (quad.object.value,);
                } else if (existing_type.typeCode() == NT_LIST) {
                    if (!inlist(quad.object.value, existing_type)) {
                        list<auto> updated = existing_type;
                        push updated, quad.object.value;
                        node."@type" = updated;
                    }
                }
                continue;
            }

            # Convert object to JSON-LD value
            auto json_value;
            if (quad.object.termType == RdfTermType::Iri
                    || quad.object.termType == RdfTermType::BlankNode) {
                json_value = {"@id": quad.object.value};
            } else {
                # Literal
                json_value = rdfLiteralToValue(quad.object);
            }

            string prop = quad.predicate.value;
            auto existing = node{prop};
            if (!exists existing) {
                node{prop} = (json_value,);
            } else if (existing.typeCode() == NT_LIST) {
                # Deduplicate: check if value already exists
                bool dup = False;
                foreach auto ev in (existing) {
                    if (ev == json_value) {
                        # Type-aware: {"@value": true} != {"@value": 1}
                        if (ev.typeCode() == NT_HASH && json_value.typeCode() == NT_HASH
                                && ev.hasKey("@value") && json_value.hasKey("@value")
                                && ev."@value".typeCode() != json_value."@value".typeCode()) {
                            continue;
                        }
                        dup = True;
                        break;
                    }
                }
                if (!dup) {
                    list<auto> updated = existing;
                    push updated, json_value;
                    node{prop} = updated;
                }
            }
        }
    }

    #! Converts an RDF literal to a JSON-LD value object
    private hash<auto> rdfLiteralToValue(hash<RdfTerm> term) {
        hash<auto> result = {};

        # Handle RDF JSON
        if (term.datatype == RDF_JSON) {
            result."@value" = parse_json(term.value);
            result."@type" = "@json";
            return result;
        }

        # Handle i18n-datatype direction encoding
        if (exists term.datatype && options.rdfDirection == "i18n-datatype"
                && term.datatype.find(RDF_DIRECTION) == 0) {
            string suffix = substr(term.datatype, RDF_DIRECTION.length());
            *list<string> parts = (suffix =~ x/^(.*)_([^_]+)$/);
            if (parts) {
                result."@value" = term.value;
                if (parts[0].length() > 0) {
                    result."@language" = parts[0];
                }
                result."@direction" = parts[1];
                return result;
            }
        }

        if (options.useNativeTypes) {
            if (term.datatype == XSD_BOOLEAN) {
                # Only convert canonical xsd:boolean values: "true", "false", "1", "0"
                if (term.value == "true" || term.value == "false"
                        || term.value == "1" || term.value == "0") {
                    result."@value" = (term.value == "true" || term.value == "1") ? True : False;
                    return result;
                }
                # Non-canonical forms (True, FALSE, etc.) stay as typed string
            }
            if (term.datatype == XSD_INTEGER && term.value =~ /^-?[0-9]+$/) {
                result."@value" = int(term.value);
                return result;
            }
            if (term.datatype == XSD_DOUBLE) {
                # Check if the value can be losslessly represented as a native number
                # Values like +INF, -INF, NaN, and extreme exponents cannot
                if (term.value =~ /^-?[0-9]*\.?[0-9]+([eE][+-]?[0-9]+)?$/
                        && term.value != "+INF" && term.value != "-INF" && term.value != "NaN") {
                    float fval = float(term.value);
                    # Check for overflow to max double or underflow
                    if (fval != @inf@ && fval != -@inf@
                            && !(fval == 0.0 && term.value !~ /^-?0(\.0+)?$/)) {
                        # Verify the value can be represented without precision loss
                        # by checking the exponent isn't absurdly large
                        if (term.value !~ /[eE][+-]?[0-9]{10,}/) {
                            result."@value" = fval;
                            return result;
                        }
                    }
                }
                # Non-representable values stay as typed string
            }
        }

        result."@value" = term.value;

        if (term.datatype == RDF_LANGSTRING && exists term.language) {
            result."@language" = term.language;
        } else if (exists term.datatype && term.datatype != XSD_STRING) {
            result."@type" = term.datatype;
        }

        return result;
    }

    #! Serializes a single quad to N-Quads format
    private static string serializeQuad(hash<RdfQuad> quad) {
        string result = serializeTerm(quad.subject) + " "
            + serializeTerm(quad.predicate) + " "
            + serializeTerm(quad.object);

        if (exists quad.graphName) {
            result += " " + serializeTerm(quad.graphName);
        }

        return result + " .";
    }

    #! Serializes an RDF term for N-Quads
    private static string serializeTerm(hash<RdfTerm> term) {
        switch (term.termType) {
            case RdfTermType::Iri:
                return "<" + escapeNQuadsIri(term.value) + ">";
            case RdfTermType::BlankNode:
                return term.value;
            case RdfTermType::Literal: {
                string result = "\"" + escapeNQuadsString(term.value) + "\"";
                if (exists term.language) {
                    result += "@" + term.language;
                } else if (exists term.datatype && term.datatype != XSD_STRING) {
                    result += "^^<" + escapeNQuadsIri(term.datatype) + ">";
                }
                return result;
            }
        }
        return "";
    }

    #! Escapes a string for N-Quads
    private static string escapeNQuadsString(string s) {
        string result;
        for (int i = 0; i < s.length(); ++i) {
            string ch = s[i];
            int cp = ord(ch);
            switch (cp) {
                case 0x08:
                    result += "\\b";
                    break;
                case 0x09:
                    result += "\\t";
                    break;
                case 0x0a:
                    result += "\\n";
                    break;
                case 0x0c:
                    result += "\\f";
                    break;
                case 0x0d:
                    result += "\\r";
                    break;
                case 0x22:
                    result += "\\\"";
                    break;
                case 0x5c:
                    result += "\\\\";
                    break;
                default:
                    if (cp < 0x20) {
                        result += sprintf("\\u%04X", cp);
                    } else {
                        result += ch;
                    }
            }
        }
        return result;
    }

    #! Escapes an IRI for N-Quads
    private static string escapeNQuadsIri(string s) {
        return replace(replace(s, ">", "%3E"), "<", "%3C");
    }

    #! Parses a single N-Quads line
    private static hash<RdfQuad> parseNQuadLine(string line, int line_num = 0) {
        # Remove trailing " ."
        if (line =~ /\s*\.\s*$/) {
            *list<string> m = (line =~ x/^(.*\S)\s*\.\s*$/);
            if (m) {
                line = m[0];
            }
        }

        list<string> parts = ();
        string remaining = trim(line);

        # Parse up to 4 terms
        for (int i = 0; i < 4 && remaining.length() > 0; ++i) {
            remaining = trim(remaining);
            if (remaining.length() == 0) {
                break;
            }

            hash<auto> parsed = parseNQuadTerm(remaining);
            push parts, parsed.term_str;
            remaining = trim(parsed.rest);
        }

        if (parts.size() < 3) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("invalid N-Quad line %d: %y", line_num, line));
        }

        hash<RdfQuad> quad = <RdfQuad>{
            "subject": parseTermString(parts[0]),
            "predicate": parseTermString(parts[1]),
            "object": parseTermString(parts[2]),
        };

        if (parts.size() >= 4) {
            quad.graphName = parseTermString(parts[3]);
        }

        return quad;
    }

    #! Parses a single N-Quad term from the beginning of a string
    private static hash<auto> parseNQuadTerm(string s) {
        if (s =~ /^</) {
            # IRI
            *list<string> m = (s =~ x/^<([^>]*)>(.*)/s);
            if (m) {
                return {"term_str": "<" + m[0] + ">", "rest": m[1]};
            }
        } else if (s =~ /^_:/) {
            # Blank node
            *list<string> m = (s =~ x/^(_:\S+)(.*)/s);
            if (m) {
                return {"term_str": m[0], "rest": m[1]};
            }
        } else if (s =~ /^"/) {
            # Literal - find matching closing quote (handling escapes)
            int pos = 1;
            while (pos < s.length()) {
                if (s[pos] == "\\") {
                    pos += 2;
                } else if (s[pos] == "\"") {
                    break;
                } else {
                    ++pos;
                }
            }
            string lit = substr(s, 0, pos + 1);
            string rest = substr(s, pos + 1);

            # Check for language tag or datatype
            if (rest =~ /^@/) {
                *list<string> m = (rest =~ x/^(@[a-zA-Z0-9-]+)(.*)/s);
                if (m) {
                    lit += m[0];
                    rest = m[1];
                }
            } else if (rest =~ /^\^\^/) {
                *list<string> m = (rest =~ x/^(\^\^<[^>]*>)(.*)/s);
                if (m) {
                    lit += m[0];
                    rest = m[1];
                }
            }

            return {"term_str": lit, "rest": rest};
        }

        throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
            sprintf("cannot parse N-Quad term from: %y", substr(s, 0, 50)));
        # unreachable
        return {};
    }

    #! Parses a serialized N-Quad term string into an RdfTerm
    private static hash<RdfTerm> parseTermString(string s) {
        if (s =~ /^<(.*)>$/) {
            *list<string> m = (s =~ x/^<(.*)>$/);
            return <RdfTerm>{"termType": RdfTermType::Iri, "value": m[0]};
        }

        if (s =~ /^_:/) {
            return <RdfTerm>{"termType": RdfTermType::BlankNode, "value": s};
        }

        # Literal
        hash<RdfTerm> result = <RdfTerm>{"termType": RdfTermType::Literal};

        if (s =~ /^"/) {
            # Extract value
            *list<string> m = (s =~ x/^"((?:[^"\\]|\\.)*)"/s);
            if (m) {
                result.value = unescapeNQuadsString(m[0]);
            }

            # Check for language tag
            *list<string> lang_m = (s =~ x/@([a-zA-Z0-9-]+)$/);
            if (lang_m) {
                result.language = lang_m[0];
                result.datatype = RDF_LANGSTRING;
            } else {
                # Check for datatype
                *list<string> dt_m = (s =~ x/\^\^<([^>]*)>$/);
                if (dt_m) {
                    result.datatype = dt_m[0];
                } else {
                    result.datatype = XSD_STRING;
                }
            }
        }

        return result;
    }

    #! Produces canonical JSON (sorted keys) for RDF JSON literals
    private static auto canonicalizeJson(auto val) {
        switch (val.typeCode()) {
            case NT_HASH: {
                hash<auto> result = {};
                # Sort keys by code-point order (empty string first)
                list<string> sorted_keys = sort(keys val,
                    int sub (string a, string b) {
                        if (a == b) { return 0; }
                        if (a.empty()) { return -1; }
                        if (b.empty()) { return 1; }
                        return a < b ? -1 : 1;
                    });
                foreach string k in (sorted_keys) {
                    result{k} = canonicalizeJson(val{k});
                }
                return result;
            }
            case NT_LIST: {
                return map canonicalizeJson($1), val;
            }
            default:
                return val;
        }
    }

    #! Serializes a JSON value with canonical float precision
    /** Uses shortest-representation for floats per JSON Canonicalization Scheme.
    */
    private static string serializeJsonLiteral(auto val) {
        switch (val.typeCode()) {
            case NT_NOTHING:
                return "null";
            case NT_BOOLEAN:
                return val ? "true" : "false";
            case NT_INT:
                return string(val);
            case NT_FLOAT:
            case NT_NUMBER: {
                # Use shortest representation per JCS/ECMAScript Number.toString()
                float fv = float(val);
                # Try integer form (including zero)
                if (fv == float(int(fv)) && fv >= -1e20 && fv <= 1e20) {
                    return string(int(fv));
                }
                # Find shortest representation that round-trips correctly
                for (int prec = 1; prec <= 17; ++prec) {
                    string fmt = sprintf("%%.%dg", prec);
                    string s = sprintf(fmt, fv);
                    if (float(s) == fv) {
                        return s;
                    }
                }
                return sprintf("%.17g", fv);
            }
            case NT_STRING: {
                # Use make_json for proper escaping
                return make_json(val);
            }
            case NT_HASH: {
                auto canonical = canonicalizeJson(val);
                list<string> parts = ();
                foreach string k in (keys canonical) {
                    push parts, serializeJsonLiteral(k) + ":" + serializeJsonLiteral(canonical{k});
                }
                return "{" + (join(",", parts)) + "}";
            }
            case NT_LIST: {
                list<string> parts = ();
                foreach auto item in (val) {
                    push parts, serializeJsonLiteral(item);
                }
                return "[" + (join(",", parts)) + "]";
            }
            default:
                return make_json(val);
        }
    }

    #! Produces canonical XSD double representation
    /** Per XSD spec: uses "E" (uppercase), strips trailing zeros from mantissa,
        uses E0 for values without exponent, handles special values.
    */
    private static string canonicalDouble(float val) {
        # Handle special values
        if (val != val) {
            return "NaN";
        }
        if (val == @inf@) {
            return "INF";
        }
        if (val == -@inf@) {
            return "-INF";
        }

        # Use %E format for canonical form
        string s = sprintf("%.15E", val);

        # Split mantissa and exponent
        *list<string> parts = (s =~ x/^(-?[0-9]+\.[0-9]+)E([+-]?[0-9]+)$/);
        if (!parts) {
            return s;
        }

        string mantissa = parts[0];
        string exp_str = parts[1];

        # Strip trailing zeros from mantissa (keep at least one digit after .)
        mantissa = regex_subst(mantissa, "0+$", "");
        if (mantissa =~ /\.$/) {
            mantissa += "0";
        }

        # Normalize exponent: remove leading zeros and unnecessary +
        int exp_val = int(exp_str);
        return mantissa + "E" + string(exp_val);
    }

    #! Unescapes an N-Quads string
    private static string unescapeNQuadsString(string s) {
        string result;
        for (int i = 0; i < s.length(); ++i) {
            string ch = s[i];
            if (ch != "\\") {
                result += ch;
                continue;
            }

            if (i + 1 >= s.length()) {
                throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                    "invalid trailing escape in N-Quads literal");
            }

            string esc = s[++i];
            switch (esc) {
                case "b":
                    result += "\b";
                    break;
                case "t":
                    result += "\t";
                    break;
                case "n":
                    result += "\n";
                    break;
                case "f":
                    result += "\f";
                    break;
                case "r":
                    result += "\r";
                    break;
                case "\"":
                    result += "\"";
                    break;
                case "'":
                    result += "'";
                    break;
                case "\\":
                    result += "\\";
                    break;
                case "u": {
                    if (i + 4 >= s.length()) {
                        throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                            "short \\u escape in N-Quads literal");
                    }
                    string hex = substr(s, i + 1, 4);
                    if (hex !~ /^[0-9A-Fa-f]{4}$/) {
                        throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                            sprintf("invalid \\u escape in N-Quads literal: %y", hex));
                    }
                    result += codepointToUtf8String(int(hex, 16));
                    i += 4;
                    break;
                }
                case "U": {
                    if (i + 8 >= s.length()) {
                        throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                            "short \\U escape in N-Quads literal");
                    }
                    string hex = substr(s, i + 1, 8);
                    if (hex !~ /^[0-9A-Fa-f]{8}$/) {
                        throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                            sprintf("invalid \\U escape in N-Quads literal: %y", hex));
                    }
                    result += codepointToUtf8String(int(hex, 16));
                    i += 8;
                    break;
                }
                default:
                    throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                        sprintf("invalid escape in N-Quads literal: \\%s", esc));
            }
        }

        return result;
    }

    #! Encodes a Unicode codepoint as a UTF-8 string
    private static string codepointToUtf8String(int cp) {
        if (cp < 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("invalid Unicode codepoint in N-Quads literal: U+%04X", cp));
        }

        binary encoded = binary();
        if (cp <= 0x7f) {
            encoded += chr(cp);
        } else if (cp <= 0x7ff) {
            encoded += chr(0xc0 | (cp >> 6));
            encoded += chr(0x80 | (cp & 0x3f));
        } else if (cp <= 0xffff) {
            encoded += chr(0xe0 | (cp >> 12));
            encoded += chr(0x80 | ((cp >> 6) & 0x3f));
            encoded += chr(0x80 | (cp & 0x3f));
        } else {
            encoded += chr(0xf0 | (cp >> 18));
            encoded += chr(0x80 | ((cp >> 12) & 0x3f));
            encoded += chr(0x80 | ((cp >> 6) & 0x3f));
            encoded += chr(0x80 | (cp & 0x3f));
        }
        return encoded.toString("utf-8");
    }
}

} # namespace JsonLd
