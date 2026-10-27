# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LspConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! A JSON-RPC connection with LSP base protocol framing (\c Content-Length headers)
/** By default the connection uses the process's standard input and output like
    @ref QLS::Messenger "Messenger"; it can also use any pair of streams, for example to run
    @ref QLS::QLSServer::main() "QLSServer::main()" in a test or in an embedding application.

    Messages can be sent from any thread; receiving is meant for a single reader thread.

    @par Example
    @code{.py}
# run a server on in-memory pipes and send it the initialize request
StreamPipe requests();
StreamPipe responses();
QLSServer server();
background server.main(new LspConnection(requests.getInputStream(), responses.getOutputStream()));
LspConnection client(responses.getInputStream(), requests.getOutputStream());
client.send(make_json({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"processId": getpid(),
    "capabilities": {}}}));
hash<auto> msg = client.receive();
    @endcode
*/
public class LspConnection {
    private {
        #! Reader for the input stream; standard input is used if not set
        *StreamReader reader;

        #! The output stream; standard output is used if not set
        *OutputStream out;

        #! Serializes writing messages
        Mutex sendLock();
    }

    #! Creates a connection on the process's standard input and output
    constructor() {
    }

    #! Creates a connection on the given streams
    /** @param input the stream that messages are read from
        @param output the stream that messages are written to
    */
    constructor(InputStream input, OutputStream output) {
        reader = new StreamReader(input, "UTF-8");
        out = output;
    }

    #! Receives one message
    /** @return a hash with a \c msg key with the message text, or an \c error key; the error is \c "EOF" when
        the input is closed
    */
    hash<auto> receive() {
        if (!reader) {
            return Messenger::receive();
        }
        int contentLength = -1;
        *string contentType;
        while (True) {
            # the separator is removed, so the line ending the headers is empty
            *string line = reader.readLine("\r\n");
            if (!exists line) {
                return {"error": "EOF"};
            }
            *list<auto> match;
            if (match = (line =~ x/^Content-Length:\s*(\d+)\s*$/i)) {
                contentLength = match[0].toInt();
            } else if (match = (line =~ x/^Content-Type:\s*(.+?)\s*$/i)) {
                contentType = match[0];
            } else if (line == "") {
                if (contentLength == -1) {
                    return {"error": "Content-Length header is missing"};
                }
                binary body = binary();
                while (body.size() < contentLength) {
                    *binary chunk = reader.readBinary(contentLength - body.size());
                    if (!chunk) {
                        return {"error": "EOF"};
                    }
                    body += chunk;
                }
                return {"Content-Type": contentType, "msg": body.toString("UTF-8")};
            }
        }
    }

    #! Sends one message
    /** @param msg the JSON-RPC message
    */
    send(string msg) {
        AutoLock al(sendLock);
        if (!out) {
            Messenger::send(msg);
            return;
        }
        binary body = binary(convert_encoding(msg, "UTF-8"));
        out.write(binary(sprintf("Content-Length: %d\r\n\r\n", body.size())) + body);
    }
}

} # namespace QLS
