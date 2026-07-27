# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdExpansion.qc @brief JSON-LD Expansion Algorithm (W3C 4.3)

/*  JsonLdExpansion.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! JSON-LD Expansion Algorithm
/** Implements the Expansion Algorithm (W3C Section 4.3).
    Expands a JSON-LD document by resolving all terms to full IRIs,
    removing the context, and producing a uniform expanded form.

    @since json 1.12
*/
public class JsonLdExpansion {
    private {
        #! Context processor
        JsonLdContext contextProcessor;

        #! Processor options
        hash<JsonLdOptions> options;

    }

    #! Creates a new expansion processor
    /** @param ctx_processor the context processor
        @param opts processing options
    */
    constructor(JsonLdContext ctx_processor, hash<JsonLdOptions> opts) {
        contextProcessor = ctx_processor;
        options = opts;
    }

    #! Expands a JSON-LD element
    /** Implements the main expansion algorithm.

        @param active_context the active context
        @param active_property the active property (or NOTHING for top-level)
        @param element the element to expand
        @param base_url the base URL
        @param frame_expansion whether this is frame expansion
        @param ordered whether to process keys in order

        @return the expanded result

        @since json 1.12
    */
    auto expand(hash<JsonLdActiveContext> active_context, *string active_property,
            auto element, *string base_url, bool frame_expansion = False,
            bool ordered = False) {
        # Step 1: If element is null, return null
        if (!exists element) {
            return NOTHING;
        }

        # Step 8: Apply property-scoped context to ALL element types
        # Track if property-scoped context added @propagate:false previousContext
        *hash<JsonLdActiveContext> prop_scope_previous;
        if (exists active_property) {
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};
            if (exists td && td.hasLocalContext) {
                # Track whether previousContext existed before applying property scope
                bool had_previous = exists active_context.previousContext;
                active_context = contextProcessor.processContext(active_context, td.localContext,
                    td.baseUrl, NOTHING, True);  # override_protected=True
                if (had_previous && exists active_context.previousContext) {
                    # Remove type-scoped previousContext that was set before property scope
                    remove active_context."previousContext";
                } else if (!had_previous && exists active_context.previousContext) {
                    # Property-scoped @propagate:false set a new previousContext
                    # Save it and remove from active_context so step 10 in expandObject
                    # doesn't restore it for the immediate value object.
                    # It will be re-added after step 10 so nested objects will restore it.
                    if (element.typeCode() == NT_HASH) {
                        prop_scope_previous = active_context.previousContext;
                        remove active_context."previousContext";
                    }
                }
            }
        }

        # Step 2: If element is a scalar
        if (element.typeCode() != NT_HASH && element.typeCode() != NT_LIST) {
            if (!exists active_property || active_property == "@graph") {
                return NOTHING;
            }
            return JsonLdValue::expandValue(active_context, active_property, element);
        }

        # Step 3: If element is an array
        if (element.typeCode() == NT_LIST) {
            list<auto> result = ();
            foreach auto item in (element) {
                auto expanded_item = expand(active_context, active_property, item,
                    base_url, frame_expansion, ordered);
                if ((active_property == "@list" || hasListContainer(active_context, active_property))
                        && expanded_item.typeCode() == NT_LIST) {
                    expanded_item = {"@list": expanded_item};
                }
                if (exists expanded_item) {
                    if (expanded_item.typeCode() == NT_LIST) {
                        result += expanded_item;
                    } else {
                        push result, expanded_item;
                    }
                }
            }
            return result;
        }

        # Step 4: Element is an object
        hash<auto> elem = element;

        # Step 5: If element has @context, process it
        if (elem.hasKey("@context")) {
            active_context = contextProcessor.processContext(active_context, elem."@context", base_url);
        }

        # Process the object
        return expandObject(active_context, active_property, elem, base_url,
            frame_expansion, ordered, prop_scope_previous);
    }

    #! Expands a JSON-LD object
    private auto expandObject(hash<JsonLdActiveContext> active_context,
            *string active_property, hash<auto> element, *string base_url,
            bool frame_expansion, bool ordered,
            *hash<JsonLdActiveContext> prop_scope_previous) {
        # Initialize result
        hash<auto!> result = {};

        # Always sort keys for deterministic output
        # (the W3C test suite expects lexicographic ordering)
        list<string> keys_list = sort(keys element);

        # Find the @type value — it may be under an alias
        auto type_val;
        if (exists element."@type") {
            type_val = element."@type";
        } else {
            foreach string ek in (keys element) {
                *hash<JsonLdTermDefinition> ek_td = active_context.termDefinitions{ek};
                if (exists ek_td && ek_td.iriMapping == "@type") {
                    type_val = element{ek};
                    break;
                }
            }
        }

        # Deferred @nest objects (processed after all base properties)
        # Each entry is {key: <term>, obj: <nest object>}
        *list<hash<auto>> deferred_nest;

        # Step 10: Restore previousContext if element has no key expanding to @value
        # AND element does not consist of a single key expanding to @id
        # (per W3C 4.3 step 10: scope of term-scoped context applies to @id-only refs)
        if (exists active_context.previousContext) {
            bool has_value_key = False;
            bool is_single_id = False;
            list<string> elem_keys = keys element;
            foreach string ek in (elem_keys) {
                *string exp_ek = JsonLdIri::expandIri(active_context, ek, True);
                if (exp_ek == "@value") {
                    has_value_key = True;
                    break;
                }
            }
            # Check if element is a single @id key (possibly with @context)
            if (!has_value_key) {
                list<string> non_ctx_keys = ();
                foreach string ek in (elem_keys) {
                    if (ek != "@context") {
                        *string exp_ek = JsonLdIri::expandIri(active_context, ek, True);
                        if (exists exp_ek) {
                            push non_ctx_keys, exp_ek;
                        }
                    }
                }
                if (non_ctx_keys.size() == 1 && non_ctx_keys[0] == "@id") {
                    is_single_id = True;
                }
            }
            if (!has_value_key && !is_single_id) {
                active_context = active_context.previousContext;
            }
        }

        # Re-add property-scoped @propagate:false previousContext after step 10
        # This ensures the property-scoped terms apply to this node's properties
        # while nested objects will restore to previousContext via their own step 10
        if (exists prop_scope_previous) {
            active_context.previousContext = prop_scope_previous;
        }

        # Save context for @type value expansion (AFTER step 10, BEFORE step 13)
        hash<JsonLdActiveContext> type_expansion_context = active_context;

        # Step 13: Apply type-scoped contexts
        if (exists type_val) {
            list<string> type_terms = ();
            if (type_val.typeCode() == NT_STRING) {
                type_terms = (type_val,);
            } else if (type_val.typeCode() == NT_LIST) {
                foreach auto tv in (type_val) {
                    if (tv.typeCode() == NT_STRING) {
                        push type_terms, tv;
                    }
                }
                type_terms = sort(type_terms);
            }
            # Use the pre-type-scoped context for looking up term definitions
            # so that earlier type contexts don't hide later type definitions
            hash<JsonLdActiveContext> pre_type_ctx = active_context;
            foreach string tt in (type_terms) {
                *hash<JsonLdTermDefinition> type_td = pre_type_ctx.termDefinitions{tt};
                if (exists type_td && type_td.hasLocalContext) {
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

        # Track keyword aliases for collision detection
        hash<string, string> seen_keywords = {};

        # Check for nested properties and flatten them
        hash<auto> input_elem = element;
        foreach string key in (keys_list) {
            *hash<JsonLdTermDefinition> key_td = active_context.termDefinitions{key};
            if (exists key_td && exists key_td.nestValue) {
                # Skip nest containers - they'll be processed via nesting
                continue;
            }
        }

        # Process each key in the element
        foreach string key in (keys_list) {
            auto value = input_elem{key};

            # Skip @context (already processed)
            if (key == "@context") {
                continue;
            }

            # Expand the key
            *string expanded_property = JsonLdIri::expandIri(active_context, key, True);
            if (!exists expanded_property) {
                continue;
            }

            # Skip properties that expand to nothing or are not absolute IRIs/keywords
            if (!isJsonLdKeyword(expanded_property) && !isAbsoluteIri(expanded_property)
                    && !isBlankNodeIdentifier(expanded_property)) {
                continue;
            }

            # Process keyword properties
            if (isJsonLdKeyword(expanded_property)) {
                # Defer @nest processing to after all base keys
                if (expanded_property == "@nest") {
                    auto nest_val = input_elem{key};
                    list<auto> nest_objs = nest_val.typeCode() == NT_LIST
                        ? nest_val : (nest_val,);
                    foreach auto no in (nest_objs) {
                        push deferred_nest, {"key": key, "obj": no};
                    }
                    # Still call keyword handler for validation only
                    expandKeywordProperty(\result, active_context, active_property,
                        expanded_property, key, input_elem{key}, base_url,
                        frame_expansion, ordered);
                    continue;
                }

                # Detect colliding keywords (two different aliases for same keyword)
                # @type, @included, and @nest are exempt (multiple aliases allowed)
                if (exists seen_keywords{expanded_property}
                        && seen_keywords{expanded_property} != key
                        && expanded_property != "@type"
                        && expanded_property != "@included"
                        && expanded_property != "@nest") {
                    throwJsonLdError(JSONLD_ERR_COLLIDING_KEYWORDS,
                        sprintf("colliding keywords: %y and %y both expand to %y",
                            seen_keywords{expanded_property}, key, expanded_property));
                }
                seen_keywords{expanded_property} = key;

                # For @type, use the pre-type-scoped context for IRI expansion
                if (expanded_property == "@type") {
                    expandKeywordProperty(\result, type_expansion_context, active_property,
                        expanded_property, key, value, base_url, frame_expansion, ordered);
                } else {
                    expandKeywordProperty(\result, active_context, active_property,
                        expanded_property, key, value, base_url, frame_expansion, ordered);
                }
                continue;
            }

            # Get term definition for container handling
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{key};

            # Determine container mapping
            *list<string> container = td.containerMapping ?? NOTHING;

            # Determine target for container results: @reverse or regular
            bool is_reverse = exists td && td.reverse;

            # Handle @language container
            if (exists container && inlist("@language", container) && value.typeCode() == NT_HASH) {
                if (is_reverse) {
                    if (!exists result."@reverse") { result."@reverse" = {}; }
                    expandLanguageMap(\result."@reverse", active_context, expanded_property,
                        value, td, ordered);
                } else {
                    expandLanguageMap(\result, active_context, expanded_property,
                        value, td, ordered);
                }
                continue;
            }

            # Handle @index container (possibly combined with @graph)
            if (exists container && inlist("@index", container) && value.typeCode() == NT_HASH) {
                bool wrap_graph = inlist("@graph", container);
                if (is_reverse) {
                    if (!exists result."@reverse") { result."@reverse" = {}; }
                    expandIndexMap(\result."@reverse", active_context, expanded_property,
                        key, value, base_url, frame_expansion, ordered, td, wrap_graph);
                } else {
                    expandIndexMap(\result, active_context, expanded_property, key, value,
                        base_url, frame_expansion, ordered, td, wrap_graph);
                }
                continue;
            }

            # Handle @id container (possibly combined with @graph)
            if (exists container && inlist("@id", container) && value.typeCode() == NT_HASH) {
                bool wrap_graph = inlist("@graph", container);
                if (is_reverse) {
                    if (!exists result."@reverse") { result."@reverse" = {}; }
                    expandIdMap(\result."@reverse", active_context, expanded_property,
                        key, value, base_url, frame_expansion, ordered, td, wrap_graph);
                } else {
                    expandIdMap(\result, active_context, expanded_property, key, value,
                        base_url, frame_expansion, ordered, td, wrap_graph);
                }
                continue;
            }

            # Handle @type container
            if (exists container && inlist("@type", container) && value.typeCode() == NT_HASH) {
                expandTypeMap(\result, active_context, expanded_property, key, value,
                    base_url, frame_expansion, ordered, td);
                continue;
            }

            # Check for @type: @json - value should be treated as a JSON literal
            if (exists td && td.typeMapping == "@json") {
                auto expanded_value = {"@value": value, "@type": "@json"};
                mergeExpandedValue(\result, expanded_property, expanded_value);
                continue;
            }

            # Standard value expansion
            auto expanded_value = expand(active_context, key, value, base_url,
                frame_expansion, ordered);

            if (!exists expanded_value) {
                continue;
            }

            # If container includes @list and expanded value is not already a list object
            if (exists container && inlist("@list", container)
                    && !isListObject(expanded_value)) {
                if (expanded_value.typeCode() != NT_LIST) {
                    expanded_value = (expanded_value,);
                }
                expanded_value = {"@list": expanded_value};
            }

            # If container includes @graph, always wrap in @graph
            if (exists container && inlist("@graph", container)) {
                list<auto> items = expanded_value.typeCode() == NT_LIST
                    ? expanded_value : (expanded_value,);
                list<auto> wrapped = ();
                foreach auto item in (items) {
                    if (item.typeCode() == NT_LIST) {
                        push wrapped, {"@graph": item};
                    } else {
                        push wrapped, {"@graph": (item,)};
                    }
                }
                expanded_value = wrapped;
            }

            # If container includes @set, wrap in array
            if (exists container && inlist("@set", container)) {
                if (expanded_value.typeCode() != NT_LIST) {
                    expanded_value = (expanded_value,);
                }
            }

            # If td is reverse
            if (exists td && td.reverse) {
                if (!exists result."@reverse") {
                    result."@reverse" = {};
                }
                # Validate: reverse property values must not be value/list objects
                list<auto> rev_items = expanded_value.typeCode() == NT_LIST
                    ? expanded_value : (expanded_value,);
                foreach auto ri in (rev_items) {
                    if (ri.typeCode() == NT_HASH
                            && (isValueObject(ri) || isListObject(ri))) {
                        throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_PROPERTY_VALUE,
                            "value/list objects not allowed in @reverse");
                    }
                }
                mergeExpandedValue(\result."@reverse", expanded_property, expanded_value);
            } else {
                mergeExpandedValue(\result, expanded_property, expanded_value);
            }
        }

        # Handle nested properties (terms with @nest: "containerTerm")
        foreach string key in (keys_list) {
            *hash<JsonLdTermDefinition> key_td = active_context.termDefinitions{key};
            if (exists key_td && exists key_td.nestValue) {
                # Apply the nest term's property-scoped context if it has one
                hash<JsonLdActiveContext> nest_ctx = active_context;
                if (key_td.hasLocalContext) {
                    nest_ctx = contextProcessor.processContext(active_context,
                        key_td.localContext, key_td.baseUrl, NOTHING, True);
                }
                auto nested_value = input_elem{key};
                if (nested_value.typeCode() == NT_HASH) {
                    expandNestContent(\result, nest_ctx, active_property,
                        nested_value, base_url, frame_expansion, ordered);
                }
            }
        }

        # Process deferred @nest objects (after all base properties)
        # Sort by nest term name for deterministic ordering
        if (exists deferred_nest) {
            deferred_nest = sort(deferred_nest, int sub (hash<auto> a, hash<auto> b) {
                return a."key" <=> b."key";
            });
            foreach hash<auto> nest_entry in (deferred_nest) {
                string nest_term = nest_entry."key";
                hash<auto> nest_hash = nest_entry.obj;
                # Apply the nest term's property-scoped context if it has one
                hash<JsonLdActiveContext> nest_ctx = active_context;
                *hash<JsonLdTermDefinition> nest_td = active_context.termDefinitions{nest_term};
                if (exists nest_td && nest_td.hasLocalContext) {
                    nest_ctx = contextProcessor.processContext(active_context,
                        nest_td.localContext, nest_td.baseUrl, NOTHING, True);
                }
                expandNestContent(\result, nest_ctx, active_property, nest_hash,
                    base_url, frame_expansion, ordered);
            }
        }

        # Post-processing

        # Step 13: If result has @value (use hasKey to catch null @value)
        if (result.hasKey("@value")) {
            *hash<auto> normalized = normalizeValueObject(result);
            if (!exists normalized) {
                return NOTHING;
            }
            # Step 16: Drop free-floating value objects
            if (!exists active_property || active_property == "@graph") {
                return NOTHING;
            }
            return normalized;
        }

        # Step 14: If result has @type, ensure it's an array
        if (exists result."@type") {
            if (result."@type".typeCode() != NT_LIST) {
                result."@type" = (result."@type",);
            }
        }

        # Step 15: If result has @set, replace result with contents
        if (exists result."@set") {
            auto set_value = result."@set";
            # If active_property is null or @graph and @set is empty, return NOTHING
            if ((!exists active_property || active_property == "@graph")
                    && (!exists set_value || (set_value.typeCode() == NT_LIST && set_value.empty()))) {
                return NOTHING;
            }
            return set_value;
        }

        # If result has @list, ensure it only has @list (and optionally @index)
        if (exists result."@list") {
            foreach string k in (keys result) {
                if (k != "@list" && k != "@index") {
                    throwJsonLdError(JSONLD_ERR_INVALID_SET_OR_LIST_OBJECT,
                        sprintf("@list object has disallowed key: %y", k));
                }
            }
            # Step 16: Drop free-floating list objects
            if (!exists active_property || active_property == "@graph") {
                return NOTHING;
            }
        }

        # If result only has @language, discard it
        if (result.size() == 1 && exists result."@language") {
            return NOTHING;
        }

        # Step 16: If active property is null or @graph, drop free-floating nodes
        if (!exists active_property || active_property == "@graph") {
            if (result.empty()) {
                return NOTHING;
            }
            # Drop nodes with only @id (unless frame expansion)
            if (result.size() == 1 && exists result."@id" && !frame_expansion) {
                return NOTHING;
            }
        }

        # Only drop empty results when at top level (active_property is null/@graph)
        # Empty node objects should be preserved when they're property values
        if (result.empty() && (!exists active_property || active_property == "@graph")) {
            return NOTHING;
        }
        return result;
    }

    #! Expands nest content recursively
    /** Handles \@nest properties by flattening their content into the parent result.
        Supports nested \@nest (nest within nest).
    */
    private expandNestContent(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> nest_ctx, *string active_property,
            hash<auto> nest_hash, *string base_url, bool frame_expansion, bool ordered) {
        # Process @context in the nest hash first
        if (nest_hash.hasKey("@context")) {
            nest_ctx = contextProcessor.processContext(nest_ctx, nest_hash."@context", base_url);
        }
        foreach string nk in (ordered ? sort(keys nest_hash) : keys nest_hash) {
            if (nk == "@context") {
                continue;
            }
            *string nk_expanded = JsonLdIri::expandIri(nest_ctx, nk, True);
            if (!exists nk_expanded) {
                continue;
            }
            # If this key also expands to @nest, recursively flatten
            if (nk_expanded == "@nest") {
                # Apply the nest term's scoped context
                hash<JsonLdActiveContext> inner_ctx = nest_ctx;
                *hash<JsonLdTermDefinition> nk_td = nest_ctx.termDefinitions{nk};
                if (exists nk_td && nk_td.hasLocalContext) {
                    inner_ctx = contextProcessor.processContext(nest_ctx,
                        nk_td.localContext, nk_td.baseUrl, NOTHING, True);
                }
                auto inner_val = nest_hash{nk};
                list<auto> inner_objs = inner_val.typeCode() == NT_LIST
                    ? inner_val : (inner_val,);
                foreach auto inner_obj in (inner_objs) {
                    if (inner_obj.typeCode() == NT_HASH) {
                        expandNestContent(\result, inner_ctx, active_property,
                            inner_obj, base_url, frame_expansion, ordered);
                    }
                }
            } else if (isJsonLdKeyword(nk_expanded)) {
                expandKeywordProperty(\result, nest_ctx, active_property,
                    nk_expanded, nk, nest_hash{nk}, base_url, frame_expansion,
                    ordered);
            } else if (isAbsoluteIri(nk_expanded)
                    || isBlankNodeIdentifier(nk_expanded)) {
                # Get term definition for container handling
                *hash<JsonLdTermDefinition> td = nest_ctx.termDefinitions{nk};
                *list<string> container = td.containerMapping ?? NOTHING;

                # Handle @language container
                if (exists container && inlist("@language", container)
                        && nest_hash{nk}.typeCode() == NT_HASH) {
                    expandLanguageMap(\result, nest_ctx, nk_expanded,
                        nest_hash{nk}, td, ordered);
                    continue;
                }

                auto expanded_nv = expand(nest_ctx, nk, nest_hash{nk},
                    base_url, frame_expansion, ordered);
                if (exists expanded_nv) {
                    # Handle @list container
                    if (exists container && inlist("@list", container)
                            && !isListObject(expanded_nv)) {
                        if (expanded_nv.typeCode() != NT_LIST) {
                            expanded_nv = (expanded_nv,);
                        }
                        expanded_nv = {"@list": expanded_nv};
                    }
                    # Handle reverse property
                    if (exists td && td.reverse) {
                        if (!exists result."@reverse") {
                            result."@reverse" = {};
                        }
                        mergeExpandedValue(\result."@reverse", nk_expanded, expanded_nv);
                    } else {
                        mergeExpandedValue(\result, nk_expanded, expanded_nv);
                    }
                }
            }
        }
    }

    #! Processes a keyword property during expansion
    private expandKeywordProperty(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> active_context, *string active_property,
            string expanded_property, string key, auto value,
            *string base_url, bool frame_expansion, bool ordered) {
        switch (expanded_property) {
            case "@id": {
                if (value.typeCode() != NT_STRING) {
                    if (!frame_expansion) {
                        throwJsonLdError(JSONLD_ERR_INVALID_ID_VALUE,
                            sprintf("@id value must be a string; got %s", value.type()));
                    }
                }
                if (value.typeCode() == NT_STRING) {
                    *string expanded_id = JsonLdIri::expandIri(active_context, value, False, True);
                    # Always set @id, even when expanded to null (NOTHING)
                    # Use hasKey to preserve null @id values in the result
                    result{"@id"} = expanded_id;
                } else {
                    result."@id" = value;
                }
                break;
            }

            case "@type": {
                if (value.typeCode() == NT_STRING) {
                    *string expanded_type = JsonLdIri::expandIri(active_context, value, True, True);
                    if (exists expanded_type) {
                        if (exists result."@type") {
                            if (result."@type".typeCode() == NT_LIST) {
                                push result."@type", expanded_type;
                            } else {
                                result."@type" = (result."@type", expanded_type);
                            }
                        } else {
                            result."@type" = expanded_type;
                        }
                    }
                } else if (value.typeCode() == NT_LIST) {
                    list<auto> expanded_types = ();
                    foreach auto tv in (value) {
                        if (tv.typeCode() == NT_STRING) {
                            *string et = JsonLdIri::expandIri(active_context, tv, True, True);
                            if (exists et) {
                                push expanded_types, et;
                            }
                        }
                    }
                    if (exists result."@type") {
                        if (result."@type".typeCode() == NT_LIST) {
                            result."@type" += expanded_types;
                        } else {
                            result."@type" = (result."@type",) + expanded_types;
                        }
                    } else {
                        result."@type" = expanded_types;
                    }
                } else if (!frame_expansion) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TYPE_VALUE,
                        sprintf("@type must be a string or array; got %s", value.type()));
                }
                break;
            }

            case "@value": {
                result."@value" = value;
                break;
            }

            case "@language": {
                if (value.typeCode() != NT_STRING) {
                    if (!frame_expansion) {
                        throwJsonLdError(JSONLD_ERR_INVALID_LANGUAGE_TAGGED_STRING,
                            sprintf("@language must be a string; got %s", value.type()));
                    }
                }
                result."@language" = value.typeCode() == NT_STRING ? value.lwr() : value;
                break;
            }

            case "@direction": {
                if (value.typeCode() != NT_STRING) {
                    if (!frame_expansion) {
                        throwJsonLdError(JSONLD_ERR_INVALID_BASE_DIRECTION,
                            sprintf("@direction must be a string; got %s", value.type()));
                    }
                }
                result."@direction" = value;
                break;
            }

            case "@index": {
                if (value.typeCode() != NT_STRING) {
                    throwJsonLdError(JSONLD_ERR_INVALID_INDEX_VALUE,
                        sprintf("@index must be a string; got %s", value.type()));
                }
                result."@index" = value;
                break;
            }

            case "@list": {
                if (!exists active_property || active_property == "@graph") {
                    return;
                }
                auto expanded_list = expand(active_context, active_property, value,
                    base_url, frame_expansion, ordered);
                if (expanded_list.typeCode() != NT_LIST) {
                    expanded_list = (expanded_list,);
                }
                result."@list" = expanded_list;
                break;
            }

            case "@set": {
                auto expanded_set = expand(active_context, active_property, value,
                    base_url, frame_expansion, ordered);
                result."@set" = expanded_set;
                break;
            }

            case "@reverse": {
                if (value.typeCode() != NT_HASH) {
                    throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_VALUE,
                        sprintf("@reverse value must be an object; got %s", value.type()));
                }
                # Validate: @reverse map keys must not be keywords
                hash<auto> rev_map = value;
                foreach string rk in (keys rev_map) {
                    if (rk == "@context") {
                        continue;
                    }
                    *string expanded_rk = JsonLdIri::expandIri(active_context, rk, True);
                    if (exists expanded_rk && isJsonLdKeyword(expanded_rk)) {
                        throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_PROPERTY_MAP,
                            sprintf("@reverse map must not contain keywords; got %y", rk));
                    }
                }
                auto expanded_reverse = expand(active_context, "@reverse", value,
                    base_url, frame_expansion, ordered);
                if (expanded_reverse.typeCode() == NT_HASH) {
                    hash<auto> rev_hash = expanded_reverse;
                    # Merge any nested @reverse
                    if (exists rev_hash."@reverse") {
                        hash<auto> nested_rev = rev_hash."@reverse";
                        foreach string prop in (keys nested_rev) {
                            mergeExpandedValue(\result, prop, nested_rev{prop});
                        }
                        remove rev_hash."@reverse";
                    }
                    if (rev_hash.size() > 0) {
                        if (!exists result."@reverse") {
                            result."@reverse" = {};
                        }
                        foreach string prop in (keys rev_hash) {
                            list<auto> items = rev_hash{prop}.typeCode() == NT_LIST
                                ? rev_hash{prop} : (rev_hash{prop},);
                            foreach auto item in (items) {
                                if (isValueObject(item) || isListObject(item)) {
                                    throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_PROPERTY_VALUE,
                                        "value/list objects not allowed in @reverse");
                                }
                                mergeExpandedValue(\result."@reverse", prop, item);
                            }
                        }
                    }
                }
                break;
            }

            case "@graph": {
                auto expanded_graph = expand(active_context, "@graph", value,
                    base_url, frame_expansion, ordered);
                if (expanded_graph.typeCode() != NT_LIST) {
                    expanded_graph = exists expanded_graph ? (expanded_graph,) : ();
                }
                result."@graph" = expanded_graph;
                break;
            }

            case "@included": {
                # @included value must be an object or array of objects (not value/list objects)
                if (value.typeCode() != NT_HASH && value.typeCode() != NT_LIST) {
                    throwJsonLdError(JSONLD_ERR_INVALID_INCLUDED_VALUE,
                        sprintf("@included value must be an object or array; got %s", value.type()));
                }
                # Check raw input for value/list objects
                list<auto> raw_items = value.typeCode() == NT_LIST ? value : (value,);
                foreach auto ri in (raw_items) {
                    if (ri.typeCode() == NT_HASH) {
                        hash<auto> rih = ri;
                        if (rih.hasKey("@value") || rih.hasKey("@list")) {
                            throwJsonLdError(JSONLD_ERR_INVALID_INCLUDED_VALUE,
                                "@included values must be node objects, not value/list objects");
                        }
                    }
                }
                auto expanded_inc = expand(active_context, NOTHING, value,
                    base_url, frame_expansion, ordered);
                if (expanded_inc.typeCode() != NT_LIST) {
                    expanded_inc = exists expanded_inc ? (expanded_inc,) : ();
                }
                # Validate: all @included values must be node objects
                foreach auto inc_item in (expanded_inc) {
                    if (inc_item.typeCode() != NT_HASH || isValueObject(inc_item)
                            || isListObject(inc_item)) {
                        throwJsonLdError(JSONLD_ERR_INVALID_INCLUDED_VALUE,
                            "@included values must be node objects");
                    }
                }
                if (exists result."@included") {
                    result."@included" += expanded_inc;
                } else {
                    result."@included" = expanded_inc;
                }
                break;
            }

            case "@nest": {
                # @nest in data — just validate; defer processing to after all base keys
                if (value.typeCode() != NT_HASH && value.typeCode() != NT_LIST) {
                    throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                        sprintf("@nest value must be an object or array; got %s", value.type()));
                }
                list<auto> nest_objs = value.typeCode() == NT_LIST ? value : (value,);
                foreach auto nest_obj in (nest_objs) {
                    if (nest_obj.typeCode() != NT_HASH) {
                        throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                            sprintf("@nest array items must be objects; got %s",
                                nest_obj.type()));
                    }
                    if (nest_obj.hasKey("@value")
                            || nest_obj.hasKey("@list")) {
                        throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                            "@nest value must not be a value object or list object");
                    }
                }
                # Don't process here — expandObject handles it after all base keys
                break;
            }
        }
    }

    #! Expands a language map
    private expandLanguageMap(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> active_context, string expanded_property,
            hash<auto> value, *hash<JsonLdTermDefinition> td, bool ordered = False) {
        list<auto> expanded_values = ();

        # Always sort language map keys for deterministic output
        foreach string lang in (sort(keys value)) {
            auto lang_values = value{lang};
            if (lang_values.typeCode() != NT_LIST) {
                lang_values = (lang_values,);
            }
            foreach auto item in (lang_values) {
                if (!exists item) {
                    continue;
                }
                if (item.typeCode() != NT_STRING) {
                    throwJsonLdError(JSONLD_ERR_INVALID_LANGUAGE_MAP_VALUE,
                        sprintf("language map values must be strings; got %s", item.type()));
                }
                hash<auto> val_obj = {"@value": item};
                # Check if the language key is @none or maps to @none via term def
                string effective_lang = lang;
                if (exists active_context.termDefinitions{lang}) {
                    *string mapped = active_context.termDefinitions{lang}.iriMapping;
                    if (mapped == "@none") {
                        effective_lang = "@none";
                    }
                }
                if (effective_lang != "@none") {
                    val_obj."@language" = lang.lwr();
                    # Add @direction from term definition or context default
                    *string dir;
                    if (exists td && td.hasDirectionMapping) {
                        dir = td.directionMapping;
                    } else if (exists active_context.defaultBaseDirection) {
                        dir = active_context.defaultBaseDirection;
                    }
                    if (exists dir) {
                        val_obj."@direction" = dir;
                    }
                }
                push expanded_values, val_obj;
            }
        }

        mergeExpandedValue(\result, expanded_property, expanded_values);
    }

    #! Expands an index map
    private expandIndexMap(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> active_context, string expanded_property,
            string key, hash<auto> value, *string base_url,
            bool frame_expansion, bool ordered, *hash<JsonLdTermDefinition> td,
            bool wrap_graph = False) {
        list<auto> expanded_values = ();

        foreach string idx in (sort(keys value)) {
            auto idx_values = value{idx};
            if (idx_values.typeCode() != NT_LIST) {
                idx_values = (idx_values,);
            }
            auto expanded_items = expand(active_context, key, idx_values, base_url,
                frame_expansion, ordered);
            if (expanded_items.typeCode() != NT_LIST) {
                expanded_items = (expanded_items,);
            }

            if (wrap_graph) {
                # Check if idx is @none or maps to @none
                bool idx_is_none = (idx == "@none");
                if (!idx_is_none && exists active_context.termDefinitions{idx}) {
                    idx_is_none = active_context.termDefinitions{idx}.iriMapping == "@none";
                }
                # Check for property-valued index
                *string graph_index_prop;
                if (exists td && exists td.indexProperty) {
                    graph_index_prop = JsonLdIri::expandIri(active_context,
                        td.indexProperty, True);
                }
                # Each expanded item gets its own @graph entry with the index
                foreach auto ei in (expanded_items) {
                    hash<auto!> graph_obj;
                    if (ei.typeCode() == NT_HASH && isGraphObject(ei)) {
                        graph_obj = ei;
                    } else {
                        graph_obj = {"@graph": (ei,)};
                    }
                    if (!idx_is_none) {
                        if (exists graph_index_prop) {
                            # Property-valued index on graph object
                            auto idx_value = JsonLdValue::expandValue(active_context,
                                td.indexProperty, idx);
                            graph_obj{graph_index_prop} = (idx_value,);
                        } else {
                            graph_obj."@index" = idx;
                        }
                    }
                    push expanded_values, graph_obj;
                }
            } else {
                # Check if idx is @none or maps to @none
                bool idx_is_none2 = (idx == "@none");
                if (!idx_is_none2 && exists active_context.termDefinitions{idx}) {
                    idx_is_none2 = active_context.termDefinitions{idx}.iriMapping == "@none";
                }
                # Check for property-valued index
                *string index_prop;
                if (exists td && exists td.indexProperty) {
                    *string expanded_idx_prop = JsonLdIri::expandIri(active_context,
                        td.indexProperty, True);
                    index_prop = expanded_idx_prop;
                }
                foreach auto item in (expanded_items) {
                    if (!exists item) {
                        continue;
                    }
                    if (item.typeCode() == NT_HASH && !idx_is_none2) {
                        hash<auto!> item_hash = item;
                        if (exists index_prop) {
                            # Property-valued index: can't add property to value objects
                            if (isValueObject(item_hash)) {
                                throwJsonLdError(JSONLD_ERR_INVALID_VALUE_OBJECT,
                                    "cannot add property to a value object via "
                                    "property-valued index");
                            }
                            # Prepend the index value before existing property values
                            auto idx_value = JsonLdValue::expandValue(active_context,
                                td.indexProperty, idx);
                            if (exists item_hash{index_prop}) {
                                list<auto> existing = item_hash{index_prop}.typeCode() == NT_LIST
                                    ? item_hash{index_prop} : (item_hash{index_prop},);
                                item_hash{index_prop} = (idx_value,) + existing;
                            } else {
                                item_hash{index_prop} = (idx_value,);
                            }
                        } else if (!exists item_hash."@index") {
                            item_hash."@index" = idx;
                        }
                        item = item_hash;
                    }
                    push expanded_values, item;
                }
            }
        }

        mergeExpandedValue(\result, expanded_property, expanded_values);
    }

    #! Expands an id map
    private expandIdMap(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> active_context, string expanded_property,
            string key, hash<auto> value, *string base_url,
            bool frame_expansion, bool ordered, *hash<JsonLdTermDefinition> td,
            bool wrap_graph = False) {
        list<auto> expanded_values = ();

        foreach string id in (sort(keys value)) {
            auto id_values = value{id};
            if (id_values.typeCode() != NT_LIST) {
                id_values = (id_values,);
            }
            auto expanded_items = expand(active_context, key, id_values, base_url,
                frame_expansion, ordered);
            if (expanded_items.typeCode() != NT_LIST) {
                expanded_items = (expanded_items,);
            }

            if (wrap_graph) {
                # Each expanded item gets its own @graph entry with the id
                foreach auto ei in (expanded_items) {
                    hash<auto!> graph_obj;
                    if (ei.typeCode() == NT_HASH && isGraphObject(ei)) {
                        graph_obj = ei;
                    } else {
                        graph_obj = {"@graph": (ei,)};
                    }
                    # Check if id is @none or maps to @none
                    bool id_is_none = (id == "@none");
                    if (!id_is_none && exists active_context.termDefinitions{id}) {
                        id_is_none = active_context.termDefinitions{id}.iriMapping == "@none";
                    }
                    if (!id_is_none) {
                        *string expanded_id = JsonLdIri::expandIri(active_context, id,
                            False, True);
                        if (exists expanded_id) {
                            graph_obj."@id" = expanded_id;
                        }
                    }
                    push expanded_values, graph_obj;
                }
            } else {
                foreach auto item in (expanded_items) {
                    if (!exists item || item.typeCode() != NT_HASH) {
                        continue;
                    }
                    hash<auto> item_hash = item;
                    # Check if id is @none or maps to @none via term definition
                    bool is_none = (id == "@none");
                    if (!is_none && exists active_context.termDefinitions{id}) {
                        is_none = active_context.termDefinitions{id}.iriMapping == "@none";
                    }
                    if (!is_none) {
                        *string expanded_id = JsonLdIri::expandIri(active_context, id,
                            False, True);
                        if (!exists item_hash."@id") {
                            item_hash."@id" = expanded_id;
                        }
                    }
                    push expanded_values, item_hash;
                }
            }
        }

        mergeExpandedValue(\result, expanded_property, expanded_values);
    }

    #! Expands a type map
    private expandTypeMap(reference<hash<auto!>> result,
            hash<JsonLdActiveContext> active_context, string expanded_property,
            string key, hash<auto> value, *string base_url,
            bool frame_expansion, bool ordered, *hash<JsonLdTermDefinition> td) {
        list<auto> expanded_values = ();

        foreach string type_key in (sort(keys value)) {
            auto type_values = value{type_key};
            if (type_values.typeCode() != NT_LIST) {
                type_values = (type_values,);
            }

            # Apply scoped context for type if available
            # For type map values, apply the type-scoped context on top of the
            # previousContext (if any), not the current type-scoped context.
            # This ensures the outer type's context doesn't leak into inner types.
            hash<JsonLdActiveContext> type_ctx = active_context;
            *hash<JsonLdTermDefinition> type_td = active_context.termDefinitions{type_key};
            if (exists type_td && type_td.hasLocalContext) {
                hash<JsonLdActiveContext> base_ctx = exists active_context.previousContext
                    ? active_context.previousContext : active_context;
                type_ctx = contextProcessor.processContext(base_ctx,
                    type_td.localContext, type_td.baseUrl);
            }

            auto expanded_items = expand(type_ctx, key, type_values, base_url,
                frame_expansion, ordered);
            if (expanded_items.typeCode() != NT_LIST) {
                expanded_items = (expanded_items,);
            }

            *string expanded_type = JsonLdIri::expandIri(active_context, type_key, True);

            foreach auto item in (expanded_items) {
                if (!exists item || item.typeCode() != NT_HASH) {
                    continue;
                }
                hash<auto!> item_hash = item;
                # @none type means no @type should be added
                if (exists expanded_type && expanded_type != "@none") {
                    if (exists item_hash."@type") {
                        # Prepend the map key type (it should come first)
                        if (item_hash."@type".typeCode() == NT_LIST) {
                            if (!inlist(expanded_type, item_hash."@type")) {
                                item_hash."@type" = (expanded_type,) + item_hash."@type";
                            }
                        } else {
                            item_hash."@type" = (expanded_type, item_hash."@type");
                        }
                    } else {
                        item_hash."@type" = (expanded_type,);
                    }
                }
                push expanded_values, item_hash;
            }
        }

        mergeExpandedValue(\result, expanded_property, expanded_values);
    }

    #! Normalizes a value object, checking for invalid combinations
    private *hash<auto> normalizeValueObject(hash<auto> result) {
        # Check for invalid combinations
        if (exists result."@type") {
            if (exists result."@language" || exists result."@direction") {
                throwJsonLdError(JSONLD_ERR_INVALID_VALUE_OBJECT,
                    "value object cannot have both @type and @language/@direction");
            }
        }

        # Validate that @type in value object is a single string (not an array with multiple items)
        if (exists result."@type" && result."@type".typeCode() == NT_LIST) {
            list<auto> type_list = result."@type";
            if (type_list.size() > 1) {
                throwJsonLdError(JSONLD_ERR_INVALID_VALUE_OBJECT,
                    "value object @type must be a single value");
            }
            if (type_list.size() == 1) {
                result."@type" = type_list[0];
            }
        }

        # Validate @value content
        auto val = result."@value";
        if (exists result."@type") {
            # @type in value objects must be a well-formed IRI (not a blank node, no spaces)
            string vtype = result."@type";
            if (vtype != "@json" && (!isAbsoluteIri(vtype) || vtype =~ /\s/)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TYPED_VALUE,
                    sprintf("@type in value object must be an IRI or @json; got %y", vtype));
            }
            if (vtype == "@json") {
                # @json allows any value
            } else if (exists val && val.typeCode() != NT_BOOLEAN && val.typeCode() != NT_INT
                    && val.typeCode() != NT_FLOAT && val.typeCode() != NT_STRING
                    && val.typeCode() != NT_NUMBER) {
                throwJsonLdError(JSONLD_ERR_INVALID_TYPED_VALUE,
                    sprintf("@value with @type must be a scalar; got %s", val.type()));
            }
        } else if (exists val && (val.typeCode() == NT_HASH || val.typeCode() == NT_LIST)) {
            # @value without @type: @json must be scalar, string, number, boolean, or null
            throwJsonLdError(JSONLD_ERR_INVALID_VALUE_OBJECT_VALUE,
                sprintf("@value must be a scalar or null without @type: @json; got %s", val.type()));
        } else if (exists result."@language") {
            if (val.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_LANGUAGE_TAGGED_VALUE,
                    sprintf("@language requires @value to be a string; got %s", val.type()));
            }
        }

        # Check for extra keys
        foreach string k in (keys result) {
            if (k != "@value" && k != "@type" && k != "@language" && k != "@direction"
                    && k != "@index") {
                throwJsonLdError(JSONLD_ERR_INVALID_VALUE_OBJECT,
                    sprintf("invalid key in value object: %y", k));
            }
        }

        # Null @value: @type: @json allows null, otherwise return NOTHING
        if (!exists val) {
            if (exists result."@type" && result."@type" == "@json") {
                # @json with null is valid — keep the value object
            } else {
                return NOTHING;
            }
        }

        return result;
    }

    #! Checks if the active property has a \@list container
    private bool hasListContainer(hash<JsonLdActiveContext> active_context, *string active_property) {
        if (!exists active_property) {
            return False;
        }
        *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};
        if (!exists td || !exists td.containerMapping) {
            return False;
        }
        return inlist("@list", td.containerMapping);
    }

    #! Merges an expanded value into the result
    static mergeExpandedValue(reference<hash<auto!>> result, string property, auto value) {
        if (!exists value) {
            return;
        }

        list<auto!> values;
        if (value.typeCode() == NT_LIST) {
            values = value;
        } else {
            values = (value,);
        }

        if (exists result{property}) {
            if (result{property}.typeCode() == NT_LIST) {
                result{property} += values;
            } else {
                result{property} = (result{property},) + values;
            }
        } else {
            result{property} = values;
        }
    }

    #! Pushes a value to a list if not already present
    static pushUniq(reference<auto> lst, auto value) {
        if (lst.typeCode() != NT_LIST) {
            lst = (lst,);
        }
        if (!inlist(value, lst)) {
            push lst, value;
        }
    }
}

} # namespace JsonLd
