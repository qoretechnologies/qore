# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ErrorResponse.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

#! LSP ErrorCodes definition
public const ErrorCodes = {
    "ParseError": -32700,
    "InvalidRequest": -32600,
    "MethodNotFound": -32601,
    "InvalidParams": -32602,
    "InternalError": -32603,
    "serverErrorStart": -32099,
    "serverErrorEnd": -32000,
    "ServerNotInitialized": -32002,
    "UnknownErrorCode": -32001,
};

#! Error response factory.
public class ErrorResponse {
    public static string notInitialized(hash<auto> request) {
        hash<auto> err = {
            "code": ErrorCodes.ServerNotInitialized,
            "message": "server has not been initialized yet",
        };
        return make_jsonrpc_error(request.jsonrpc, request.id, err);
    }

    public static string invalidRequest(hash<auto> request, *string message) {
        hash<auto> err = {
            "code": ErrorCodes.InvalidRequest,
            "message": message ? message : "invalid request",
        };
        return make_jsonrpc_error(request.jsonrpc, request.id, err);
    }

    public static string methodNotFound(hash<auto> request) {
        hash<auto> err = {
            "code": ErrorCodes.MethodNotFound,
            "message": sprintf("method '%s' is not implemented", request.method),
        };
        return make_jsonrpc_error(request.jsonrpc, request.id, err);
    }

    public static string internalError(string jsonrpc, *string message) {
        hash<auto> err = {
            "code": ErrorCodes.InternalError,
            "message": message ?? "internal error",
        };
        return make_jsonrpc_error(jsonrpc, NOTHING, err);
    }

    public static string invalidParams(hash<auto> request, *string message) {
        hash<auto> err = {
            "code": ErrorCodes.InvalidParams,
            "message": message ?? "invalid params",
        };
        return make_jsonrpc_error(request.jsonrpc, request.id, err);
    }

    public static string nonExistentDocument(hash<auto> request) {
        string msg = sprintf("document with URI '%s' is not present in parsed documents",
            request.params.textDocument.uri);
        return ErrorResponse::invalidParams(request, msg);
    }
}

} # namespace QLS
