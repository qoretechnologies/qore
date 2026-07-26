# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Notification.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

#! LSP MessageType definition
public const MessageType = {
    "Error": 1,
    "Warning": 2,
    "Info": 3,
    "Log": 4,
};

#! Notification factory.
public class Notification {
    #! Create a diagnostics notification.
    public static string diagnostics(string jsonRpcVer, string uri, *list<auto> diagnostics) {
        hash<auto> notification = {
            "uri": uri,
            "diagnostics": diagnostics ?? (),
        };
        return make_jsonrpc_request("textDocument/publishDiagnostics", jsonRpcVer, NOTHING, notification);
    }

    #! Create a "showMessage" notification.
    public static string showMessage(string jsonRpcVer, int messageType, string message) {
        hash<auto> notification = {
            "type": messageType,
            "message": message,
        };
        return make_jsonrpc_request("window/showMessage", jsonRpcVer, NOTHING, notification);
    }
}

} # namespace QLS
