# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectError.qc Connect protocol error model

/*  ConnectError.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! the Connect namespace holds all public definitions in the Connect module
public namespace Connect {

#! Connect error info
public hashdecl ConnectErrorInfo {
    #! The error code (one of the \c CONNECT_ERROR_* constants)
    string code;
    #! Optional human-readable error message
    *string message;
}

#! Connect error code: the operation was canceled
public const CONNECT_ERROR_CANCELED = "canceled";

#! Connect error code: unknown error
public const CONNECT_ERROR_UNKNOWN = "unknown";

#! Connect error code: invalid argument
public const CONNECT_ERROR_INVALID_ARGUMENT = "invalid_argument";

#! Connect error code: deadline exceeded
public const CONNECT_ERROR_DEADLINE_EXCEEDED = "deadline_exceeded";

#! Connect error code: resource not found
public const CONNECT_ERROR_NOT_FOUND = "not_found";

#! Connect error code: resource already exists
public const CONNECT_ERROR_ALREADY_EXISTS = "already_exists";

#! Connect error code: permission denied
public const CONNECT_ERROR_PERMISSION_DENIED = "permission_denied";

#! Connect error code: resource exhausted
public const CONNECT_ERROR_RESOURCE_EXHAUSTED = "resource_exhausted";

#! Connect error code: failed precondition
public const CONNECT_ERROR_FAILED_PRECONDITION = "failed_precondition";

#! Connect error code: operation aborted
public const CONNECT_ERROR_ABORTED = "aborted";

#! Connect error code: out of range
public const CONNECT_ERROR_OUT_OF_RANGE = "out_of_range";

#! Connect error code: method not implemented
public const CONNECT_ERROR_UNIMPLEMENTED = "unimplemented";

#! Connect error code: internal error
public const CONNECT_ERROR_INTERNAL = "internal";

#! Connect error code: service unavailable
public const CONNECT_ERROR_UNAVAILABLE = "unavailable";

#! Connect error code: data loss
public const CONNECT_ERROR_DATA_LOSS = "data_loss";

#! Connect error code: unauthenticated
public const CONNECT_ERROR_UNAUTHENTICATED = "unauthenticated";

#! Maps Connect error codes to HTTP status codes (for unary error responses)
public const ConnectErrorToHttpStatus = {
    CONNECT_ERROR_CANCELED: 408,
    CONNECT_ERROR_UNKNOWN: 500,
    CONNECT_ERROR_INVALID_ARGUMENT: 400,
    CONNECT_ERROR_DEADLINE_EXCEEDED: 408,
    CONNECT_ERROR_NOT_FOUND: 404,
    CONNECT_ERROR_ALREADY_EXISTS: 409,
    CONNECT_ERROR_PERMISSION_DENIED: 403,
    CONNECT_ERROR_RESOURCE_EXHAUSTED: 429,
    CONNECT_ERROR_FAILED_PRECONDITION: 412,
    CONNECT_ERROR_ABORTED: 409,
    CONNECT_ERROR_OUT_OF_RANGE: 400,
    CONNECT_ERROR_UNIMPLEMENTED: 501,
    CONNECT_ERROR_INTERNAL: 500,
    CONNECT_ERROR_UNAVAILABLE: 503,
    CONNECT_ERROR_DATA_LOSS: 500,
    CONNECT_ERROR_UNAUTHENTICATED: 401,
};

#! Maps HTTP status codes to Connect error codes (for client-side inference)
public const HttpStatusToConnectError = {
    400: CONNECT_ERROR_INVALID_ARGUMENT,
    401: CONNECT_ERROR_UNAUTHENTICATED,
    403: CONNECT_ERROR_PERMISSION_DENIED,
    404: CONNECT_ERROR_NOT_FOUND,
    408: CONNECT_ERROR_DEADLINE_EXCEEDED,
    409: CONNECT_ERROR_ABORTED,
    412: CONNECT_ERROR_FAILED_PRECONDITION,
    429: CONNECT_ERROR_RESOURCE_EXHAUSTED,
    499: CONNECT_ERROR_CANCELED,
    500: CONNECT_ERROR_INTERNAL,
    501: CONNECT_ERROR_UNIMPLEMENTED,
    503: CONNECT_ERROR_UNAVAILABLE,
};

#! Connect error utility class
/** Provides static methods for error serialization and parsing.

    Connect protocol errors are always serialized as JSON, regardless of the
    codec used for messages.
*/
public class ConnectError {
    #! Serialize a Connect error to JSON
    /** @param err the error info
        @return the JSON string
    */
    static string serializeError(hash<ConnectErrorInfo> err) {
        hash<auto> obj = {"code": err.code};
        if (err.message.val()) {
            obj.message = err.message;
        }
        return make_json(obj);
    }

    #! Parse a Connect error from JSON
    /** @param json the JSON string
        @return the parsed error info

        @throw CONNECT-ERROR-PARSE-ERROR if the JSON cannot be parsed
    */
    static hash<ConnectErrorInfo> parseError(string json) {
        hash<auto> obj;
        obj = parse_json(json);
        if (!obj.code.val()) {
            throw "CONNECT-ERROR-PARSE-ERROR", sprintf("missing 'code' in error JSON: %y", json);
        }
        return <ConnectErrorInfo>{
            "code": obj.code,
            "message": obj.message,
        };
    }

    #! Get the HTTP status code for a Connect error code
    /** @param code the Connect error code
        @return the HTTP status code, or 500 if the code is unknown
    */
    static int httpStatusForError(string code) {
        return ConnectErrorToHttpStatus{code} ?? 500;
    }

    #! Get the Connect error code for an HTTP status code
    /** @param status the HTTP status code
        @return the Connect error code, or \c CONNECT_ERROR_UNKNOWN if the status is unmapped
    */
    static string errorForHttpStatus(int status) {
        return HttpStatusToConnectError{status} ?? CONNECT_ERROR_UNKNOWN;
    }
}

} # namespace Connect
