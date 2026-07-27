# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdCompaction.qc @brief JSON-LD Compaction Algorithm (W3C 4.5)

/*  JsonLdCompaction.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! JSON-LD Compaction Algorithm
/** Implements the Compaction Algorithm (W3C Section 4.5).
    Compacts an expanded JSON-LD document using a context.

    @since json 1.12
*/
public class JsonLdCompaction {
    private {
        #! Context processor
        JsonLdContext contextProcessor;

        #! Processor options
        hash<JsonLdOptions> options;
    }

    #! Creates a new compaction processor
    constructor(JsonLdContext ctx_processor, hash<JsonLdOptions> opts) {
        contextProcessor = ctx_processor;
        options = opts;
    }

    #! Compacts a JSON-LD element
    /** @param active_context the active context
        @param active_property the active property
        @param element the expanded element to compact
        @param compact_arrays whether to compact single-element arrays to scalars

        @return the compacted result

        @since json 1.12
    */
    auto compact(hash<JsonLdActiveContext> active_context, *string active_property,
            auto element, bool compact_arrays = True) {
        # 1. If element is a scalar, return it
        if (!exists element) {
            return NOTHING;
        }

        # 2. If element is an array
        if (element.typeCode() == NT_LIST) {
            list<auto> result = ();
            foreach auto item in (element) {
                auto compacted = compact(active_context, active_property, item, compact_arrays);
                if (exists compacted) {
                    push result, compacted;
                }
            }

            # Compact single-element arrays
            if (compact_arrays && result.size() == 1
                    && !hasArrayContainer(active_context, active_property)) {
                return result[0];
            }
            return result;
        }

        # 3. Element must be a hash (expanded node/value object)
        if (element.typeCode() != NT_HASH) {
            return element;
        }

        hash<auto> elem = element;

        # 4. If element is a value object
        if (isValueObject(elem)) {
            return compactValueObject(active_context, active_property, elem);
        }

        # 5. If element is a list object
        if (isListObject(elem)) {
            return compactListObject(active_context, active_property, elem);
        }

        # 5b. If element is a simple @id reference and active property has @type: @id/@vocab,
        # compact to a plain string
        if (elem.size() == 1 && exists elem."@id" && exists active_property) {
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};
            if (exists td && (td.typeMapping == "@id" || td.typeMapping == "@vocab")) {
                return JsonLdValue::compactValue(active_context, active_property, elem);
            }
        }

        # 6. Compact a node object
        return compactNodeObject(active_context, active_property, elem, compact_arrays);
    }

    #! Compacts a value object
    private auto compactValueObject(hash<JsonLdActiveContext> active_context,
            *string active_property, hash<auto> value) {
        auto compacted;
        if (exists active_property) {
            compacted = JsonLdValue::compactValue(active_context, active_property, value);
        }

        if (!exists compacted || compacted.typeCode() != NT_HASH) {
            if (!exists compacted && !exists active_property) {
                # No active property - try basic compaction
                hash<auto> result = {};
                foreach string key in (keys value) {
                    string compacted_key = JsonLdIri::compactIri(active_context, key, NOTHING, True);
                    result{compacted_key} = value{key};
                }
                return result;
            }
            return compacted;
        }

        # Compact internal keys of the value object
        hash<auto> val_hash = compacted;
        hash<auto> result = {};
        foreach string key in (keys val_hash) {
            string compacted_key = JsonLdIri::compactIri(active_context, key, NOTHING, True);
            auto compacted_val = val_hash{key};
            # Compact @type values
            if (key == "@type" && compacted_val.typeCode() == NT_STRING) {
                compacted_val = JsonLdIri::compactIri(active_context, compacted_val, NOTHING, True);
            }
            result{compacted_key} = compacted_val;
        }
        return result;
    }

    #! Compacts a list object
    private auto compactListObject(hash<JsonLdActiveContext> active_context,
            *string active_property, hash<auto> elem) {
        # Compact the list items
        list<auto> compacted_items = ();
        foreach auto item in (elem."@list") {
            auto compacted = compact(active_context, active_property, item);
            if (exists compacted) {
                push compacted_items, compacted;
            }
        }

        # If active property has @list container, return the compacted array directly
        if (hasContainerMapping(active_context, active_property, "@list")) {
            return compacted_items;
        }

        # Otherwise wrap in compacted @list key
        string list_key = JsonLdIri::compactIri(active_context, "@list", NOTHING, True);
        hash<auto> result = {};
        result{list_key} = compacted_items;

        # Include @index if present
        if (exists elem."@index") {
            string index_key = JsonLdIri::compactIri(active_context, "@index", NOTHING, True);
            result{index_key} = elem."@index";
        }

        return result;
    }

    #! Compacts a node object
    private hash<auto> compactNodeObject(hash<JsonLdActiveContext> active_context,
            *string active_property, hash<auto> elem, bool compact_arrays) {
        hash<auto> result = {};

        # Look up property-scoped context BEFORE restoring previousContext
        # The property term may be defined in the type-scoped context that previousContext
        # would revert, so we need to capture it before the revert
        *auto prop_local_context;
        *string prop_base_url;
        if (exists active_property) {
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};
            if (exists td && td.hasLocalContext) {
                prop_local_context = td.localContext;
                prop_base_url = td.baseUrl;
            }
        }

        # Restore previousContext for non-value nodes (mirrors expansion step 10)
        # This reverts type-scoped contexts from parent nodes
        if (exists active_context.previousContext) {
            # Check if this is a value-like node (has @value) — if so, keep type context
            bool has_value = exists elem."@value";
            # Check if this is a single @id node — if so, keep type context
            bool is_single_id = !has_value && elem.size() == 1 && exists elem."@id";
            if (!has_value && !is_single_id) {
                active_context = active_context.previousContext;
            }
        }

        # Apply property-scoped context (looked up before previousContext restoration)
        if (exists prop_local_context) {
            active_context = contextProcessor.processContext(active_context,
                prop_local_context, prop_base_url, NOTHING, True);  # override_protected=True
        }

        # Save context for @type value compaction (before type-scoped contexts)
        hash<JsonLdActiveContext> type_compaction_context = active_context;

        # Apply type-scoped contexts
        if (exists elem."@type") {
            # Sort types for deterministic context application
            list<auto> types = elem."@type".typeCode() == NT_LIST
                ? sort(elem."@type") : (elem."@type",);
            # Use pre-type context for looking up term definitions (same fix as expansion)
            hash<JsonLdActiveContext> pre_type_ctx = active_context;
            foreach auto t in (types) {
                if (t.typeCode() == NT_STRING) {
                    *hash<JsonLdTermDefinition> type_td = pre_type_ctx.termDefinitions{t};
                    if (!exists type_td) {
                        # Try to find via compacted IRI
                        string compacted_type = JsonLdIri::compactIri(pre_type_ctx, t, NOTHING, True);
                        type_td = pre_type_ctx.termDefinitions{compacted_type};
                    }
                    if (exists type_td && type_td.hasLocalContext) {
                        # Check if @propagate is explicitly set in the local context
                        bool propagate = False;
                        auto lc = type_td.localContext;
                        if (lc.typeCode() == NT_HASH
                                && lc."@propagate" == True) {
                            propagate = True;
                        } else if (lc.typeCode() == NT_LIST) {
                            foreach auto lci in (lc) {
                                if (lci.typeCode() == NT_HASH
                                        && lci."@propagate" == True) {
                                    propagate = True;
                                    break;
                                }
                            }
                        }
                        active_context = contextProcessor.processContext(active_context,
                            type_td.localContext, type_td.baseUrl, NOTHING, False, propagate);
                    }
                }
            }
        }

        # Process each key (sorted if ordered option is set)
        list<string> keys_list = options.ordered ? sort(keys elem) : keys elem;

        foreach string expanded_property in (keys_list) {
            auto expanded_value = elem{expanded_property};

            # Handle @id
            if (expanded_property == "@id") {
                string compacted_id;
                if (expanded_value.typeCode() == NT_STRING) {
                    compacted_id = JsonLdIri::compactIri(active_context, expanded_value, NOTHING, False);
                } else {
                    compacted_id = expanded_value;
                }
                string alias = JsonLdIri::compactIri(active_context, "@id", NOTHING, True);
                result{alias} = compacted_id;
                continue;
            }

            # Handle @type — use pre-type-scoped context for compacting type values
            if (expanded_property == "@type") {
                auto compacted_types;
                if (expanded_value.typeCode() == NT_LIST) {
                    list<auto> ct = ();
                    foreach auto tv in (expanded_value) {
                        if (tv.typeCode() == NT_STRING) {
                            push ct, JsonLdIri::compactIri(type_compaction_context, tv, NOTHING, True);
                        } else {
                            push ct, tv;
                        }
                    }
                    # In 1.1 mode, @type alias with @container: @set preserves arrays
                    # In 1.0 mode, @type always compacts to scalar for single values
                    string type_alias_check = JsonLdIri::compactIri(active_context,
                        "@type", NOTHING, True);
                    *hash<JsonLdTermDefinition> type_alias_td =
                        active_context.termDefinitions{type_alias_check};
                    bool type_has_set = exists type_alias_td
                        && exists type_alias_td.containerMapping
                        && inlist("@set", type_alias_td.containerMapping)
                        && options.processingMode != "json-ld-1.0";
                    compacted_types = ct.size() == 1 && compact_arrays && !type_has_set
                        ? ct[0] : ct;
                } else if (expanded_value.typeCode() == NT_STRING) {
                    compacted_types = JsonLdIri::compactIri(type_compaction_context, expanded_value, NOTHING, True);
                } else {
                    compacted_types = expanded_value;
                }
                string alias = JsonLdIri::compactIri(active_context, "@type", NOTHING, True);
                result{alias} = compacted_types;
                continue;
            }

            # Handle @reverse — per-value term selection
            if (expanded_property == "@reverse") {
                if (expanded_value.typeCode() == NT_HASH) {
                    hash<auto> rev_value = expanded_value;
                    foreach string prop in (keys rev_value) {
                        auto prop_values = rev_value{prop};
                        list<auto> items = prop_values.typeCode() == NT_LIST
                            ? prop_values : (prop_values,);

                        # Per-value term selection for reverse properties
                        hash<string, list<auto>> term_groups = {};
                        foreach auto item in (items) {
                            string item_alias = JsonLdIri::compactIri(active_context,
                                prop, item, True, True);
                            auto compacted_item = compact(active_context, item_alias,
                                item, compact_arrays);
                            if (!exists term_groups{item_alias}) {
                                term_groups{item_alias} = ();
                            }
                            push term_groups{item_alias}, compacted_item;
                        }

                        foreach string grp_alias in (keys term_groups) {
                            *hash<JsonLdTermDefinition> alias_td =
                                active_context.termDefinitions{grp_alias};
                            list<auto> grp_items = term_groups{grp_alias};
                            auto compacted_val;
                            if (compact_arrays && grp_items.size() == 1
                                    && !hasArrayContainer(active_context, grp_alias)) {
                                compacted_val = grp_items[0];
                            } else {
                                compacted_val = grp_items;
                            }

                            if (exists alias_td && alias_td.reverse) {
                                # Check for container mapping on reverse term
                                if (exists alias_td.containerMapping
                                        && inlist("@index", alias_td.containerMapping)) {
                                    # Build index map from the expanded values
                                    hash<auto> idx_map = {};
                                    # Check for property-valued index
                                    *string idx_prop;
                                    if (exists alias_td.indexProperty) {
                                        *string exp_idx_prop = JsonLdIri::expandIri(
                                            active_context, alias_td.indexProperty, True);
                                        idx_prop = exp_idx_prop;
                                    }
                                    foreach auto orig in (items) {
                                        if (orig.typeCode() != NT_HASH) {
                                            continue;
                                        }
                                        hash<auto> orig_hash = orig;
                                        string oidx;
                                        hash<auto> clean = orig_hash;
                                        if (exists idx_prop) {
                                            # Property-valued index
                                            auto idx_vals = orig_hash{idx_prop};
                                            if (idx_vals.typeCode() == NT_LIST
                                                    && idx_vals.size() > 0) {
                                                auto first = idx_vals[0];
                                                if (first.typeCode() == NT_HASH
                                                        && exists first."@id") {
                                                    # Check if the index property's term
                                                    # has @type: @id or @type: @vocab
                                                    *hash<JsonLdTermDefinition> idx_td;
                                                    if (exists alias_td.indexProperty) {
                                                        idx_td = active_context
                                                            .termDefinitions{
                                                                alias_td.indexProperty};
                                                    }
                                                    if (exists idx_td
                                                            && (idx_td.typeMapping == "@id"
                                                                || idx_td.typeMapping
                                                                    == "@vocab")) {
                                                        oidx = JsonLdIri::compactIri(
                                                            active_context, first."@id",
                                                            NOTHING,
                                                            idx_td.typeMapping == "@vocab");
                                                    } else {
                                                        oidx = "@none";
                                                    }
                                                } else if (first.typeCode() == NT_HASH
                                                        && first.hasKey("@value")) {
                                                    oidx = string(first."@value");
                                                } else {
                                                    oidx = "@none";
                                                }
                                                # Remove index property only if a real
                                                # index was extracted (not @none)
                                                if (oidx != "@none") {
                                                    if (idx_vals.size() == 1) {
                                                        remove clean{idx_prop};
                                                    } else {
                                                        clean{idx_prop} = idx_vals[1..];
                                                    }
                                                }
                                            } else {
                                                oidx = "@none";
                                            }
                                        } else {
                                            oidx = orig_hash."@index" ?? "@none";
                                            remove clean."@index";
                                        }
                                        if (oidx == "@none") {
                                            oidx = JsonLdIri::compactIri(active_context,
                                                "@none", NOTHING, True);
                                        }
                                        auto cv = compact(active_context, grp_alias,
                                            clean, compact_arrays);
                                        if (!exists idx_map{oidx}) {
                                            idx_map{oidx} = cv;
                                        } else if (idx_map{oidx}.typeCode() == NT_LIST) {
                                            push idx_map{oidx}, cv;
                                        } else {
                                            idx_map{oidx} = (idx_map{oidx}, cv);
                                        }
                                    }
                                    if (idx_map.size() > 0) {
                                        compacted_val = idx_map;
                                    }
                                }
                                result{grp_alias} = compacted_val;
                            } else {
                                # No reverse term — put under compacted @reverse key
                                string rev_key = JsonLdIri::compactIri(active_context,
                                    "@reverse", NOTHING, True);
                                if (!result.hasKey(rev_key)) {
                                    result{rev_key} = {};
                                }
                                result{rev_key}{grp_alias} = compacted_val;
                            }
                        }
                    }
                }
                continue;
            }

            # Handle @index - skip if container has @index
            if (expanded_property == "@index") {
                if (hasContainerMapping(active_context, active_property, "@index")) {
                    continue;
                }
                string alias = JsonLdIri::compactIri(active_context, "@index", NOTHING, True);
                result{alias} = expanded_value;
                continue;
            }

            # Handle @graph
            if (expanded_property == "@graph") {
                auto compacted_graph = compact(active_context, "@graph", expanded_value,
                    compact_arrays);
                string alias = JsonLdIri::compactIri(active_context, "@graph", NOTHING, True);
                # Named graphs (@id + @graph at non-container level) always use arrays
                # Inside a container property, compactArrays applies normally
                bool force_array = !exists active_property
                    || active_property == "@graph";
                if (force_array) {
                    # Top-level or named graph: @graph is always an array
                    if (compacted_graph.typeCode() != NT_LIST) {
                        compacted_graph = exists compacted_graph
                            ? (compacted_graph,) : ();
                    }
                } else {
                    # Inside property: apply compactArrays
                    if (compacted_graph.typeCode() != NT_LIST) {
                        if (!compact_arrays) {
                            compacted_graph = (compacted_graph,);
                        }
                    } else if (compact_arrays && compacted_graph.size() == 1) {
                        compacted_graph = compacted_graph[0];
                    }
                }
                result{alias} = compacted_graph;
                continue;
            }

            # Handle @included
            if (expanded_property == "@included") {
                auto compacted_inc = compact(active_context, NOTHING, expanded_value,
                    compact_arrays);
                string alias = JsonLdIri::compactIri(active_context, "@included", NOTHING, True);
                # @included is always an array in expanded form; compact based on container
                if (compacted_inc.typeCode() != NT_LIST) {
                    # Check if alias has @set container
                    bool has_set_container = hasArrayContainer(active_context, alias);
                    if (has_set_container || !compact_arrays) {
                        compacted_inc = (compacted_inc,);
                    }
                }
                if (exists result{alias}) {
                    if (result{alias}.typeCode() == NT_LIST) {
                        if (compacted_inc.typeCode() == NT_LIST) {
                            result{alias} += compacted_inc;
                        } else {
                            push result{alias}, compacted_inc;
                        }
                    } else {
                        result{alias} = (result{alias},);
                        if (compacted_inc.typeCode() == NT_LIST) {
                            result{alias} += compacted_inc;
                        } else {
                            push result{alias}, compacted_inc;
                        }
                    }
                } else {
                    result{alias} = compacted_inc;
                }
                continue;
            }

            # Handle @value, @language, @direction (shouldn't appear at node level)
            if (expanded_property == "@value" || expanded_property == "@language"
                    || expanded_property == "@direction") {
                string alias = JsonLdIri::compactIri(active_context, expanded_property, NOTHING, True);
                result{alias} = expanded_value;
                continue;
            }

            # Handle @list at node level (shouldn't normally happen, but be safe)
            if (expanded_property == "@list") {
                auto compacted_list = compact(active_context, active_property, expanded_value,
                    compact_arrays);
                string alias = JsonLdIri::compactIri(active_context, "@list", NOTHING, True);
                result{alias} = compacted_list;
                continue;
            }

            # Regular property
            if (expanded_value.typeCode() == NT_LIST && expanded_value.empty()) {
                # Empty array: compact property and set empty array
                string alias = JsonLdIri::compactIri(active_context, expanded_property,
                    expanded_value, True);
                result{alias} = ();
                continue;
            }

            # Compact the property — pass first item as representative value
            auto repr_value = expanded_value;
            if (expanded_value.typeCode() == NT_LIST && expanded_value.size() > 0) {
                repr_value = expanded_value[0];
            }
            string alias = JsonLdIri::compactIri(active_context, expanded_property,
                repr_value, True);

            # Check for container mappings
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{alias};

            # Handle @list container: per-list term selection
            if (exists td && exists td.containerMapping
                    && inlist("@list", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                # Each @list object gets its own per-list term selection
                hash<string, list<auto>> list_groups = {};
                foreach auto item in (expanded_value) {
                    if (item.typeCode() == NT_HASH && isListObject(item)) {
                        # Select the best term for this specific list
                        string list_alias = JsonLdIri::compactIri(active_context,
                            expanded_property, item, True);
                        if (!exists list_groups{list_alias}) {
                            list_groups{list_alias} = ();
                        }
                        foreach auto li in (item."@list") {
                            auto compacted_item = compact(active_context, list_alias,
                                li, compact_arrays);
                            if (exists compacted_item) {
                                push list_groups{list_alias}, compacted_item;
                            }
                        }
                    }
                }
                foreach string la in (keys list_groups) {
                    result{la} = list_groups{la};
                }
                continue;
            }

            # Handle @language container
            if (exists td && exists td.containerMapping
                    && inlist("@language", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                hash<auto> lang_map = {};
                # Collect values that don't belong in the language map
                # (per-value term selection may choose a different term)
                list<auto> non_map_values = ();
                foreach auto item in (expanded_value) {
                    if (item.typeCode() == NT_HASH && item.hasKey("@value")) {
                        # Check if this specific value should use a different term
                        string item_alias = JsonLdIri::compactIri(active_context,
                            expanded_property, item, True);
                        if (item_alias != alias) {
                            # Value prefers a different term — don't put in language map
                            push non_map_values, item;
                            continue;
                        }
                        string lang = item."@language" ?? "@none";
                        # Compact @none to its alias
                        if (lang == "@none") {
                            lang = JsonLdIri::compactIri(active_context, "@none",
                                NOTHING, True);
                        }
                        auto lang_val = item."@value";
                        # Wrap in array if @set container
                        bool lang_has_set = inlist("@set", td.containerMapping);
                        if (lang_has_set) {
                            lang_val = (lang_val,);
                        }
                        if (!exists lang_map{lang}) {
                            lang_map{lang} = lang_val;
                        } else if (lang_map{lang}.typeCode() == NT_LIST) {
                            if (lang_has_set) {
                                push lang_map{lang}, item."@value";
                            } else {
                                push lang_map{lang}, lang_val;
                            }
                        } else {
                            lang_map{lang} = (lang_map{lang}, lang_val);
                        }
                    }
                }
                result{alias} = lang_map;
                # Process non-map values through per-value term selection
                if (!non_map_values.empty()) {
                    foreach auto nmv in (non_map_values) {
                        string nmv_alias = JsonLdIri::compactIri(active_context,
                            expanded_property, nmv, True);
                        auto compacted_nmv;
                        if (isValueObject(nmv)) {
                            compacted_nmv = compactValueObject(active_context,
                                nmv_alias, nmv);
                        } else {
                            compacted_nmv = compact(active_context, nmv_alias,
                                nmv, compact_arrays);
                        }
                        if (exists result{nmv_alias}) {
                            if (result{nmv_alias}.typeCode() == NT_LIST) {
                                push result{nmv_alias}, compacted_nmv;
                            } else {
                                result{nmv_alias} = (result{nmv_alias}, compacted_nmv);
                            }
                        } else {
                            if (compact_arrays
                                    && !hasArrayContainer(active_context, nmv_alias)) {
                                result{nmv_alias} = compacted_nmv;
                            } else {
                                result{nmv_alias} = (compacted_nmv,);
                            }
                        }
                    }
                }
                continue;
            }

            # Handle @index container (skip if @graph is also in container — handled below)
            if (exists td && exists td.containerMapping
                    && inlist("@index", td.containerMapping)
                    && !inlist("@graph", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                hash<auto> index_map = {};
                # Check for property-valued index
                *string idx_prop;
                if (exists td.indexProperty) {
                    *string exp_idx_prop = JsonLdIri::expandIri(active_context,
                        td.indexProperty, True);
                    idx_prop = exp_idx_prop;
                }
                foreach auto item in (expanded_value) {
                    if (item.typeCode() == NT_HASH) {
                        string idx;
                        hash<auto> clean_item = item;
                        if (exists idx_prop) {
                            # Property-valued index: extract index from property value
                            auto idx_values = item{idx_prop};
                            if (idx_values.typeCode() == NT_LIST && idx_values.size() > 0) {
                                auto first = idx_values[0];
                                if (first.typeCode() == NT_HASH && first.hasKey("@value")) {
                                    idx = string(first."@value");
                                } else if (first.typeCode() == NT_HASH && exists first."@id") {
                                    # Check if the index property's term definition has
                                    # @type: @id or @type: @vocab — only then does a node
                                    # reference compact to a simple string suitable as an
                                    # index key. Without such a type mapping, the @id value
                                    # would compact to an object {"@id": "..."}, which cannot
                                    # serve as a property-valued index key.
                                    *string idx_compact_term = td.indexProperty;
                                    *hash<JsonLdTermDefinition> idx_td;
                                    if (exists idx_compact_term) {
                                        idx_td = active_context.termDefinitions{idx_compact_term};
                                    }
                                    if (exists idx_td
                                            && (idx_td.typeMapping == "@id"
                                                || idx_td.typeMapping == "@vocab")) {
                                        # The index property compacts @id values to strings
                                        idx = JsonLdIri::compactIri(active_context,
                                            first."@id", NOTHING,
                                            idx_td.typeMapping == "@vocab");
                                    } else {
                                        # @id value won't compact to a string — use @none
                                        # and keep the index property in the item
                                        idx = "@none";
                                    }
                                } else {
                                    idx = "@none";
                                }
                                # Remove the index property from the item only if we
                                # successfully extracted an index (not @none)
                                if (idx != "@none") {
                                    if (idx_values.size() == 1) {
                                        remove clean_item{idx_prop};
                                    } else {
                                        clean_item{idx_prop} = idx_values[1..];
                                    }
                                }
                            } else {
                                idx = "@none";
                            }
                        } else {
                            idx = item."@index" ?? "@none";
                            remove clean_item."@index";
                        }
                        # Compact @none to alias
                        if (idx == "@none") {
                            idx = JsonLdIri::compactIri(active_context, "@none", NOTHING, True);
                        }

                        # For property-valued index, when the index property was actually
                        # removed from the item, don't use the alias (avoids @type:@id
                        # shortcut that would prevent round-tripping)
                        bool idx_was_removed = exists idx_prop
                            && exists item{idx_prop} && !exists clean_item{idx_prop};
                        auto compacted_item = compact(active_context,
                            idx_was_removed ? NOTHING : alias, clean_item, compact_arrays);

                        # Wrap in array if @set container
                        bool idx_has_set = inlist("@set", td.containerMapping);
                        if (idx_has_set) {
                            compacted_item = compacted_item.typeCode() == NT_LIST
                                ? compacted_item : (compacted_item,);
                        }
                        if (!exists index_map{idx}) {
                            index_map{idx} = compacted_item;
                        } else if (index_map{idx}.typeCode() == NT_LIST) {
                            if (compacted_item.typeCode() == NT_LIST) {
                                index_map{idx} += compacted_item;
                            } else {
                                push index_map{idx}, compacted_item;
                            }
                        } else {
                            index_map{idx} = (index_map{idx}, compacted_item);
                        }
                    }
                }
                result{alias} = index_map;
                continue;
            }

            # Handle @id container (skip if @graph is also in container — handled below)
            if (exists td && exists td.containerMapping
                    && inlist("@id", td.containerMapping)
                    && !inlist("@graph", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                hash<auto> id_map = {};
                foreach auto item in (expanded_value) {
                    if (item.typeCode() == NT_HASH) {
                        string item_id = item."@id" ?? "@none";
                        if (item_id != "@none") {
                            item_id = JsonLdIri::compactIri(active_context, item_id);
                        } else {
                            # Compact @none to its alias
                            item_id = JsonLdIri::compactIri(active_context, "@none",
                                NOTHING, True);
                        }
                        hash<auto> clean_item = item;
                        remove clean_item."@id";
                        auto compacted_item = compact(active_context, alias, clean_item,
                            compact_arrays);

                        # Wrap in array if @set container
                        bool id_has_set = inlist("@set", td.containerMapping);
                        if (id_has_set) {
                            compacted_item = compacted_item.typeCode() == NT_LIST
                                ? compacted_item : (compacted_item,);
                        }

                        if (!exists id_map{item_id}) {
                            id_map{item_id} = compacted_item;
                        } else if (id_map{item_id}.typeCode() == NT_LIST) {
                            if (compacted_item.typeCode() == NT_LIST) {
                                id_map{item_id} += compacted_item;
                            } else {
                                push id_map{item_id}, compacted_item;
                            }
                        } else {
                            id_map{item_id} = (id_map{item_id}, compacted_item);
                        }
                    }
                }
                result{alias} = id_map;
                continue;
            }

            # Handle @graph container
            if (exists td && exists td.containerMapping
                    && inlist("@graph", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                bool has_id = inlist("@id", td.containerMapping);
                bool has_index = inlist("@index", td.containerMapping);

                # Check if @graph+@index/id map can be used
                bool can_use_graph_map = has_id || has_index;
                if (can_use_graph_map && has_index && !has_id) {
                    # @graph+@index cannot represent items with @id
                    foreach auto item in (expanded_value) {
                        if (item.typeCode() == NT_HASH && exists item."@id") {
                            can_use_graph_map = False;
                            break;
                        }
                    }
                }

                if (can_use_graph_map) {
                    hash<auto> graph_map = {};
                    foreach auto item in (expanded_value) {
                        if (item.typeCode() != NT_HASH) {
                            continue;
                        }
                        hash<auto> item_hash = item;
                        string map_key;
                        if (has_id) {
                            map_key = exists item_hash."@id"
                                ? JsonLdIri::compactIri(active_context, item_hash."@id")
                                : JsonLdIri::compactIri(active_context, "@none",
                                    NOTHING, True);
                        } else {
                            map_key = item_hash."@index"
                                ?? JsonLdIri::compactIri(active_context, "@none",
                                    NOTHING, True);
                        }
                        # Compact the @graph contents
                        auto graph_val = exists item_hash."@graph"
                            ? compact(active_context, alias, item_hash."@graph", compact_arrays)
                            : NOTHING;
                        if (exists graph_val) {
                            # For single-item results, unwrap unless @set container
                            bool has_set = inlist("@set", td.containerMapping);
                            if (graph_val.typeCode() == NT_LIST && graph_val.size() == 1
                                    && compact_arrays && !has_set) {
                                graph_val = graph_val[0];
                            }
                            if (!exists graph_map{map_key}) {
                                graph_map{map_key} = graph_val;
                            } else {
                                # Multiple entries for same key — ensure array
                                if (graph_map{map_key}.typeCode() != NT_LIST) {
                                    graph_map{map_key} = (graph_map{map_key},);
                                }
                                if (graph_val.typeCode() == NT_LIST) {
                                    graph_map{map_key} += graph_val;
                                } else {
                                    push graph_map{map_key}, graph_val;
                                }
                            }
                        }
                    }
                    result{alias} = graph_map;
                } else {
                    # @graph only (or @graph+@set): compact @graph objects
                    bool has_set = inlist("@set", td.containerMapping);
                    # Check if any single graph object contains multiple inner
                    # nodes — if so, those need @included wrapping
                    bool needs_included = False;
                    foreach auto item in (expanded_value) {
                        if (item.typeCode() == NT_HASH && isGraphObject(item)) {
                            hash<auto> ih = item;
                            if (!exists ih."@id" && ih."@graph".typeCode() == NT_LIST
                                    && ih."@graph".size() > 1) {
                                needs_included = True;
                                break;
                            }
                        }
                    }

                    list<auto> graph_items = ();
                    foreach auto item in (expanded_value) {
                        if (item.typeCode() == NT_HASH && isGraphObject(item)) {
                            hash<auto> ih = item;
                            if (exists ih."@id") {
                                # Graph with @id — compact the node with alias but without
                                # @index container stripping. Compact directly.
                                hash<auto> cv = {};
                                # Compact @id
                                cv{JsonLdIri::compactIri(active_context, "@id", NOTHING, True)} =
                                    JsonLdIri::compactIri(active_context, ih."@id", NOTHING, False);
                                # Compact @index if present
                                if (exists ih."@index") {
                                    cv{JsonLdIri::compactIri(active_context, "@index", NOTHING, True)} =
                                        ih."@index";
                                }
                                # Compact @graph contents
                                auto gv = compact(active_context, alias, ih."@graph",
                                    compact_arrays);
                                cv{JsonLdIri::compactIri(active_context, "@graph", NOTHING, True)} = gv;
                                push graph_items, cv;
                            } else if (needs_included) {
                                # Multiple nodes inside — wrap inner nodes
                                # with @included
                                list<auto> inner = ();
                                foreach auto gc in (ih."@graph") {
                                    auto cv = compact(active_context, alias, gc,
                                        compact_arrays);
                                    if (exists cv) {
                                        push inner, cv;
                                    }
                                }
                                string inc_key = JsonLdIri::compactIri(active_context,
                                    "@included", NOTHING, True);
                                hash<auto> wrapper = {};
                                wrapper{inc_key} = inner;
                                push graph_items, wrapper;
                            } else {
                                # Graph without @id — unwrap @graph contents
                                list<auto> graph_contents = ih."@graph";
                                foreach auto gc in (graph_contents) {
                                    auto cv = compact(active_context, alias, gc,
                                        compact_arrays);
                                    if (exists cv) {
                                        push graph_items, cv;
                                    }
                                }
                            }
                        } else {
                            auto cv = compact(active_context, alias, item,
                                compact_arrays);
                            if (exists cv) {
                                push graph_items, cv;
                            }
                        }
                    }
                    if (compact_arrays && graph_items.size() == 1 && !has_set) {
                        result{alias} = graph_items[0];
                    } else {
                        result{alias} = graph_items;
                    }
                }
                continue;
            }

            # Handle @type container
            if (exists td && exists td.containerMapping
                    && inlist("@type", td.containerMapping)
                    && expanded_value.typeCode() == NT_LIST) {
                hash<auto> type_map = {};
                foreach auto item in (expanded_value) {
                    if (item.typeCode() != NT_HASH) {
                        continue;
                    }
                    hash<auto> item_hash = item;
                    # Get the type(s) for the map key
                    list<string> types = ();
                    if (exists item_hash."@type") {
                        auto tv = item_hash."@type";
                        if (tv.typeCode() == NT_LIST) {
                            foreach auto t in (tv) {
                                if (t.typeCode() == NT_STRING) {
                                    push types, JsonLdIri::compactIri(active_context,
                                        t, NOTHING, True);
                                }
                            }
                        } else if (tv.typeCode() == NT_STRING) {
                            push types, JsonLdIri::compactIri(active_context,
                                tv, NOTHING, True);
                        }
                    }
                    if (types.empty()) {
                        # Compact @none to its alias
                        types = (JsonLdIri::compactIri(active_context, "@none",
                            NOTHING, True),);
                    }
                    # Use only the first type as the map key;
                    # put remaining types back in @type on the item
                    string map_type_key = types[0];
                    hash<auto> clean_item = item_hash;
                    if (types.size() > 1) {
                        # Keep remaining types in @type
                        list<string> remaining_types = types[1..];
                        # Re-expand remaining types for the clean item
                        list<auto> remaining_expanded = ();
                        foreach auto t in (item_hash."@type") {
                            string ct = t.typeCode() == NT_STRING
                                ? JsonLdIri::compactIri(active_context, t, NOTHING, True) : t;
                            if (ct != map_type_key) {
                                push remaining_expanded, t;
                            }
                        }
                        clean_item."@type" = remaining_expanded;
                    } else {
                        remove clean_item."@type";
                    }
                    # Apply type-scoped context for the map key type
                    hash<JsonLdActiveContext> type_ctx = active_context;
                    if (exists item_hash."@type") {
                        list<auto> orig_types = item_hash."@type".typeCode() == NT_LIST
                            ? item_hash."@type" : (item_hash."@type",);
                        foreach auto ot in (orig_types) {
                            if (ot.typeCode() == NT_STRING) {
                                # Look up type-scoped context by expanded IRI
                                *hash<JsonLdTermDefinition> ot_td =
                                    type_ctx.termDefinitions{ot};
                                if (!exists ot_td) {
                                    # Try compact form
                                    string compact_ot = JsonLdIri::compactIri(type_ctx,
                                        ot, NOTHING, True);
                                    ot_td = type_ctx.termDefinitions{compact_ot};
                                }
                                if (exists ot_td && ot_td.hasLocalContext) {
                                    type_ctx = contextProcessor.processContext(type_ctx,
                                        ot_td.localContext, ot_td.baseUrl);
                                }
                            }
                        }
                    }
                    auto compacted_item;
                    # If the cleaned item is just {"@id": "..."}, try value compaction
                    if (clean_item.size() == 1 && exists clean_item."@id") {
                        compacted_item = JsonLdValue::compactValue(type_ctx,
                            alias, clean_item);
                    } else {
                        compacted_item = compact(type_ctx, alias, clean_item,
                            compact_arrays);
                    }

                    # Wrap in array if @set container
                    bool type_has_set = inlist("@set", td.containerMapping);
                    if (type_has_set) {
                        compacted_item = compacted_item.typeCode() == NT_LIST
                            ? compacted_item : (compacted_item,);
                    }

                    # Use the first type as the map key
                    list<string> types_to_process = (map_type_key,);
                    foreach string type_key in (types_to_process) {
                        if (!exists type_map{type_key}) {
                            type_map{type_key} = compacted_item;
                        } else if (type_map{type_key}.typeCode() == NT_LIST) {
                            push type_map{type_key}, compacted_item;
                        } else {
                            type_map{type_key} = (type_map{type_key}, compacted_item);
                        }
                    }
                }
                result{alias} = type_map;
                continue;
            }

            # Standard compaction: per-value term selection (W3C 4.5 step 7)
            if (expanded_value.typeCode() == NT_LIST) {
                # Group values by the best compacted term for each value
                hash<string, list<auto>> term_groups = {};
                foreach auto item in (expanded_value) {
                    # Select best term for this specific value
                    string item_alias = JsonLdIri::compactIri(active_context,
                        expanded_property, item, True);
                    *hash<JsonLdTermDefinition> item_td =
                        active_context.termDefinitions{item_alias};

                    # Compact the value
                    auto compacted_item;
                    if (item.typeCode() == NT_HASH) {
                        hash<auto> item_hash = item;
                        if (exists item_td
                                && (item_td.typeMapping == "@id"
                                    || item_td.typeMapping == "@vocab")
                                && item_hash.size() == 1 && exists item_hash."@id") {
                            # Apply property-scoped context for @vocab compaction
                            hash<JsonLdActiveContext> value_ctx = active_context;
                            if (exists item_td && item_td.hasLocalContext) {
                                value_ctx = contextProcessor.processContext(active_context,
                                    item_td.localContext, item_td.baseUrl, NOTHING, True);
                            }
                            compacted_item = JsonLdValue::compactValue(value_ctx,
                                item_alias, item_hash);
                        } else if (isValueObject(item_hash)) {
                            compacted_item = compactValueObject(active_context,
                                item_alias, item_hash);
                        } else {
                            compacted_item = compact(active_context, item_alias,
                                item, compact_arrays);
                        }
                    } else {
                        compacted_item = item;
                    }

                    if (!exists term_groups{item_alias}) {
                        term_groups{item_alias} = ();
                    }
                    push term_groups{item_alias}, compacted_item;
                }

                # Add each group to the result
                foreach string grp_alias in (keys term_groups) {
                    list<auto> items = term_groups{grp_alias};
                    # Check if this group's term has @language container
                    *hash<JsonLdTermDefinition> grp_td_chk =
                        active_context.termDefinitions{grp_alias};
                    if (exists grp_td_chk && exists grp_td_chk.containerMapping
                            && inlist("@language", grp_td_chk.containerMapping)) {
                        # Build language map from the original expanded values
                        hash<auto> lang_map = {};
                        bool lang_has_set = inlist("@set", grp_td_chk.containerMapping);
                        foreach auto item in (expanded_value) {
                            if (item.typeCode() != NT_HASH
                                    || !item.hasKey("@value")) {
                                continue;
                            }
                            # Verify this item selects this term
                            string item_alias_chk = JsonLdIri::compactIri(active_context,
                                expanded_property, item, True);
                            if (item_alias_chk != grp_alias) {
                                continue;
                            }
                            string lang = item."@language" ?? "@none";
                            if (lang == "@none") {
                                lang = JsonLdIri::compactIri(active_context, "@none",
                                    NOTHING, True);
                            }
                            auto lang_val = item."@value";
                            if (lang_has_set) {
                                lang_val = (lang_val,);
                            }
                            if (!exists lang_map{lang}) {
                                lang_map{lang} = lang_val;
                            } else if (lang_map{lang}.typeCode() == NT_LIST) {
                                push lang_map{lang}, item."@value";
                            } else {
                                lang_map{lang} = (lang_map{lang}, lang_val);
                            }
                        }
                        result{grp_alias} = lang_map;
                        continue;
                    }
                    auto compacted_value;
                    if (compact_arrays && items.size() == 1
                            && !hasArrayContainer(active_context, grp_alias)) {
                        compacted_value = items[0];
                    } else if (items.size() == 1
                            && items[0].typeCode() == NT_LIST) {
                        # If the single item is already a list (e.g., from @list compaction
                        # or @type: @json), use it directly to avoid double-wrapping
                        *hash<JsonLdTermDefinition> grp_td =
                            active_context.termDefinitions{grp_alias};
                        if (exists grp_td && exists grp_td.containerMapping
                                && inlist("@list", grp_td.containerMapping)) {
                            # @list container: the compacted value IS the unwrapped list
                            compacted_value = items[0];
                        } else if (exists grp_td && grp_td.typeMapping == "@json") {
                            compacted_value = items[0];
                        } else {
                            compacted_value = items;
                        }
                    } else {
                        compacted_value = items;
                    }
                    if (exists result{grp_alias}) {
                        # Merge with existing values
                        if (result{grp_alias}.typeCode() == NT_LIST) {
                            result{grp_alias} += items;
                        } else {
                            result{grp_alias} = (result{grp_alias},) + items;
                        }
                    } else {
                        result{grp_alias} = compacted_value;
                    }
                }
            } else {
                auto compacted_value = compact(active_context, alias, expanded_value,
                    compact_arrays);
                result{alias} = compacted_value;
            }
        }

        # Post-process: move nested properties into @nest containers
        hash<auto> final_result = {};
        foreach string rk in (keys result) {
            *hash<JsonLdTermDefinition> rk_td = active_context.termDefinitions{rk};
            if (exists rk_td && exists rk_td.nestValue) {
                string nest_key = rk_td.nestValue;
                # Validate nest term is defined or is @nest
                if (nest_key != "@nest" && !exists active_context.termDefinitions{nest_key}) {
                    throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                        sprintf("@nest value %y references undefined term", nest_key));
                }
                if (!final_result.hasKey(nest_key)) {
                    final_result{nest_key} = {};
                }
                if (final_result{nest_key}.typeCode() == NT_HASH) {
                    ((final_result{nest_key})){rk} = result{rk};
                }
            } else {
                final_result{rk} = result{rk};
            }
        }

        return final_result;
    }

    #! Checks if the active property has a container that preserves arrays
    private bool hasArrayContainer(hash<JsonLdActiveContext> active_context, *string prop) {
        if (!exists prop) {
            return False;
        }
        *hash<JsonLdTermDefinition> td = active_context.termDefinitions{prop};
        if (!exists td || !exists td.containerMapping) {
            return False;
        }
        return inlist("@set", td.containerMapping) || inlist("@list", td.containerMapping);
    }

    #! Checks if the active property has a specific container mapping
    private bool hasContainerMapping(hash<JsonLdActiveContext> active_context, *string prop,
            string container) {
        if (!exists prop) {
            return False;
        }
        *hash<JsonLdTermDefinition> td = active_context.termDefinitions{prop};
        if (!exists td || !exists td.containerMapping) {
            return False;
        }
        return inlist(container, td.containerMapping);
    }
}

} # namespace JsonLd
