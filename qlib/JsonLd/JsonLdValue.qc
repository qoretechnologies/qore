# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdValue.qc @brief JSON-LD Value expansion and compaction

/*  JsonLdValue.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! JSON-LD Value expansion and compaction
/** Implements Value Expansion (W3C 4.3.5) and Value Compaction (W3C 4.5.5).

    @since json 1.12
*/
public class JsonLdValue {
    #! Expands a value based on the active context and active property
    /** Implements the Value Expansion algorithm (W3C Section 4.3.5)

        @param active_context the active context
        @param active_property the active property (term)
        @param value the value to expand

        @return the expanded value (a value object or \@id reference)

        @since json 1.12
    */
    static auto expandValue(hash<JsonLdActiveContext> active_context, string active_property, auto value) {
        *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};

        # 1. If active property has type mapping of @id
        if (exists td && td.typeMapping == "@id") {
            if (value.typeCode() == NT_STRING) {
                *string expanded = JsonLdIri::expandIri(active_context, value, False, True);
                hash<auto!> id_ref = {"@id": expanded};
                return id_ref;
            }
        }

        # 2. If active property has type mapping of @vocab
        if (exists td && td.typeMapping == "@vocab") {
            if (value.typeCode() == NT_STRING) {
                *string expanded = JsonLdIri::expandIri(active_context, value, True, True);
                hash<auto!> id_ref = {"@id": expanded};
                return id_ref;
            }
        }

        # 3. Create a value object (auto! to prevent type narrowing from value)
        hash<auto!> result = {"@value": value};

        # 4. If active property has type mapping
        if (exists td && exists td.typeMapping && td.typeMapping != "@id" && td.typeMapping != "@vocab"
                && td.typeMapping != "@none") {
            if (td.typeMapping == "@json") {
                result."@type" = "@json";
            } else {
                result."@type" = td.typeMapping;
            }
        }
        # 5. Else if value is a string
        else if (value.typeCode() == NT_STRING) {
            # 5.1 If active property has language mapping
            if (exists td && td.hasLanguageMapping) {
                if (exists td.languageMapping) {
                    result."@language" = td.languageMapping;
                }
            } else if (exists active_context.defaultLanguage) {
                result."@language" = active_context.defaultLanguage;
            }

            # 5.2 If active property has direction mapping
            if (exists td && td.hasDirectionMapping) {
                if (exists td.directionMapping) {
                    result."@direction" = td.directionMapping;
                }
            } else if (exists active_context.defaultBaseDirection) {
                result."@direction" = active_context.defaultBaseDirection;
            }
        }

        return result;
    }

    #! Compacts a value object for the active context and property
    /** Implements the Value Compaction algorithm (W3C Section 4.5.5)

        @param active_context the active context
        @param active_property the active property
        @param value the expanded value to compact

        @return the compacted value

        @since json 1.12
    */
    static auto compactValue(hash<JsonLdActiveContext> active_context, string active_property,
            hash<auto> value) {
        *hash<JsonLdTermDefinition> td = active_context.termDefinitions{active_property};

        # If value has @id and is an IRI reference (no @value, no @type, no @language)
        if (exists value."@id" && !exists value."@value") {
            # If type mapping is @id
            if (exists td && td.typeMapping == "@id") {
                return JsonLdIri::compactIri(active_context, value."@id");
            }
            # If type mapping is @vocab
            if (exists td && td.typeMapping == "@vocab") {
                return JsonLdIri::compactIri(active_context, value."@id", NOTHING, True);
            }
        }

        # Value object (use hasKey to detect {"@value": null})
        if (value.hasKey("@value")) {
            # If term has @type: @none, values must be kept as value objects
            if (exists td && td.typeMapping == "@none") {
                return value;
            }

            # If value has @index, it can only be compacted to a plain value
            # if the term has @index container (which strips @index)
            if (exists value."@index") {
                bool has_index_container = exists td && exists td.containerMapping
                    && inlist("@index", td.containerMapping);
                if (!has_index_container) {
                    # Keep as object — @index must be preserved
                    return value;
                }
                # Strip @index for further compaction checks
            }

            # If has @type that matches term type mapping
            if (exists value."@type" && exists td && td.typeMapping == value."@type") {
                return value."@value";
            }

            # If @type is @json, return as-is
            if (value."@type" == "@json") {
                return value;
            }

            # If value has @type that doesn't match term type mapping, keep as object
            if (exists value."@type") {
                return value;
            }

            # If value is a string and matches language/direction
            if (value."@value".typeCode() == NT_STRING) {
                *string value_lang = value."@language";
                *string value_dir = value."@direction";

                # Check if language matches term or context default
                *string term_lang = exists td && td.hasLanguageMapping
                    ? td.languageMapping
                    : active_context.defaultLanguage;
                *string term_dir = exists td && td.hasDirectionMapping
                    ? td.directionMapping
                    : active_context.defaultBaseDirection;

                if (value_lang == term_lang && value_dir == term_dir) {
                    return value."@value";
                }
            }

            # If no language/direction/type info, return the plain value
            if (!exists value."@language" && !exists value."@direction"
                    && !exists value."@type") {
                # Non-string values (numbers, booleans) can always be compacted to plain
                if (value."@value".typeCode() != NT_STRING) {
                    return value."@value";
                }
                # If no term definition and no context defaults, return plain value
                if (!exists td && !exists active_context.defaultLanguage
                        && !exists active_context.defaultBaseDirection) {
                    return value."@value";
                }
                # If term has no language/direction mapping and no context defaults apply
                if (exists td && !td.hasLanguageMapping && !td.hasDirectionMapping
                        && !exists active_context.defaultLanguage
                        && !exists active_context.defaultBaseDirection) {
                    return value."@value";
                }
                # If term has explicit null language mapping
                if (exists td && td.hasLanguageMapping && !exists td.languageMapping) {
                    if (!exists td.directionMapping || !td.hasDirectionMapping) {
                        return value."@value";
                    }
                }
            }
        }

        return value;
    }
}

} # namespace JsonLd
