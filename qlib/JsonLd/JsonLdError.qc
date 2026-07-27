# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdError.qc @brief JSON-LD W3C error codes and error handling

/*  JsonLdError.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

# ============================================================================
# W3C JSON-LD Error Codes
# ============================================================================

#! Colliding keywords error
public const JSONLD_ERR_COLLIDING_KEYWORDS = "colliding keywords";
#! Context overflow error
public const JSONLD_ERR_CONTEXT_OVERFLOW = "context overflow";
#! Cyclic IRI mapping error
public const JSONLD_ERR_CYCLIC_IRI_MAPPING = "cyclic IRI mapping";
#! Invalid \@id value error
public const JSONLD_ERR_INVALID_ID_VALUE = "invalid @id value";
#! Invalid \@import value error
public const JSONLD_ERR_INVALID_IMPORT_VALUE = "invalid @import value";
#! Invalid \@included value error
public const JSONLD_ERR_INVALID_INCLUDED_VALUE = "invalid @included value";
#! Invalid \@index value error
public const JSONLD_ERR_INVALID_INDEX_VALUE = "invalid @index value";
#! Invalid \@nest value error
public const JSONLD_ERR_INVALID_NEST_VALUE = "invalid @nest value";
#! Invalid \@prefix value error
public const JSONLD_ERR_INVALID_PREFIX_VALUE = "invalid @prefix value";
#! Invalid \@propagate value error
public const JSONLD_ERR_INVALID_PROPAGATE_VALUE = "invalid @propagate value";
#! Invalid \@protected value error
public const JSONLD_ERR_INVALID_PROTECTED_VALUE = "invalid @protected value";
#! Invalid \@reverse value error
public const JSONLD_ERR_INVALID_REVERSE_VALUE = "invalid @reverse value";
#! Invalid \@version value error
public const JSONLD_ERR_INVALID_VERSION_VALUE = "invalid @version value";
#! Invalid base IRI error
public const JSONLD_ERR_INVALID_BASE_IRI = "invalid base IRI";
#! Invalid base direction error
public const JSONLD_ERR_INVALID_BASE_DIRECTION = "invalid base direction";
#! Invalid container mapping error
public const JSONLD_ERR_INVALID_CONTAINER_MAPPING = "invalid container mapping";
#! Invalid context entry error
public const JSONLD_ERR_INVALID_CONTEXT_ENTRY = "invalid context entry";
#! Invalid context nullification error
public const JSONLD_ERR_INVALID_CONTEXT_NULLIFICATION = "invalid context nullification";
#! Invalid default language error
public const JSONLD_ERR_INVALID_DEFAULT_LANGUAGE = "invalid default language";
#! Invalid IRI mapping error
public const JSONLD_ERR_INVALID_IRI_MAPPING = "invalid IRI mapping";
#! Invalid JSON literal error
public const JSONLD_ERR_INVALID_JSON_LITERAL = "invalid JSON literal";
#! Invalid keyword alias error
public const JSONLD_ERR_INVALID_KEYWORD_ALIAS = "invalid keyword alias";
#! Invalid language map value error
public const JSONLD_ERR_INVALID_LANGUAGE_MAP_VALUE = "invalid language map value";
#! Invalid language mapping error
public const JSONLD_ERR_INVALID_LANGUAGE_MAPPING = "invalid language mapping";
#! Invalid language-tagged string error
public const JSONLD_ERR_INVALID_LANGUAGE_TAGGED_STRING = "invalid language-tagged string";
#! Invalid language-tagged value error
public const JSONLD_ERR_INVALID_LANGUAGE_TAGGED_VALUE = "invalid language-tagged value";
#! Invalid local context error
public const JSONLD_ERR_INVALID_LOCAL_CONTEXT = "invalid local context";
#! Invalid remote context error
public const JSONLD_ERR_INVALID_REMOTE_CONTEXT = "invalid remote context";
#! Invalid reverse property error
public const JSONLD_ERR_INVALID_REVERSE_PROPERTY = "invalid reverse property";
#! Invalid reverse property map error
public const JSONLD_ERR_INVALID_REVERSE_PROPERTY_MAP = "invalid reverse property map";
#! Invalid reverse property value error
public const JSONLD_ERR_INVALID_REVERSE_PROPERTY_VALUE = "invalid reverse property value";
#! Invalid scoped context error
public const JSONLD_ERR_INVALID_SCOPED_CONTEXT = "invalid scoped context";
#! Invalid set or list object error
public const JSONLD_ERR_INVALID_SET_OR_LIST_OBJECT = "invalid set or list object";
#! Invalid term definition error
public const JSONLD_ERR_INVALID_TERM_DEFINITION = "invalid term definition";
#! Invalid type mapping error
public const JSONLD_ERR_INVALID_TYPE_MAPPING = "invalid type mapping";
#! Invalid type value error
public const JSONLD_ERR_INVALID_TYPE_VALUE = "invalid type value";
#! Invalid typed value error
public const JSONLD_ERR_INVALID_TYPED_VALUE = "invalid typed value";
#! Invalid value object error
public const JSONLD_ERR_INVALID_VALUE_OBJECT = "invalid value object";
#! Invalid value object value error
public const JSONLD_ERR_INVALID_VALUE_OBJECT_VALUE = "invalid value object value";
#! Invalid vocab mapping error
public const JSONLD_ERR_INVALID_VOCAB_MAPPING = "invalid vocab mapping";
#! IRI confused with prefix error
public const JSONLD_ERR_IRI_CONFUSED_WITH_PREFIX = "IRI confused with prefix";
#! Keyword redefinition error
public const JSONLD_ERR_KEYWORD_REDEFINITION = "keyword redefinition";
#! Loading document failed error
public const JSONLD_ERR_LOADING_DOCUMENT_FAILED = "loading document failed";
#! Loading remote context failed error
public const JSONLD_ERR_LOADING_REMOTE_CONTEXT_FAILED = "loading remote context failed";
#! Multiple context link headers error
public const JSONLD_ERR_MULTIPLE_CONTEXT_LINK_HEADERS = "multiple context link headers";
#! Processing mode conflict error
public const JSONLD_ERR_PROCESSING_MODE_CONFLICT = "processing mode conflict";
#! Protected term redefinition error
public const JSONLD_ERR_PROTECTED_TERM_REDEFINITION = "protected term redefinition";

# ============================================================================
# Error Info Hashdecl
# ============================================================================

#! JSON-LD error information
/** @since json 1.12
*/
public hashdecl JsonLdErrorInfo {
    #! W3C error code
    string code;

    #! Human-readable error message
    string message;

    #! Additional error details
    *hash<auto> details;
}

# ============================================================================
# Error Throwing Functions
# ============================================================================

#! Throws a JSON-LD-ERROR exception with W3C error code
/** @param code the W3C error code
    @param message human-readable error message
    @param details optional additional details

    @throw JSON-LD-ERROR with hash<JsonLdErrorInfo> as the exception argument

    @since json 1.12
*/
public sub throwJsonLdError(string code, string message, *hash<auto> details) {
    throw "JSON-LD-ERROR", message, <JsonLdErrorInfo>{
        "code": code,
        "message": message,
        "details": details,
    };
}

} # namespace JsonLd
