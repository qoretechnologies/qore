# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DebugMcpSession.qc the debug session of the DebugMcpServer module

/*  DebugMcpSession.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The debug session of a DebugMcpServer: a debugger whose output is collected as JSON objects
/** The session processes the commands of the \c qdbg command-line debugger with JSON output (see
    @ref DebugCmdLine::DebugCommandLine::jsonOutput "jsonOutput"); a command that resumes a thread returns when a thread
    pauses, when the program ends, or when the wait timeout passes, and then the program is paused (see
    @ref DebugCmdLine::DebugCommandLine::waitTimeout "waitTimeout").  The events and the output of the program are
    collected until the next report; see makeReport()

    A subclass connects the session to a debugger: see DebugMcpLocalSession and DebugMcpRemoteSession
*/
public class DebugMcpSession inherits DebugCmdLine::DebugCommandLine {
    public {
        #! the default time that a command that resumes a thread waits for a thread to pause, in milliseconds
        const DefaultWaitTimeout = 30000;

        #! the most elements, keys or members of a value that are returned at once
        const MaxChildren = 100;
    }

    private {
        #! the objects of the JSON output that are not reported yet
        list<hash<auto>> objects = ();
        #! serializes the access to objects
        Mutex objectLock();
    }

    #! Creates the session
    /** @param logger the logger of the session
    */
    constructor(*Logger::LoggerInterface logger) : DebugCommandLine(logger, False) {
        jsonOutput = True;
        waitsForStops = True;
        waitTimeout = DefaultWaitTimeout;
    }

    #! Passes text that a command prints to the output of the command; see DebugCmdLine::DebugCommandLine::printJsonText()
    public printfImpl(string fmt, ...) {
        printJsonText(vsprintf(fmt, argv));
    }

    #! Keeps an object of the JSON output for the next report
    public writeJson(hash<auto> h) {
        AutoLock al(objectLock);
        push objects, h;
    }

    #! Processes data that the debugger sends: its events are shown and update the state of the threads
    public handleEvent(auto data) {
        printData(data);
        if (data.typeCode() == NT_HASH) {
            # the state of the threads is updated before a command that waits for the event continues
            processEvent(data);
            eventReceived(data);
        }
    }

    #! Returns @ref True if the debugged program ended
    abstract public bool isProgramEnded();

    #! Ends the session: a launched program ends, and the connection to a debug server is closed
    abstract public close();

    #! Runs commands of the \c qdbg command-line debugger, until one fails, and returns the report
    /** @param cmds the commands
        @param wait_timeout the time that a command that resumes a thread waits for a thread to pause, in
        milliseconds, if not the default

        @return the report; see makeReport()
    */
    public hash<auto> execute(softlist<string> cmds, *int wait_timeout) {
        int old_timeout = waitTimeout;
        if (exists wait_timeout) {
            if (wait_timeout < 0) {
                throw "DEBUG-MCP-ERROR", sprintf("the timeout cannot be negative: %d", wait_timeout);
            }
            waitTimeout = wait_timeout;
        }
        on_exit waitTimeout = old_timeout;
        foreach string cmd in (cmds) {
            processCmdLine(cmd);
            if (exists commandError) {
                break;
            }
        }
        commandsDone();
        return makeReport();
    }

    #! Called after the commands of execute() before the report is made; does nothing by default
    /** A subclass waits for the end of a program whose thread left it, so that the report gives the end
    */
    private commandsDone() {
    }

    #! Returns the report of what happened since the last report, and the state of the debugger
    /** @param extra keys that are added to the report, such as the result of a tool

        @return a hash with the keys:
        - \c ok: @ref False if a command failed
        - \c error: the error of the command that failed
        - \c text: the text that the commands and the debugger printed
        - \c timedOut: @ref True if no thread paused before the time of a command passed, so that the program was
          paused
        - \c events: the events of the debugger, each with its \c text, such as
          <tt>Paused at breakpoint 1 at main.q:3 (thread 2)</tt>, and its \c tid, \c file, \c line, the \c reason of a
          stop, the \c exception of a stop at an exception with the \c hint of a common pitfall, and the
          \c breakpoint of an event of a breakpoint
        - \c programOutput: the output of the program
        - \c programEnd: the end of the program, with the \c exception that ended it, or \c terminated if the debugger
          ended it
        - \c state: the state of the debugger; see getState()

        The text that the debugger printed outside of a command, as for a pause, is a part of \c text
    */
    public hash<auto> makeReport(*hash<auto> extra) {
        list<hash<auto>> objs;
        {
            AutoLock al(objectLock);
            objs = objects;
            objects = ();
        }
        hash<auto!> rv = {"ok": True};
        list<hash<auto>> events = ();
        string text = "";
        string output = "";
        foreach hash<auto> o in (objs) {
            switch (o.type) {
                case "result":
                    text += o.output;
                    if (!o.ok && rv.ok) {
                        rv.ok = False;
                        rv.error = o.error;
                    }
                    if (o.timedOut) {
                        rv.timedOut = True;
                    }
                    break;
                case "event":
                    push events, DebugMcpSession::summarizeEvent(o);
                    break;
                case "output":
                    output += o.text;
                    break;
                case "end":
                    rv.programEnd = o - "type";
                    break;
                case "message":
                    text += o.text;
                    break;
                case "error":
                    text += sprintf("error of the debug server: %s: %s\n", o.command, o.error);
                    break;
            }
        }
        if (text.val()) {
            rv.text = text;
        }
        if (events) {
            rv.events = events;
        }
        if (output.val()) {
            rv.programOutput = output;
        }
        rv += extra;
        rv.state = getState();
        return rv;
    }

    #! Returns the state of the debugger
    /** @return a hash with the keys \c programId and \c threadId, the current program and thread, if any;
        \c paused, @ref True if the current thread is paused; and \c programEnded, @ref True if the program ended
    */
    public hash<auto> getState() {
        *softint pgm_id = getContextValue('programId');
        *softint tid = getContextValue('threadId');
        hash<auto!> rv = {
            "programEnded": isProgramEnded(),
        };
        if (exists pgm_id) {
            rv.programId = pgm_id;
            if (exists tid) {
                rv.threadId = tid;
                rv.paused = isThreadPaused(pgm_id, tid);
            }
        }
        return rv;
    }

    #! Makes a thread the current thread, if given, and returns the current thread
    /** @throw DEBUG-COMMAND there is no current thread
    */
    public int selectThread(*int tid) {
        if (exists tid) {
            setContextValue('threadId', tid);
        }
        return checkThreadId();
    }

    #! Returns the call stack of a thread, without the frames of the debugger
    /** @param tid the thread; the current thread if not given

        @return the frames from the top, each with its \c frame number, the \c function, the \c file, \c line and
        \c column (counted in characters from 1) of its statement, and \c current for the frame whose variables the
        other tools use by default
    */
    public list<hash<auto>> getStack(*int tid) {
        int t = selectThread(tid);
        list<auto> stack = doCommand({"cmd": "thread/stack", "tid": t}, False).result ?? ();
        list<hash<auto>> rv = ();
        foreach hash<auto> frame in (DebugCommandLine::getShownFrames(stack)) {
            hash<auto!> f = {
                "frame": $#,
                "function": frame.name,
            };
            if (frame.source) {
                f.file = frame.source;
                f.line = frame.line + frame.offset;
            } else if (frame.file) {
                f.file = frame.file;
                if (frame.line > 0) {
                    f.line = frame.line;
                    if (exists frame.column && frame.column >= 0) {
                        f.column = DebugCommandLine::getCharColumn(getSourceLine(frame.file, frame.line),
                            frame.column);
                    }
                }
            }
            if (frame.current) {
                f.current = True;
            }
            push rv, f;
        }
        return rv;
    }

    #! Returns the local variables of a frame, or the elements, keys or members of a value
    /** @param tid the thread; the current thread if not given
        @param frame the frame, as given by getStack(); the current frame of the thread if not given
        @param path the path of a value, as \c "h", \c "h.lines[1]" or \c "obj.member{key}", whose elements, keys or
        members are returned; the local variables of the frame if not given
        @param start the index of the first element, key or member that is returned

        @return the description of the value of the path, with its \c type and \c value, and its elements, keys or
        members in \c children (at most @ref MaxChildren), or the local variables of the frame in \c variables; each
        child or variable has its \c name, \c type and \c value, \c size if it has elements, keys or members, and a
        local variable has its \c declaredType, and \c unassigned if it was declared but not assigned

        @throw DEBUG-ERROR the path does not exist
    */
    public hash<auto> getVariables(*int tid, *int frame, *string path, int start = 0) {
        int t = selectThread(tid);
        hash<auto!> cmd = {"tid": t};
        if (exists frame) {
            cmd.frameid = frame;
        }
        *list<auto> locals = doCommand(cmd + {"cmd": "thread/local//list"}, False).result;
        if (!path.val()) {
            list<hash<auto>> vars = ();
            foreach string name in (sort(locals ?? ())) {
                hash<auto> d = doCommand(cmd + {"cmd": "thread/variable/get", "root": {"local": name}}, False).result;
                push vars, {"name": name} + DebugMcpSession::describeValue(d);
            }
            return {"variables": vars};
        }
        hash<auto> pv = DebugCommandLine::parseVariablePath(path);
        cmd += {
            "cmd": "thread/variable/get",
            "root": inlist(pv.name, locals ?? ()) ? {"local": pv.name} : {"global": pv.name},
            "path": pv.path,
        };
        hash<auto> d = doCommand(cmd, False).result;
        hash<auto!> rv = {"path": path} + DebugMcpSession::describeValue(d);
        if (d.indexed || d.named) {
            d = doCommand(cmd + {"children": True, "start": start, "count": MaxChildren}, False).result;
            rv.children = map {"name": $1.name} + DebugMcpSession::describeValue($1), d.children;
            int total = d.indexed ?? d.named;
            if (start + rv.children.size() < total) {
                rv.more = total - start - rv.children.size();
            }
        }
        return rv;
    }

    #! Evaluates an expression with the variables of a frame
    /** @param expr the expression
        @param tid the thread; the current thread if not given
        @param frame the frame, as given by getStack(); the current frame of the thread if not given

        @return the description of the value with its \c type and \c value, and \c size if it has elements, keys or
        members; see getVariables() for them
    */
    public hash<auto> evaluate(string expr, *int tid, *int frame) {
        int t = selectThread(tid);
        hash<auto!> cmd = {"cmd": "thread/evaluate", "tid": t, "expression": expr};
        if (exists frame) {
            cmd.frameid = frame;
        }
        return {"expression": expr} + DebugMcpSession::describeValue(doCommand(cmd, False).result);
    }

    #! Pauses the current program, and waits for a thread to pause as long as the wait timeout
    /** @return the report; see makeReport()
    */
    public hash<auto> pause() {
        int pgm_id = checkProgramId();
        # a paused thread is not paused again
        *softint tid = getContextValue('threadId');
        if (exists tid && isThreadPaused(pgm_id, tid)) {
            printfImpl("thread %d is paused\n", tid);
            return makeReport();
        }
        expectStop();
        {
            on_error cancelStop();
            doCommand({"cmd": "program/" + pgm_id + "/stop"}, False);
        }
        waitForStop();
        return makeReport();
    }

    #! Returns the description of a value of the debugger for a tool
    static hash<auto> describeValue(hash<auto> d) {
        hash<auto!> rv = {"type": d.type, "value": d.value};
        if (d.indexed || d.named) {
            rv.size = d.indexed ?? d.named;
        }
        if (exists d.declaredType) {
            rv.declaredType = d.declaredType;
        }
        if (d.unassigned) {
            rv.unassigned = True;
        }
        return rv;
    }

    #! Returns the summary of an event object of the JSON output for a report
    static hash<auto> summarizeEvent(hash<auto> o) {
        hash<auto!> rv = {"text": o.text};
        hash<auto> d = o.data ?? {};
        switch (o.event) {
            case DebugEventCmd::Interrupt:
                rv.reason = d.func;
                if (exists d.tid) {
                    rv.tid = d.tid;
                }
                if (exists d.file) {
                    rv.file = d.file;
                }
                if (d.line > 0) {
                    rv.line = d.line;
                }
                if (d.bkptids) {
                    rv.breakpoints = d.bkptids;
                } else if (d.bkptid) {
                    rv.breakpoints = (d.bkptid,);
                }
                if (d.func == "exception") {
                    rv.exception = d.exception{"err", "desc"};
                    if (d.uncaught) {
                        rv.exception.uncaught = True;
                    }
                    if (o.hint) {
                        rv.hint = o.hint;
                    }
                }
                break;
            case DebugEventCmd::Breakpoint:
                rv.breakpoint = d.info.id;
                rv.change = d.event;
                if (*hash<auto> st = d.info.statements[0]) {
                    rv.file = st.file;
                    rv.line = st.start_line + st.offset;
                }
                break;
            case DebugEventCmd::Run:
                rv.tid = d.tid;
                break;
        }
        return rv;
    }
}
}
