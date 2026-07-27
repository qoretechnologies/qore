# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdUtils.qc @brief JSON-LD shared utilities

/*  JsonLdUtils.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Returns True if the given string is a JSON-LD keyword
/** @param str the string to check
    @return True if \a str is a JSON-LD keyword

    @since json 1.12
*/
public bool sub isJsonLdKeyword(string str) {
    return JSONLD_KEYWORDS{str} ?? False;
}

#! Returns True if the given string looks like a JSON-LD keyword (starts with @)
/** @param str the string to check
    @return True if \a str starts with @

    @since json 1.12
*/
public bool sub isKeywordLike(string str) {
    return str =~ /^@[a-zA-Z]+$/;
}

#! Returns True if the given string is an absolute IRI
/** An absolute IRI contains a scheme followed by ":"
    @param str the string to check
    @return True if \a str is an absolute IRI

    @since json 1.12
*/
public bool sub isAbsoluteIri(string str) {
    return str =~ /^[a-zA-Z][a-zA-Z0-9+\-.]*:/;
}

#! Returns True if the given string is a relative IRI
/** @param str the string to check
    @return True if \a str is a relative IRI (not absolute and not blank node)

    @since json 1.12
*/
public bool sub isRelativeIri(string str) {
    return !isAbsoluteIri(str) && !isBlankNodeIdentifier(str);
}

#! Returns True if the given string is a blank node identifier
/** Blank node identifiers start with "_:"
    @param str the string to check
    @return True if \a str is a blank node identifier

    @since json 1.12
*/
public bool sub isBlankNodeIdentifier(string str) {
    return str =~ /^_:/;
}

#! Returns True if the given string is a compact IRI (contains ":" but is not absolute IRI or blank node)
/** @param str the string to check
    @return True if \a str is a compact IRI (prefix:suffix format)

    @since json 1.12
*/
public bool sub isCompactIri(string str) {
    # Must contain ":" but not start with "_:" (blank node) and not be a keyword
    if (str =~ /^@/ || str =~ /^_:/) {
        return False;
    }
    int pos = index(str, ":");
    if (pos < 1) {
        return False;
    }
    # Check that the prefix is a valid prefix (no "//" immediately after the ":")
    string rest = substr(str, pos + 1);
    if (rest =~ /^\/\//) {
        return False;
    }
    return True;
}

#! Resolves a relative IRI against a base IRI
/** Implements RFC 3986 Section 5.3 reference resolution
    @param base the base IRI
    @param relative the relative IRI to resolve
    @return the resolved absolute IRI

    @since json 1.12
*/
#! Parses an IRI into its components per RFC 3986 Appendix B
/** Works for ALL URI schemes, not just HTTP.
    @param iri the IRI to parse
    @return hash with keys: protocol, host, port, path, query, fragment
*/
public hash<auto> sub parseIriComponents(string iri) {
    hash<auto!> result = {};
    # RFC 3986 Appendix B regex:
    # ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?$
    # Qore capture indices (0-based):
    #   0 = (scheme:)    1 = (scheme)
    #   2 = (//authority) 3 = (authority)
    #   4 = (path)
    #   5 = (?query)     6 = (query)
    #   7 = (#fragment)  8 = (fragment)
    # NOTE: Qore only returns groups up to the last matched one
    *list<auto> m = (iri =~ x/^(([^:\/?#]+):)?(\/\/([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?$/s);
    if (m && m.size() > 0) {
        if (m.size() > 1 && exists m[1]) {
            result.protocol = m[1];
        }
        if (m.size() > 2 && exists m[2]) {
            # authority component exists (even if empty, e.g. "http://")
            result.host = (m.size() > 3 && exists m[3]) ? m[3] : "";
        }
        if (m.size() > 4) {
            result.path = m[4] ?? "";
        } else {
            result.path = "";
        }
        if (m.size() > 5 && exists m[5]) {
            result.query = (m.size() > 6) ? (m[6] ?? "") : "";
        }
        if (m.size() > 7 && exists m[7]) {
            result.fragment = (m.size() > 8) ? (m[8] ?? "") : "";
        }
    } else {
        result.path = iri;
    }
    return result;
}

public string sub resolveIri(string base, string relative) {
    # RFC 3986 Section 5.2.2 — Reference Resolution

    # Parse R (reference)
    hash<auto> R = parseIriComponents(relative);

    # If R has a scheme, it's absolute — use R with dot segments removed
    if (exists R.protocol) {
        string T_path = removeDotSegmentsPath(R.path);
        return recomposeIri(R.protocol, R.host, T_path, R.query, R.fragment);
    }

    # Parse Base
    hash<auto> B = parseIriComponents(base);

    string T_scheme = B.protocol ?? "";
    *string T_authority;
    string T_path;
    *string T_query;
    *string T_fragment = R.fragment;

    if (exists R.host) {
        # R has authority
        T_authority = R.host;
        T_path = removeDotSegmentsPath(R.path);
        T_query = R.query;
    } else {
        T_authority = B.host;
        if (R.path == "") {
            # Empty path in R
            T_path = B.path;
            if (exists R.query) {
                T_query = R.query;
            } else {
                T_query = B.query;
            }
        } else {
            if (R.path =~ /^\//) {
                # Absolute path in R
                T_path = removeDotSegmentsPath(R.path);
            } else {
                # Relative path — merge with base
                string merged;
                if (exists B.host && B.path == "") {
                    # RFC 3986 5.2.3: If base has authority and empty path
                    merged = "/" + R.path;
                } else {
                    # Remove everything after last "/" in base path
                    int last_slash = rindex(B.path, "/");
                    if (last_slash >= 0) {
                        merged = substr(B.path, 0, last_slash + 1) + R.path;
                    } else {
                        merged = R.path;
                    }
                }
                T_path = removeDotSegmentsPath(merged);
            }
            T_query = R.query;
        }
    }

    return recomposeIri(T_scheme, T_authority, T_path, T_query, T_fragment);
}

#! Recomposes a URI from its components per RFC 3986 Section 5.3
public string sub recomposeIri(string scheme, *string authority, string path,
        *string query, *string fragment) {
    string result = scheme + ":";
    if (exists authority) {
        result += "//" + authority;
    }
    result += path;
    if (exists query) {
        result += "?" + query;
    }
    if (exists fragment) {
        result += "#" + fragment;
    }
    return result;
}

#! Removes dot segments from a path per RFC 3986 Section 5.2.4
public string sub removeDotSegmentsPath(string path) {
    # Input buffer
    string input = path;
    # Output segments
    list<string> output = ();

    while (input.length() > 0) {
        # A: remove ../  or ./  prefix
        if (input =~ /^\.\.\//) {
            input = substr(input, 3);
        } else if (input =~ /^\.\//) {
            input = substr(input, 2);
        }
        # B: remove /./  or /.  prefix
        else if (input =~ /^\/\.\//) {
            input = "/" + substr(input, 3);
        } else if (input == "/.") {
            input = "/";
        }
        # C: remove /../ or /.. prefix, remove last segment
        else if (input =~ /^\/\.\.\//) {
            input = "/" + substr(input, 4);
            if (output.size() > 0) {
                pop output;
            }
        } else if (input == "/..") {
            input = "/";
            if (output.size() > 0) {
                pop output;
            }
        }
        # D: remove . or ..
        else if (input == "." || input == "..") {
            input = "";
        }
        # E: move first path segment to output
        else {
            *list<string> seg_match = (input =~ x/^(\/[^\/]*)(.*)$/s);
            if (seg_match) {
                push output, seg_match[0];
                input = seg_match[1];
            } else {
                seg_match = (input =~ x/^([^\/]+)(.*)$/s);
                if (seg_match) {
                    push output, seg_match[0];
                    input = seg_match[1];
                } else {
                    break;
                }
            }
        }
    }

    if (output.empty()) {
        return "";
    }
    return foldl $1 + $2, output;
}

#! Removes dot segments from a URI path per RFC 3986 Section 5.2.4
/** @param uri the URI to process
    @return the URI with dot segments removed

    @since json 1.12
*/
public string sub removeDotSegments(string uri) {
    # Find the path portion
    # Parse scheme + authority
    *string scheme;
    *string authority;
    string path;
    *string query_fragment;

    # Extract scheme
    *list<string> scheme_match = (uri =~ x/^([a-zA-Z][a-zA-Z0-9+\-.]*):(.*)$/s);
    if (scheme_match) {
        scheme = scheme_match[0];
        string rest = scheme_match[1];

        # Extract authority
        *list<string> auth_match = (rest =~ x/^\/\/([^\/?#]*)(.*)$/s);
        if (auth_match) {
            authority = auth_match[0];
            rest = auth_match[1];
        }

        # Extract query/fragment
        *list<string> pq_match = (rest =~ x/^([^?#]*)(.*)$/s);
        if (pq_match) {
            path = pq_match[0];
            query_fragment = pq_match[1];
        } else {
            path = rest;
        }
    } else {
        path = uri;
    }

    # Remove dot segments from path
    list<string> output = ();
    while (path.length() > 0) {
        # A: remove ../  or ./  prefix
        if (path =~ /^\.\.\//) {
            path = substr(path, 3);
        } else if (path =~ /^\.\//) {
            path = substr(path, 2);
        }
        # B: remove /./  or /.  prefix
        else if (path =~ /^\/\.\//) {
            path = "/" + substr(path, 3);
        } else if (path == "/.") {
            path = "/";
        }
        # C: remove /../ or /.. prefix, remove last segment
        else if (path =~ /^\/\.\.\//) {
            path = "/" + substr(path, 4);
            if (output.size() > 0) {
                pop output;
            }
        } else if (path == "/..") {
            path = "/";
            if (output.size() > 0) {
                pop output;
            }
        }
        # D: remove . or ..
        else if (path == "." || path == "..") {
            path = "";
        }
        # E: move first path segment to output
        else {
            *list<string> seg_match = (path =~ x/^(\/[^\/]*)(.*)$/s);
            if (seg_match) {
                push output, seg_match[0];
                path = seg_match[1];
            } else {
                seg_match = (path =~ x/^([^\/]+)(.*)$/s);
                if (seg_match) {
                    push output, seg_match[0];
                    path = seg_match[1];
                } else {
                    break;
                }
            }
        }
    }

    # Reassemble
    string result = "";
    if (exists scheme) {
        result += scheme + ":";
    }
    if (exists authority) {
        result += "//" + authority;
    }
    result += (foldl $1 + $2, output) ?? "";
    if (exists query_fragment) {
        result += query_fragment;
    }

    return result;
}

#! Returns True if the value is a JSON-LD value object (contains \@value)
/** @param v the value to check
    @return True if \a v is a hash with an \@value key

    @since json 1.12
*/
public bool sub isValueObject(auto v) {
    # Use hasKey() to detect {"@value": null} — exists returns False for null in Qore
    return v.typeCode() == NT_HASH && v.hasKey("@value");
}

#! Returns True if the value is a JSON-LD list object (contains \@list)
/** @param v the value to check
    @return True if \a v is a hash with an \@list key

    @since json 1.12
*/
public bool sub isListObject(auto v) {
    return v.typeCode() == NT_HASH && v.hasKey("@list");
}

#! Returns True if the value is a JSON-LD graph object (contains \@graph)
/** @param v the value to check
    @return True if \a v is a hash with an \@graph key

    @since json 1.12
*/
public bool sub isGraphObject(auto v) {
    if (v.typeCode() != NT_HASH) {
        return False;
    }
    hash<auto> h = v;
    return exists h."@graph";
}

#! Returns True if the value is a simple graph object (\@graph with only \@id and \@index)
/** @param v the value to check
    @return True if \a v is a graph object with only \@graph, \@id, and/or \@index keys

    @since json 1.12
*/
public bool sub isSimpleGraphObject(auto v) {
    if (!isGraphObject(v)) {
        return False;
    }
    hash<auto> h = v;
    foreach string key in (keys h) {
        if (key != "@graph" && key != "@id" && key != "@index") {
            return False;
        }
    }
    return True;
}

#! Returns True if the value is a JSON-LD node object
/** A node object is a hash that is not a value object, list object, or set object
    @param v the value to check
    @return True if \a v is a node object

    @since json 1.12
*/
public bool sub isNodeObject(auto v) {
    if (v.typeCode() != NT_HASH) {
        return False;
    }
    hash<auto> h = v;
    return !exists h."@value" && !exists h."@list" && !exists h."@set";
}

#! Returns True if the value is a well-formed IRI
/** @param str the string to check
    @return True if \a str is a well-formed IRI (absolute or blank node)

    @since json 1.12
*/
public bool sub isWellFormedIri(string str) {
    return isAbsoluteIri(str) || isBlankNodeIdentifier(str);
}

#! Ensures a value is a list; wraps non-list values in a single-element list
/** @param v the value to wrap
    @return \a v if it's already a list, otherwise a list containing \a v

    @since json 1.12
*/
public list<auto> sub asList(auto v) {
    if (!exists v) {
        return ();
    }
    if (v.typeCode() == NT_LIST) {
        return v;
    }
    return (v,);
}

#! Deep clone a value (hash/list/scalar)
/** @param v the value to clone
    @return a deep copy of \a v

    @since json 1.12
*/
public auto sub jsonLdClone(auto v) {
    switch (v.typeCode()) {
        case NT_HASH: {
            hash<auto> result = {};
            map (result{$1.key} = jsonLdClone($1.value)), v.pairIterator();
            return result;
        }
        case NT_LIST: {
            return map jsonLdClone($1), v;
        }
        default:
            return v;
    }
}

#! Fixes CESU-8 surrogate pairs in a string to proper UTF-8
/** Qore's JSON parser encodes UTF-16 surrogate pairs (\\uD800-\\uDFFF) as
    individual CESU-8 sequences (3 bytes each = 6 bytes per surrogate pair)
    instead of the correct 4-byte UTF-8 encoding. This function detects
    and replaces such sequences.

    @param s the string to fix
    @return the string with surrogate pairs replaced by proper UTF-8

    @since json 1.12
*/
public string sub fixSurrogatePairsInString(string s) {
    binary b = binary(s);
    int len = b.size();
    # Quick check: does the string contain the CESU-8 high-surrogate lead byte?
    # High surrogates (U+D800..U+DBFF) encode as: ed [a0-af] xx
    bool has_surrogate = False;
    for (int i = 0; i < len; ++i) {
        if (b[i] == 0xed) {
            has_surrogate = True;
            break;
        }
    }
    if (!has_surrogate) {
        return s;
    }

    binary result = binary();
    int i = 0;
    while (i < len) {
        # Check for CESU-8 surrogate pair: high surrogate + low surrogate
        # High surrogate (U+D800..U+DBFF) in CESU-8: ed [a0-af] [80-bf]
        # Low surrogate (U+DC00..U+DFFF) in CESU-8: ed [b0-bf] [80-bf]
        if (i + 5 < len
                && b[i] == 0xed && b[i + 1] >= 0xa0 && b[i + 1] <= 0xaf
                && b[i + 3] == 0xed && b[i + 4] >= 0xb0 && b[i + 4] <= 0xbf) {
            # Decode the surrogate pair to a Unicode codepoint
            int hi = ((b[i + 1] & 0x0f) << 6) | (b[i + 2] & 0x3f);
            int lo = ((b[i + 4] & 0x0f) << 6) | (b[i + 5] & 0x3f);
            int cp = 0x10000 + (hi << 10) + lo;
            # Encode as proper UTF-8 (4 bytes for U+10000..U+10FFFF)
            binary encoded = binary();
            encoded += chr(0xf0 | (cp >> 18));
            encoded += chr(0x80 | ((cp >> 12) & 0x3f));
            encoded += chr(0x80 | ((cp >> 6) & 0x3f));
            encoded += chr(0x80 | (cp & 0x3f));
            result += encoded;
            i += 6;
        } else {
            binary byte_val = binary();
            byte_val += chr(b[i]);
            result += byte_val;
            ++i;
        }
    }
    return result.toString("utf-8");
}

#! Recursively fixes CESU-8 surrogate pairs in all strings within a data structure
/** Walks through hashes, lists, and strings fixing any CESU-8 surrogate pair
    sequences produced by Qore's JSON parser.

    @param val the value to fix (hash, list, string, or other)
    @return the value with all strings fixed

    @since json 1.12
*/
public auto sub fixSurrogatePairs(auto val) {
    switch (val.typeCode()) {
        case NT_STRING:
            return fixSurrogatePairsInString(val);
        case NT_HASH: {
            hash<auto> result = {};
            foreach string k in (keys val) {
                string fixed_key = fixSurrogatePairsInString(k);
                result{fixed_key} = fixSurrogatePairs(val{k});
            }
            return result;
        }
        case NT_LIST:
            return map fixSurrogatePairs($1), val;
        default:
            return val;
    }
}

} # namespace JsonLd
