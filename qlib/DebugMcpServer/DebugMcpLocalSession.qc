# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DebugMcpLocalSession.qc the session of the DebugMcpServer module that debugs a script in its process

/*  DebugMcpLocalSession.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The debugger of a DebugMcpLocalSession, which runs the debugged program in the process
public class DebugMcpProgramControl inherits DebugProgramControl::DebugProgramControl {
    public {
        #! the context of the connection of the session
        const CX = {"id": 1};
    }

    private:internal {
        DebugMcpLocalSession session;
    }

    #! Creates the debugger of a session
    constructor(DebugMcpLocalSession session, *Logger::LoggerInterface logger)
            : DebugProgramControl("qdbg-mcp", logger) {
        self.session = session;
        registerConnection(CX);
    }

    #! Unregisters the connection of the session
    destructor() {
        try {
            unregisterConnection(CX);
        } catch () {
            # a program can be deleted at the end of the process (PROGRAM-ERROR)
        }
    }

    #! Delivers the answer of a stopped thread to the command that waits for it, and events to the session
    public sendDataImpl(hash<auto> cx, auto data) {
        if (!session.sync.deliver(data)) {
            session.handleEvent(data);
        }
    }

    #! Delivers events to the session
    public broadcastDataImpl(auto data) {
        session.handleEvent(data);
    }
}

#! A debug session of a DebugMcpServer that launches a script in the process of the server
/** One program is debugged at a time; its output is given in the reports of the session instead of being written to
    stdout and stderr, which are captured while the session exists (see DebugProgramControl::captureOutput())
*/
public class DebugMcpLocalSession inherits DebugMcpSession {
    public {
        #! the debugger
        DebugMcpProgramControl dpc;
        #! delivers the answers of stopped threads to the commands that wait for them
        ResponseSync sync();
    }

    private {
        #! gives the commands sent to stopped threads their unique IDs
        Sequence uidSeq();
        #! serializes the commands sent to stopped threads, as their answers are delivered one at a time
        Mutex cmdLock();
        #! counts the thread that runs the program
        Counter running();
        #! True if the output of the process is captured by the session
        bool capturing;
        #! the thread that runs the program
        int programTid;
        #! True when the thread that runs the program left it, so that the program ends
        bool programLeft;
    }

    #! Creates the session
    constructor(*Logger::LoggerInterface logger) : DebugMcpSession(logger) {
        dpc = new DebugMcpProgramControl(self, logger);
    }

    #! Sends a command to the debugger; a command that resumes a thread returns when a thread pauses again
    public *hash<auto> doCommandImpl(hash<auto> data) {
        if (!exists data.tid) {
            return dpc.processCommand(DebugMcpProgramControl::CX, data);
        }
        # the wait starts before the command is sent, as the thread can stop again before the answer is received
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
            # the stopped thread answers every command, with the unique ID of the command
            data.uid = string(uidSeq.next());
            sync.prepare(data.uid);
            on_error sync.cancel();
            ret = dpc.processCommand(DebugMcpProgramControl::CX, data);
            if (ret.type == DebugResponseType::Thread) {
                ret = sync.waitForResponse();
            } else {
                # answered without the thread, such as a thread that is not stopped
                sync.cancel();
            }
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

    #! Notes that the thread that runs the program leaves it, before the event is processed
    public handleEvent(auto data) {
        if (data.typeCode() == NT_HASH && data.type == DebugResponseType::Event && data.tid == programTid
            && ((data.cmd == DebugEventCmd::Interrupt && data.func == "detach")
                || (data.cmd == DebugEventCmd::Run && data.runstate == DebugRunStateName::Detach))) {
            programLeft = True;
        }
        DebugMcpSession::handleEvent(data);
    }

    #! Waits for the end of the program after its thread left it, so that the report gives the end
    private commandsDone() {
        if (programLeft) {
            # the thread returns from Program::run() and reports the end
            running.waitForZero();
        }
    }

    #! Waits for the end of the program after the \c kill command, as long as the wait timeout at most
    public bool waitForProgramEnd() {
        int timeout_ms = waitTimeout ?: DefaultWaitTimeout;
        if (running.waitForZero(timeout_ms)) {
            printfImpl("the program did not end within %d ms\n", timeout_ms);
            return False;
        }
        return True;
    }

    #! Returns @ref True if no program runs
    public bool isProgramEnded() {
        return !running.getCount();
    }

    #! Launches a script in the debugger; it pauses at its start
    /** @param script the file of the script
        @param args the arguments of the script
        @param opts the options of the script: \c define, a list of parse defines as \c "name=value",
        \c parse_option, a list of parse options, and \c time_zone, as with \c qdbg

        @return the report; see DebugMcpSession::makeReport()

        @throw DEBUG-MCP-ERROR a program is being debugged
    */
    public hash<auto> launch(string script, *softlist<string> args, *hash<auto> opts) {
        if (running.getCount()) {
            throw "DEBUG-MCP-ERROR", "a program is being debugged; end it first (qore_debug_terminate)";
        }
        # the output of the program is given in the reports
        if (!capturing) {
            capturing = dpc.captureOutput(False);
        }
        Program pgm = dpc.createProgram(script, opts, args ?? ());
        # the program and thread of the new program are the current ones when it pauses at its start
        setContextValue('programId', NOTHING);
        setContextValue('threadId', NOTHING);
        expectStop();
        programLeft = False;
        programTid = 0;
        running.inc();
        try {
            background runProgram(pgm);
        } catch (hash<ExceptionInfo> ex) {
            running.dec();
            cancelStop();
            rethrow;
        }
        waitForStop();
        return makeReport();
    }

    #! Ends the debugged program, and waits for its end as long as the wait timeout at most
    /** @return the report; see DebugMcpSession::makeReport()

        @throw DEBUG-MCP-ERROR no program is being debugged
    */
    public hash<auto> terminate() {
        if (!running.getCount()) {
            throw "DEBUG-MCP-ERROR", "no program is being debugged";
        }
        # the kill command waits for the end of the program; see waitForProgramEnd()
        processCmdLine("kill");
        return makeReport();
    }

    #! Ends the session: a debugged program ends, and the output of the process is no longer captured
    public close() {
        if (running.getCount()) {
            try {
                processCmdLine("kill");
            } catch (hash<ExceptionInfo> ex) {
                # the program can end before it is ended
            }
            running.waitForZero();
        }
        map dpc.removeProgram($1), dpc.getAllPrograms();
        if (capturing) {
            dpc.stopOutputCapture();
            capturing = False;
        }
    }

    #! Runs the program, and reports its end
    private runProgram(Program pgm) {
        programTid = gettid();
        on_exit running.dec();
        # a command that waits for a thread to pause ends with the program
        on_exit cancelStop();
        try {
            pgm.run();
            writeJson({"type": "end"});
        } catch (hash<ExceptionInfo> ex) {
            # a program that the debugger ended; the thread is no longer cancelled
            if (DebugProgramControl::isTerminateException(ex)) {
                clear_thread_cancel();
                writeJson({"type": "end", "terminated": True});
                return;
            }
            hash<auto> e = map {$1.key: $1.value}, ex.pairIterator(), $1.key != "callstack" && $1.key != "next";
            e.text = get_exception_string(ex);
            writeJson({"type": "end", "exception": e});
        }
    }
}
}
