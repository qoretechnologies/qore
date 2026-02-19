# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Messenger.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

#! Messenger.
public class Messenger {
    private {
        const LSP_PART_DELIMITER = "\r\n";
    }

    #! Receive JSON-RPC request from stdin.
    public static hash<auto> receive() {
        int contentLength = -1;
        string contentType;
        *string txt = "";

        while (True) {
            txt = stdin.readLine(False, LSP_PART_DELIMITER);
            if (!exists txt) {
                return {"error": "EOF"};
            }

            # read headers
            *list<auto> match;
            if (match = (txt =~ x/^Content-Length:\s*(\d+)\s*$/i)) {
                contentLength = match[0].toInt();
            } else if (match = (txt =~ x/^Content-Type:\s*(.+?)\s*$/i)) {
                contentType = match[0];
            } else if (txt == "") {
                # read JSON-RPC message
                if (contentLength == -1) {
                    return {"error": "Content-Length header is missing"};
                }
                binary b = stdin.readBinary(contentLength);
                if (!exists b) {
                    return {"error": "EOF"};
                }
                if (b.size() != contentLength) {
                    return {"error": sprintf("short read: expected %d bytes, got %d",
                        contentLength, b.size())};
                }
                txt = b.toString("UTF-8");
                return {"Content-Type": contentType, "msg": txt};
            }
        }
    }

    #! Send JSON-RPC response to stdout.
    public static send(string response) {
        stdout.printf("Content-Length: %d%s", response.size(), LSP_PART_DELIMITER);
        stdout.write(LSP_PART_DELIMITER + response);
    }
}

} # namespace QLS
