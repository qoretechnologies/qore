# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aExtensions.qc @brief A2A protocol extension support

/*  A2aExtensions.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! The service parameter carrying the extension URIs a client wants to use for a request
/** Transmitted as an HTTP header in the JSON-RPC and HTTP+JSON/REST bindings, and as gRPC metadata in the gRPC
    binding.  Multiple values are comma-separated in a single header field.

    @since A2aClient 1.0
*/
public const A2aExtensionsHeader = "A2A-Extensions";

#! Describes an extension declared by an agent in its agent card
/** @since A2aClient 1.0
*/
public hashdecl A2aAgentExtension {
    #! The unique URI identifying the extension
    /** Extensions should carry version information in the URI; a breaking change requires a new URI.
    */
    string uri;

    #! A human-readable description of how the agent uses the extension
    *string description;

    #! If @ref True "True", the client must understand and comply with the extension's requirements
    bool required = False;

    #! Optional extension-specific configuration parameters
    *hash<auto> params;
}

#! Parses an \c A2A-Extensions service parameter value into a list of extension URIs
/** Empty entries are discarded and surrounding whitespace is removed, so a value with a trailing comma or
    spaces after the separators parses the same as a strictly formatted one.

    @param value the raw header value, or @ref nothing if the header was not sent

    @return the extension URIs requested, in the order given; an empty list if the header was absent or empty

    @since A2aClient 1.0
*/
public list<string> sub a2aParseExtensionsHeader(*string value) {
    if (!value.val()) {
        return ();
    }
    list<string> rv();
    foreach string uri in (value.split(",")) {
        string t = trim(uri);
        if (t.val()) {
            rv += t;
        }
    }
    return rv;
}

#! Serializes a list of extension URIs into an \c A2A-Extensions service parameter value
/** @param uris the extension URIs

    @return the header value, or @ref nothing if the list is empty, so that the caller can omit the header
    entirely rather than sending an empty one

    @since A2aClient 1.0
*/
public *string sub a2aMakeExtensionsHeader(*softlist<string> uris) {
    if (!uris) {
        return;
    }
    list<string> clean = (map trim($1), uris, trim($1).val()) ?? ();
    return clean ? clean.join(",") : NOTHING;
}

#! Extracts the extensions an agent declares from its agent card
/** Handles both the v1.0 card layout, where extensions live under \c capabilities.extensions, and the flat
    \c extensions key used by some v0.3 implementations.

    @param card the agent card

    @return the declared extensions keyed by URI; an empty hash if the card declares none

    @since A2aClient 1.0
*/
public hash<string, hash<A2aAgentExtension>> sub a2aGetCardExtensions(*hash<auto> card) {
    hash<string, hash<A2aAgentExtension>> rv();
    *softlist<auto> decls = card.capabilities.extensions ?? card.extensions;
    foreach auto decl in (decls) {
        # some cards list plain URI strings rather than extension objects
        if (decl.typeCode() == NT_STRING) {
            rv{decl} = <A2aAgentExtension>{"uri": decl};
            continue;
        }
        if (decl.typeCode() != NT_HASH || !decl.uri.val()) {
            continue;
        }
        rv{decl.uri} = <A2aAgentExtension>{
            "uri": decl.uri,
            "description": decl.description,
            "required": decl.required ?? False,
            "params": decl.params,
        };
    }
    return rv;
}

#! Returns the URIs of the extensions an agent card marks as required
/** @param card the agent card

    @return the required extension URIs

    @since A2aClient 1.0
*/
public list<string> sub a2aGetRequiredCardExtensions(*hash<auto> card) {
    hash<string, hash<A2aAgentExtension>> exts = a2aGetCardExtensions(card);
    return (map $1, keys exts, exts{$1}.required) ?? ();
}
}
