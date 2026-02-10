# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ParamValidator.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

public namespace QLS {

public class ParamValidator {
    #! Check that required key is present in a hash and that its value is of correct type.
    /**
        @param request original request
        @param h hash to check
        @param key key name that's required
        @param types possible type-codes of the key's value
        @param err reference to a string where an eventual error will be stored

        @return True if ok, False if key is missing or value is of incorrect type
    */
    public static bool required(hash<auto> request, hash<auto> h, string key, softlist<auto> types,
            reference<string> err) {
        if (!h.hasKey(key)) {
            err = ErrorResponse::invalidRequest(request, sprintf("required attribute '%s' is missing", key));
            return False;
        }
        if (types && !types.contains(h{key}.typeCode())) {
            err = ErrorResponse::invalidRequest(
                request,
                sprintf(
                    "required attribute '%s' has invalid type: %s (%d - %y required)",
                    key, h{key}.type(), h{key}.typeCode(), types
                )
            );
            return False;
        }

        return True;
    }

    #! Validates InitializeParams.
    public static *string initializeParams(hash<auto> request, hash<auto> params) {
        string err;
        if (!ParamValidator::required(request, params, "processId", (NT_INT, NT_NOTHING), \err)) {
            return err;
        }
        if (params.hasKey("rootPath") && !ParamValidator::required(request, params, "rootPath", (NT_STRING, NT_NOTHING), \err)) {
            return err;
        }
        # should be required but VSCode sometimes doesn't send it
        if (params.hasKey("rootUri") && !ParamValidator::required(request, params, "rootUri", (NT_STRING, NT_NOTHING), \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params, "capabilities", NT_HASH, \err)) {
            return err;
        }

        # ignore 'initializationOptions'
        # ignore 'trace'
    }

    #! Validates TextDocumentIdentifier.
    public static *string textDocumentIdentifier(hash<auto> request, hash<auto> params) {
        string err;
        if (!ParamValidator::required(request, params, "textDocument", NT_HASH, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.textDocument, "uri", NT_STRING, \err)) {
            return err;
        }

        return NOTHING;
    }

    #! Validates TextDocumentItem.
    public static *string textDocumentItem(hash<auto> request, hash<auto> params) {
        *string err = ParamValidator::textDocumentIdentifier(request, params);
        if (err) {
            return err;
        }

        if (!ParamValidator::required(request, params.textDocument, "languageId", NT_STRING, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.textDocument, "version", NT_INT, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.textDocument, "text", NT_STRING, \err)) {
            return err;
        }

        return NOTHING;
    }

    #! Validates VersionedTextDocumentIdentifier.
    public static *string versionedTextDocumentIdentifier(hash<auto> request, hash<auto> params) {
        *string err = ParamValidator::textDocumentIdentifier(request, params);
        if (err) {
            return err;
        }
        if (!ParamValidator::required(request, params.textDocument, "version", NT_INT, \err)) {
            return err;
        }

        return NOTHING;
    }

    #! Validates TextDocumentPositionParams.
    public static *string textDocumentPositionParams(hash<auto> request, hash<auto> params) {
        *string err = ParamValidator::textDocumentIdentifier(request, params);
        if (err) {
            return err;
        }

        if (!ParamValidator::required(request, params, "position", NT_HASH, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.position, "line", NT_INT, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.position, "character", NT_INT, \err)) {
            return err;
        }

        return NOTHING;
    }

    #! Validate DidChangeConfigurationParams.
    public static *string didChangeConfigParams(hash<auto> request, hash<auto> params) {
        string err;
        if (!ParamValidator::required(request, params, "settings", NT_HASH, \err)) {
            return err;
        }
        return NOTHING;
    }

    #! Validate DidChangeWatchedFilesParams.
    public static *string didChangeWatchedFilesParams(hash<auto> request, hash<auto> params) {
        string err;
        if (!ParamValidator::required(request, params, "changes", NT_LIST, \err)) {
            return err;
        }
        foreach auto evt in (params.changes) {
            if (evt.typeCode() != NT_HASH) {
                return ErrorResponse::invalidRequest(
                    request,
                    sprintf(
                        "FileEvent has invalid type: %s (hash required)",
                        evt.type()
                    )
                );
            }
            if (!ParamValidator::required(request, evt, "uri", NT_STRING, \err)) {
                return err;
            }
            if (!ParamValidator::required(request, evt, "type", NT_INT, \err)) {
                return err;
            }
        }

        return NOTHING;
    }

    #! Validates ReferenceParams.
    public static *string referenceParams(hash<auto> request, hash<auto> params) {
        *string err = ParamValidator::textDocumentPositionParams(request, params);
        if (err) {
            return err;
        }

        if (!ParamValidator::required(request, params, "context", NT_HASH, \err)) {
            return err;
        }
        if (!ParamValidator::required(request, params.context, "includeDeclaration", NT_BOOLEAN, \err)) {
            return err;
        }

        return NOTHING;
    }

    #! Validate WorkspaceSymbolParams.
    public static *string workspaceSymbolParams(hash<auto> request, hash<auto> params) {
        string err;
        if (!ParamValidator::required(request, params, "query", NT_STRING, \err)) {
            return err;
        }
        return NOTHING;
    }
}

} # namespace QLS
