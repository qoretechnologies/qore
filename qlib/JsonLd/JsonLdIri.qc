# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdIri.qc @brief JSON-LD IRI expansion and compaction algorithms

/*  JsonLdIri.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! JSON-LD IRI expansion and compaction
/** Implements IRI Expansion (W3C 5.2.2) and IRI Compaction (W3C 6.2.2).

    @since json 1.12
*/
public class JsonLdIri {
    #! Expands a value to an IRI or keyword
    /** Implements the IRI Expansion algorithm (W3C Section 5.2.2)

        @param active_context the active context
        @param value the value to expand
        @param vocab whether to use vocabulary mapping
        @param document_relative whether to resolve relative to document base
        @param local_context optional local context being processed
        @param defined optional defined map for term creation

        @return the expanded IRI, keyword, or NOTHING

        @since json 1.12
    */
    static *string expandIri(hash<JsonLdActiveContext> active_context, *string value,
            bool vocab = False, bool document_relative = False,
            *hash<auto> local_context, *reference<hash<string, bool>> defined) {
        # Step 1: If value is null or a keyword, return it
        if (!exists value) {
            return NOTHING;
        }
        if (isJsonLdKeyword(value)) {
            return value;
        }

        # Step 2: If value has the form of a keyword (@1*ALPHA), return null
        # Only matches @ followed by one or more letters (not @, @foo.bar, etc.)
        if (value =~ /^@[a-zA-Z]+$/) {
            return NOTHING;
        }

        # Step 3: If local context contains value and it's not yet defined, create definition
        if (exists local_context && exists defined && local_context.hasKey(value)
                && !defined{value}) {
            JsonLdContext ctx_processor();
            ctx_processor.createTermDefinition(\active_context, local_context, value, \defined);
        }

        # Step 4: If active context has a term definition for value with keyword mapping
        if (exists active_context.termDefinitions{value}) {
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{value};
            if (exists td.iriMapping && isJsonLdKeyword(td.iriMapping)) {
                return td.iriMapping;
            }
        }

        # Step 5: If vocab is true and term definition exists, return IRI mapping
        # But skip if the mapping is a keyword-like non-keyword (e.g., @ignoreMe)
        if (vocab && exists active_context.termDefinitions{value}) {
            *hash<JsonLdTermDefinition> td = active_context.termDefinitions{value};
            if (exists td.iriMapping) {
                # Skip keyword-like non-keywords — fall through to @vocab expansion
                if (td.iriMapping =~ /^@[a-zA-Z]+$/ && !isJsonLdKeyword(td.iriMapping)) {
                    # Fall through to @vocab expansion
                } else {
                    return td.iriMapping;
                }
            } else {
                return td.iriMapping;
            }
        }

        # Step 6: If value contains a colon
        int colon_pos = index(value, ":");
        if (colon_pos > 0) {
            string prefix = substr(value, 0, colon_pos);
            string suffix = substr(value, colon_pos + 1);

            # Step 6.2: If prefix is "_" or suffix begins with "//"
            if (prefix == "_" || suffix =~ /^\/\//) {
                return value;
            }

            # Step 6.3: If prefix in local context, ensure it's defined
            if (exists local_context && exists defined && local_context.hasKey(prefix)
                    && !defined{prefix}) {
                JsonLdContext ctx_processor();
                ctx_processor.createTermDefinition(\active_context, local_context,
                    prefix, \defined);
            }

            # Step 6.4: If prefix has term definition with IRI mapping and prefix flag
            if (exists active_context.termDefinitions{prefix}) {
                *hash<JsonLdTermDefinition> prefix_td = active_context.termDefinitions{prefix};
                if (exists prefix_td.iriMapping && prefix_td.prefix) {
                    return prefix_td.iriMapping + suffix;
                }
            }

            # Step 6.5: If value is an absolute IRI, return it
            if (isAbsoluteIri(value)) {
                return value;
            }
        }

        # Step 7: If vocab is true and vocabulary mapping exists
        if (vocab && exists active_context.vocabularyMapping) {
            string expanded = active_context.vocabularyMapping + value;
            # If result is still relative (e.g., @vocab is ""), concatenate with base
            if (!isAbsoluteIri(expanded) && !isBlankNodeIdentifier(expanded)
                    && exists active_context.baseIri) {
                expanded = active_context.baseIri + expanded;
            }
            return expanded;
        }

        # Step 8: If document_relative, resolve against base IRI
        if (document_relative && exists active_context.baseIri) {
            return resolveIri(active_context.baseIri, value);
        }

        # Step 9: Return as-is
        return value;
    }

    #! Compacts an IRI using the active context
    /** Implements the IRI Compaction algorithm (W3C Section 6.2.2)
        Uses the Inverse Context for proper term selection.

        @param active_context the active context
        @param iri the IRI to compact
        @param value optional associated value (for container/type/language selection)
        @param vocab whether to compact relative to vocab
        @param reverse whether this is a reverse property

        @return the compacted IRI

        @since json 1.12
    */
    static string compactIri(hash<JsonLdActiveContext> active_context, string iri,
            *auto value, bool vocab = False, bool reverse = False) {
        # Step 1: If iri is null, return null (not applicable with string type)

        # Step 2-3: Build inverse context (cached on active context)
        if (!exists active_context.inverseContext) {
            active_context.inverseContext = buildInverseContext(active_context);
        }
        hash<auto> inverse = active_context.inverseContext;

        # Step 4: If vocab is true and iri is in inverse context
        if (vocab && exists inverse{iri}) {
            # Step 4.1: Determine default language
            string default_lang = active_context.defaultLanguage ?? "@none";
            if (exists active_context.defaultBaseDirection) {
                default_lang = default_lang.lwr() + "_" + active_context.defaultBaseDirection;
            } else if (exists active_context.defaultLanguage) {
                default_lang = default_lang.lwr();
            }

            # Step 4.3: Initialize containers
            list<string> containers = ();

            # Step 4.4: Initialize type/language tracking
            string type_lang = "@language";
            string type_lang_value = "@null";

            # Step 4.5: If value has @index (and not graph object)
            if (value.typeCode() == NT_HASH && exists value."@index"
                    && !isGraphObject(value)) {
                push containers, "@index";
                push containers, "@index@set";
            }

            # Step 4.6: If reverse
            if (reverse) {
                type_lang = "@type";
                type_lang_value = "@reverse";
                push containers, "@set";
            }
            # Step 4.7: If value is a list object
            else if (value.typeCode() == NT_HASH && isListObject(value)) {
                # Step 4.7.1
                if (!(value.typeCode() == NT_HASH && exists value."@index")) {
                    push containers, "@list";
                }
                # Step 4.7.2-8: Analyze list items for common type/language
                list<auto> lst = value."@list";
                *string common_lang;
                *string common_type;
                if (lst.empty()) {
                    common_lang = default_lang;
                }
                foreach auto item in (lst) {
                    string item_lang = "@none";
                    string item_type = "@none";
                    if (item.typeCode() == NT_HASH && item.hasKey("@value")) {
                        hash<auto> ih = item;
                        if (exists ih."@direction") {
                            item_lang = (ih."@language" ?? "").lwr() + "_" + ih."@direction";
                        } else if (exists ih."@language") {
                            item_lang = ih."@language".lwr();
                        } else if (exists ih."@type") {
                            item_type = ih."@type".typeCode() == NT_LIST
                                ? ih."@type"[0] : ih."@type";
                        } else {
                            item_lang = "@null";
                        }
                    } else {
                        item_type = "@id";
                    }
                    if (!exists common_lang) {
                        common_lang = item_lang;
                    } else if (item_lang != common_lang && item.typeCode() == NT_HASH
                            && item.hasKey("@value")) {
                        common_lang = "@none";
                    }
                    if (!exists common_type) {
                        common_type = item_type;
                    } else if (item_type != common_type) {
                        common_type = "@none";
                    }
                    if (common_lang == "@none" && common_type == "@none") {
                        break;
                    }
                }
                common_lang = common_lang ?? "@none";
                common_type = common_type ?? "@none";
                if (common_type != "@none") {
                    type_lang = "@type";
                    type_lang_value = common_type;
                } else {
                    type_lang_value = common_lang;
                }
            }
            # Step 4.8: If value is a graph object
            else if (value.typeCode() == NT_HASH && isGraphObject(value)) {
                hash<auto> vh = value;
                if (exists vh."@index") {
                    push containers, "@graph@index";
                    push containers, "@graph@index@set";
                }
                if (exists vh."@id") {
                    push containers, "@graph@id";
                    push containers, "@graph@id@set";
                }
                push containers, "@graph";
                push containers, "@graph@set";
                push containers, "@set";
                if (!exists vh."@index") {
                    push containers, "@graph@index";
                    push containers, "@graph@index@set";
                }
                if (!exists vh."@id") {
                    push containers, "@graph@id";
                    push containers, "@graph@id@set";
                }
                push containers, "@index";
                push containers, "@index@set";
                type_lang = "@type";
                type_lang_value = "@id";
            }
            # Step 4.9: Otherwise
            else {
                if (value.typeCode() == NT_HASH && value.hasKey("@value")) {
                    hash<auto> vh = value;
                    # Step 4.9.1.1: direction + no index
                    if (exists vh."@direction" && !exists vh."@index") {
                        type_lang_value = ((vh."@language" ?? "").lwr()
                            + "_" + vh."@direction");
                        push containers, "@language";
                        push containers, "@language@set";
                    }
                    # Step 4.9.1.2: language + no index
                    else if (exists vh."@language" && !exists vh."@index") {
                        type_lang_value = vh."@language".lwr();
                        push containers, "@language";
                        push containers, "@language@set";
                    }
                    # Step 4.9.1.3: @type
                    else if (exists vh."@type") {
                        type_lang_value = vh."@type".typeCode() == NT_LIST
                            ? vh."@type"[0] : vh."@type";
                        type_lang = "@type";
                    }
                    # Step 4.9.1 fallthrough: no lang/dir/type
                    else {
                        type_lang_value = "@null";
                    }
                } else {
                    # Step 4.9.2: Node object
                    type_lang = "@type";
                    type_lang_value = "@id";
                    push containers, "@id";
                    push containers, "@id@set";
                    push containers, "@type";
                    push containers, "@set@type";
                }
                # Step 4.9.3
                push containers, "@set";
            }

            # Step 4.10: Append @none
            push containers, "@none";

            # Step 4.11: If not 1.0 and no @index in value
            if (value.typeCode() != NT_HASH
                    || !value.hasKey("@index")) {
                push containers, "@index";
                push containers, "@index@set";
            }

            # Step 4.12: If not 1.0 and value has only @value
            if (value.typeCode() == NT_HASH) {
                hash<auto> vh = value;
                if (vh.size() == 1 && vh.hasKey("@value")) {
                    push containers, "@language";
                    push containers, "@language@set";
                }
            }

            # Step 4.14: Build preferred values
            list<string> preferred = ();

            # Step 4.15
            if (type_lang_value == "@reverse") {
                push preferred, "@reverse";
            }

            # Step 4.16: If @id or @reverse and value has @id
            if ((type_lang_value == "@id" || type_lang_value == "@reverse")
                    && value.typeCode() == NT_HASH
                    && exists value."@id") {
                string val_id = value."@id";
                string compacted_id = compactIri(active_context, val_id, NOTHING, True);
                if (exists active_context.termDefinitions{compacted_id}
                        && active_context.termDefinitions{compacted_id}.iriMapping == val_id) {
                    push preferred, "@vocab";
                    push preferred, "@id";
                    push preferred, "@none";
                } else {
                    push preferred, "@id";
                    push preferred, "@vocab";
                    push preferred, "@none";
                }
            } else {
                # Step 4.17: Append type/language value and @none to preferred values
                push preferred, type_lang_value;
                # If type_lang_value has direction (lang_dir format), add direction-only fallback
                if (type_lang_value.find("_") > 0) {
                    string dir_only = "_" + (type_lang_value.split("_"))[1];
                    push preferred, dir_only;
                }
                push preferred, "@none";
                # Add @null after @none for terms with explicit null direction/language
                # This ensures @none (generic) is preferred over @null (explicit null)
                if (type_lang_value != "@null") {
                    push preferred, "@null";
                }

                # Handle empty list case
                if (value.typeCode() == NT_HASH && isListObject(value)) {
                    list<auto> lst = value."@list";
                    if (lst.empty()) {
                        type_lang = "@any";
                        # For empty lists, any type/language matches
                        # Add common type patterns to preferred
                        push preferred, "@id";
                        push preferred, "@vocab";
                    }
                }
            }

            # Step 4.18
            push preferred, "@any";

            # Step 4.20: Term Selection using inverse context
            *string term = selectTerm(inverse{iri}, containers, type_lang, preferred);
            if (exists term) {
                return term;
            }
        }

        # Step 5: Try vocabulary mapping
        if (vocab && exists active_context.vocabularyMapping) {
            string vm = active_context.vocabularyMapping;
            if (vm == "" && exists active_context.baseIri) {
                # @vocab: "" means properties are relative to base
                # The expansion concatenated base + relative, so strip base to recover
                # Only use direct prefix match (not ../traversal) to ensure round-trip
                string base_prefix = active_context.baseIri;
                if (iri.find(base_prefix) == 0) {
                    string suffix = substr(iri, base_prefix.length());
                    if (suffix.length() > 0) {
                        if (!exists active_context.termDefinitions{suffix}
                                || active_context.termDefinitions{suffix}.iriMapping == iri) {
                            return suffix;
                        }
                    }
                }
                # Also check fragment-only and query-only forms
                # These are relative to the same base path
                hash<auto> base_parsed = parseIriComponents(active_context.baseIri);
                hash<auto> iri_parsed = parseIriComponents(iri);
                if (base_parsed.protocol == iri_parsed.protocol
                        && base_parsed.host == iri_parsed.host
                        && base_parsed.path == iri_parsed.path) {
                    if (exists iri_parsed.fragment) {
                        string frag = "#" + iri_parsed.fragment;
                        if (!exists active_context.termDefinitions{frag}
                                || active_context.termDefinitions{frag}.iriMapping == iri) {
                            return frag;
                        }
                    }
                    if (exists iri_parsed.query) {
                        string qry = "?" + iri_parsed.query;
                        if (exists iri_parsed.fragment) {
                            qry += "#" + iri_parsed.fragment;
                        }
                        if (!exists active_context.termDefinitions{qry}
                                || active_context.termDefinitions{qry}.iriMapping == iri) {
                            return qry;
                        }
                    }
                }
            } else if (vm != "" && iri.find(vm) == 0 && iri.length() > vm.length()) {
                string suffix = substr(iri, vm.length());
                if (!exists active_context.termDefinitions{suffix}
                        || active_context.termDefinitions{suffix}.iriMapping == iri) {
                    return suffix;
                }
            }
        }

        # Steps 6-8: Try compact IRI
        *string compact_iri;
        foreach string term in (sort(keys active_context.termDefinitions)) {
            hash<JsonLdTermDefinition> td = active_context.termDefinitions{term};
            # Step 7.1
            if (!exists td.iriMapping || td.iriMapping == iri
                    || iri.find(td.iriMapping) != 0 || !td.prefix) {
                continue;
            }

            string candidate = term + ":" + substr(iri, td.iriMapping.length());

            # Step 7.3: Prefer shorter/lexicographically less
            if (!exists compact_iri || candidate.length() < compact_iri.length()
                    || (candidate.length() == compact_iri.length()
                        && candidate < compact_iri)) {
                if (!exists active_context.termDefinitions{candidate}
                        || (active_context.termDefinitions{candidate}.iriMapping == iri
                            && !exists value)) {
                    compact_iri = candidate;
                }
            }
        }

        if (exists compact_iri) {
            return compact_iri;
        }

        # Step 9: Check for IRI confused with prefix
        # If the IRI looks like a compact IRI (prefix:suffix) and the prefix
        # matches a term with prefix flag, the IRI would be misinterpreted
        int ci_colon = index(iri, ":");
        if (ci_colon > 0) {
            string ci_prefix = substr(iri, 0, ci_colon);
            if (exists active_context.termDefinitions{ci_prefix}
                    && active_context.termDefinitions{ci_prefix}.prefix
                    && substr(iri, ci_colon + 1) !~ /^\/\//) {
                throwJsonLdError(JSONLD_ERR_IRI_CONFUSED_WITH_PREFIX,
                    sprintf("IRI %y would be confused with compact IRI using prefix %y",
                        iri, ci_prefix));
            }
        }

        # Step 10: If not vocab, try to make relative to base
        if (!vocab && exists active_context.baseIri) {
            *string relative_iri = computeRelativeIri(active_context.baseIri, iri);
            if (exists relative_iri) {
                return relative_iri;
            }
        }

        # Step 11: Return as-is
        return iri;
    }

    #! Computes a relative IRI from an absolute IRI given a base
    /** Implements RFC 3986 relative reference computation.
        Returns NOTHING if no relative form is possible.
    */
    private static *string computeRelativeIri(string base, string iri) {
        # Parse both IRIs
        hash<auto> base_parts = parseIriComponents(base);
        hash<auto> iri_parts = parseIriComponents(iri);

        # Must have same scheme and authority
        if (base_parts.protocol != iri_parts.protocol) {
            return NOTHING;
        }
        if (base_parts.host != iri_parts.host) {
            return NOTHING;
        }

        # Fragment-only difference
        if (base_parts.path == iri_parts.path && base_parts.query == iri_parts.query
                && exists iri_parts.fragment) {
            return "#" + iri_parts.fragment;
        }

        # Query difference (same path)
        if (base_parts.path == iri_parts.path && exists iri_parts.query) {
            string result = "?" + iri_parts.query;
            if (exists iri_parts.fragment) {
                result += "#" + iri_parts.fragment;
            }
            return result;
        }

        # Compute relative path
        string base_path = base_parts.path;
        string iri_path = iri_parts.path;

        # Get base directory (strip filename)
        int last_slash = rindex(base_path, "/");
        string base_dir;
        if (last_slash >= 0) {
            base_dir = substr(base_path, 0, last_slash + 1);
        } else {
            base_dir = "/";
        }

        # Split paths into segments
        list<string> base_segs = base_dir.split("/");
        list<string> iri_segs = iri_path.split("/");

        # Remove empty first segment (from leading /)
        if (base_segs.size() > 0 && base_segs[0] == "") {
            splice base_segs, 0, 1;
        }
        if (iri_segs.size() > 0 && iri_segs[0] == "") {
            splice iri_segs, 0, 1;
        }

        # Find common prefix length
        int common = 0;
        int max_common = base_segs.size() < iri_segs.size()
            ? base_segs.size() : iri_segs.size();
        while (common < max_common && base_segs[common] == iri_segs[common]) {
            ++common;
        }

        # Build relative path: "../" for each remaining base segment, then IRI segments
        string relative = "";
        for (int i = common; i < base_segs.size(); ++i) {
            if (base_segs[i] != "") {
                relative += "../";
            }
        }
        for (int i = common; i < iri_segs.size(); ++i) {
            if (i > common) {
                relative += "/";
            }
            relative += iri_segs[i];
        }

        # Avoid returning empty string or absolute IRI
        if (relative == "" || isAbsoluteIri(relative)) {
            # Same-document reference: use "./"
            relative = "./" + relative;
        }

        # Add query and fragment
        if (exists iri_parts.query) {
            relative += "?" + iri_parts.query;
        }
        if (exists iri_parts.fragment) {
            relative += "#" + iri_parts.fragment;
        }

        # If relative IRI looks like a keyword, prefix with "./" to avoid ambiguity
        if (relative =~ /^@/) {
            relative = "./" + relative;
        }

        return relative;
    }

    #! Builds the inverse context for IRI compaction (W3C 6.2.1)
    /** Maps each IRI to a multi-level structure:
        IRI → container_key → type_or_language → type_or_language_value → term
    */
    static hash<auto> buildInverseContext(hash<JsonLdActiveContext> active_context) {
        hash<auto> result = {};
        # Process terms in sorted order for deterministic selection
        foreach string term in (sort(keys active_context.termDefinitions)) {
            hash<JsonLdTermDefinition> td = active_context.termDefinitions{term};
            if (!exists td.iriMapping) {
                continue;
            }
            string var = td.iriMapping;
            if (!exists result{var}) {
                result{var} = {};
            }

            # Build container key from container mapping
            string container_key;
            if (exists td.containerMapping && td.containerMapping.size() > 0) {
                container_key = (foldl $1 + $2, sort(td.containerMapping));
            } else {
                container_key = "@none";
            }

            if (!exists result{var}{container_key}) {
                result{var}{container_key} = {"@language": {}, "@type": {}, "@any": {}};
            }

            hash<auto> type_lang_map = result{var}{container_key};

            # Reverse property
            if (td.reverse) {
                if (!exists type_lang_map."@type"."@reverse") {
                    type_lang_map."@type"."@reverse" = term;
                    result{var}{container_key} = type_lang_map;
                }
            }
            # Type mapping
            else if (exists td.typeMapping) {
                if (!exists type_lang_map."@type"{td.typeMapping}) {
                    type_lang_map."@type"{td.typeMapping} = term;
                    result{var}{container_key} = type_lang_map;
                }
                # @type: @none matches any value — also populate @language and @any
                if (td.typeMapping == "@none") {
                    if (!exists type_lang_map."@language"."@none") {
                        type_lang_map."@language"."@none" = term;
                    }
                    if (!exists type_lang_map."@any"."@none") {
                        type_lang_map."@any"."@none" = term;
                    }
                    result{var}{container_key} = type_lang_map;
                }
            }
            # Language + direction mapping
            else if (td.hasLanguageMapping && td.hasDirectionMapping) {
                string lang_dir;
                if (exists td.languageMapping && exists td.directionMapping) {
                    lang_dir = td.languageMapping.lwr() + "_" + td.directionMapping;
                } else if (exists td.languageMapping) {
                    lang_dir = td.languageMapping.lwr();
                } else if (exists td.directionMapping) {
                    lang_dir = "_" + td.directionMapping;
                } else {
                    lang_dir = "@null";
                }
                if (!exists type_lang_map."@language"{lang_dir}) {
                    type_lang_map."@language"{lang_dir} = term;
                    result{var}{container_key} = type_lang_map;
                }
            }
            # Language mapping only
            else if (td.hasLanguageMapping) {
                string lang_key = exists td.languageMapping
                    ? td.languageMapping.lwr() : "@null";
                if (!exists type_lang_map."@language"{lang_key}) {
                    type_lang_map."@language"{lang_key} = term;
                    result{var}{container_key} = type_lang_map;
                }
            }
            # Direction mapping only
            else if (td.hasDirectionMapping) {
                string dir_key = exists td.directionMapping
                    ? "_" + td.directionMapping : "@null";
                if (!exists type_lang_map."@language"{dir_key}) {
                    type_lang_map."@language"{dir_key} = term;
                    result{var}{container_key} = type_lang_map;
                }
            }
            # No type or language mapping — use as default for @language, @type, and @any
            else {
                # Terms with @language container should not be default language matches
                # — they accept values via the container mechanism, not language matching
                bool is_lang_container = exists td.containerMapping
                    && inlist("@language", td.containerMapping);
                if (!is_lang_container) {
                    if (!exists type_lang_map."@language"."@none") {
                        type_lang_map."@language"."@none" = term;
                    }
                }
                if (!exists type_lang_map."@type"."@none") {
                    type_lang_map."@type"."@none" = term;
                }
                if (!exists type_lang_map."@any"."@none") {
                    type_lang_map."@any"."@none" = term;
                }
            }

            result{var}{container_key} = type_lang_map;
        }
        return result;
    }

    #! Term Selection algorithm (W3C 6.2.3)
    /** Selects the best term from the inverse context entry for an IRI.

        @param iri_entry the inverse context entry for the IRI
        @param containers preferred container list
        @param type_lang "@language", "@type", or "@any"
        @param preferred preferred type/language values

        @return the selected term or NOTHING
    */
    private static *string selectTerm(hash<auto> iri_entry, list<string> containers,
            string type_lang, list<string> preferred) {
        foreach string container in (containers) {
            if (!exists iri_entry{container}) {
                continue;
            }
            hash<auto> container_entry = iri_entry{container};

            # Check type_lang map
            if (exists container_entry{type_lang}) {
                hash<auto> value_map = container_entry{type_lang};
                foreach string pref in (preferred) {
                    if (exists value_map{pref}) {
                        return value_map{pref};
                    }
                }
            }

            # When type_lang is @any, check all dimensions
            if (type_lang == "@any") {
                foreach string dim in (("@language", "@type")) {
                    if (exists container_entry{dim}) {
                        hash<auto> dim_map = container_entry{dim};
                        foreach string pref in (preferred) {
                            if (exists dim_map{pref}) {
                                return dim_map{pref};
                            }
                        }
                    }
                }
            }

            # Also check @any dimension
            if (type_lang != "@any" && exists container_entry."@any") {
                hash<auto> any_map = container_entry."@any";
                foreach string pref in (preferred) {
                    if (exists any_map{pref}) {
                        return any_map{pref};
                    }
                }
            }
        }

        return NOTHING;
    }
}

} # namespace JsonLd
