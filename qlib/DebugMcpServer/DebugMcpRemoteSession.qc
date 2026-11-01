# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DebugMcpRemoteSession.qc the session of the DebugMcpServer module that attaches to a debug server

/*  DebugMcpRemoteSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! the DebugMcpServer namespace contains all the definitions in the DebugMcpServer module
public namespace DebugMcpServer {
#! A debug session of a DebugMcpServer that attaches to a debug server, such as \c qdbg-server or Qorus
public class DebugMcpRemoteSession inherits DebugMcpSession {
    public {
        #! the default time that the session waits for an answer of the debug server
        const DefaultResponseTimeout = 15s;
    }

    private {
        #! delivers the answers of the debug server to the commands that wait for them
        ResponseSync sync();
        #! the ID of the command that waits for its answer
        int pendingUid;
        #! the connection to the debug server
        WebSocketClient wsc;
        #! the time that the session waits for an answer of the debug server
        timeout responseTimeout;
        #! True while the session connects
        bool connecting;
        #! True when the debug server closed the connection
        bool closed;
        #! the URL of the debug server
        string url;
        #! the name of the debug server, from its welcome
        string serverName = "";
        #! the headers of the connection, such as the token of the debug server
        *hash<auto> headers;
        #! serializes the commands, as one answer is waited for at a time
        Mutex cmdLock();
    }

    #! Creates the session
    /** @param url the URL of the debug server, such as \c ws://localhost:8001; \c ws:// is added if it has no scheme
        @param token the token of the debug server, sent as a bearer token, if any
        @param logger the logger of the session
        @param response_timeout the time that the session waits for an answer of the debug server
    */
    constructor(string url, *string token, *Logger::LoggerInterface logger, *timeout response_timeout)
            : DebugMcpSession(logger) {
        if (url !~ /^[a-z][a-z0-9+.-]*:\/\//i) {
            url = "ws://" + url;
        }
        if (url !~ /^wss?:\/\//i) {
            throw "DEBUG-MCP-ERROR", sprintf("the URL %y of the debug server must start with ws:// or wss://", url);
        }
        self.url = url;
        responseTimeout = response_timeout ?? DefaultResponseTimeout;
        if (token.val()) {
            headers.Authorization = "Bearer " + token;
        }
        wsc = new WebSocketClient(\wscEvent(), {"url": url});
    }

    #! Connects to the debug server, and makes its first program, and its stopped thread, the current ones
    /** @return the report; see DebugMcpSession::makeReport()

        @throw QORE-DEBUG the server is not a %Qore debug server, or did not answer
    */
    public hash<auto> connect() {
        connecting = True;
        on_exit connecting = False;
        sync.prepare();
        hash<auto> hh;
        {
            on_error sync.cancel();
            hh = wsc.connect({"hdr": headers});
        }
        *string prot_ver = hh{QoreDebugWsProtocolHeader.lwr()};
        if (!prot_ver.val()) {
            sync.cancel();
            throw "QORE-DEBUG", sprintf("connected to %y, but it sent no %y header; check the path of the URL", url,
                QoreDebugWsProtocolHeader);
        }
        if (prot_ver != QoreDebugProtocolVersion) {
            sync.cancel();
            throw "QORE-DEBUG", sprintf("the debug server at %y uses the protocol version %y; expecting %y", url,
                prot_ver, QoreDebugProtocolVersion);
        }
        *hash<auto> resp = sync.waitForResponse(responseTimeout);
        if (!exists resp) {
            throw "QORE-DEBUG", sprintf("the debug server at %y did not answer", url);
        }
        serverName = resp.result ?? "";
        connecting = False;
        # the debugged programs, and the threads that are stopped, such as the thread of a program that the server
        # holds at its start
        *hash<auto> sess = doCommand({"cmd": "session/get"}, False).result;
        if (sess) {
            string pgm_id = sess.firstKey();
            setContextValue('programId', pgm_id);
            if (sess{pgm_id}.interrupted) {
                setContextValue('threadId', sess{pgm_id}.interrupted[0]);
            }
            # the threads that are stopped are paused
            foreach string pid in (keys sess) {
                notePausedThreads(pid, sess{pid}.interrupted ?? ());
            }
        }
        return makeReport({"server": serverName, "programs": sess ? keys sess : ()});
    }

    #! Sends a command to the debug server; a command that resumes a thread returns when a thread pauses again
    public *hash<auto> doCommandImpl(hash<auto> data) {
        if (closed) {
            throw "DEBUG-MCP-ERROR", sprintf("the debug server at %y closed the connection", url);
        }
        bool resumes = resumesWithWait(data);
        if (resumes) {
            expectStop(data.tid);
        }
        on_error if (resumes) {
            cancelStop();
        }
        *hash<auto> ret;
        {
            AutoLock al(cmdLock);
            pendingUid = clock_getmicros();
            # an untyped hash, as a hash of strings, such as {"cmd": "session/get"}, also gets the integer ID
            hash<auto!> cmd = data;
            cmd.uid = pendingUid;
            sync.prepare();
            on_error sync.cancel();
            wsc.send(make_yaml(cmd));
            ret = sync.waitForResponse(responseTimeout);
        }
        if (!exists ret) {
            throw "DEBUG-MCP-ERROR", sprintf("the debug server at %y did not answer the command %y", url, data.cmd);
        }
        if (resumes) {
            if (ret.type == DebugResponseType::Error) {
                cancelStop();
            } else {
                waitForStop();
            }
        }
        return ret;
    }

    #! Returns @ref False, as a program of a debug server is not known to end
    public bool isProgramEnded() {
        return False;
    }

    #! Closes the connection to the debug server; its programs continue
    public close() {
        if (!closed) {
            closed = True;
            wsc.disconnect();
        }
    }

    #! Processes a message of the debug server
    private wscEvent(*data msg) {
        if (!exists msg) {
            # the debug server closed the connection; a command that waits for a thread to pause ends
            if (!connecting) {
                closed = True;
                writeJson({"type": "message", "text": sprintf("the debug server at %s closed the connection\n",
                    url)});
                cancelStop();
            }
            return;
        }
        try {
            auto d = parse_yaml(msg);
            if (d.typeCode() != NT_HASH) {
                return;
            }
            if (sync.isPending()) {
                if (((d.type == DebugResponseType::Response || d.type == DebugResponseType::Error)
                        && pendingUid == d.uid && !connecting)
                    || (d.type == DebugResponseType::Event && d.cmd == DebugEventCmd::Welcome && connecting)) {
                    sync.deliver(d);
                    return;
                }
            }
            handleEvent(d);
        } catch (hash<ExceptionInfo> ex) {
            writeJson({"type": "message", "text": sprintf("cannot process a message of the debug server: %s\n",
                DebugCommandLine::getErrorText(ex))});
        }
    }
}
}
