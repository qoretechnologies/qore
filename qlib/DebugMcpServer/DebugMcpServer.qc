# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DebugMcpServer.qc the MCP server of the DebugMcpServer module

/*  DebugMcpServer.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! An authenticator that requires a bearer token from the clients of a server
public class DebugMcpTokenAuthenticator inherits HttpServer::AbstractAuthenticator {
    private:internal {
        binary token;
    }

    #! Creates the authenticator
    /** @param token the token that the clients send as <tt>Authorization: Bearer token</tt>

        @throw DEBUG-MCP-ERROR the token is empty
    */
    constructor(string token) {
        if (!token.val()) {
            throw "DEBUG-MCP-ERROR", "the token of the server is empty";
        }
        self.token = binary(token);
    }

    #! Returns a 401 response unless the request has the token
    *hash<HttpResponseInfo> authenticateRequest(HttpServer::HttpListenerInterface listener, hash<auto> hdr,
            reference<hash<auto>> cx) {
        *string auth = hdr.authorization;
        *list<*string> m = auth ? (auth =~ x/^Bearer\s+(.+)$/i) : NOTHING;
        if (!m || !DebugMcpTokenAuthenticator::equalTokens(binary(trim(m[0])), token)) {
            return <HttpResponseInfo>{
                "code": 401,
                "body": "a valid token of the server is required",
                "hdr": {"WWW-Authenticate": "Bearer realm=\"qdbg-mcp\""},
            };
        }
    }

    #! Compares tokens in a time that does not depend on where they differ
    static bool equalTokens(binary a, binary b) {
        if (a.size() != b.size()) {
            return False;
        }
        int diff = 0;
        int len = a.size();
        for (int i = 0; i < len; ++i) {
            diff |= a[i] ^ b[i];
        }
        return !diff;
    }
}

#! The input of a tool of DebugMcpServer
public class DebugMcpToolInput inherits DataProvider::HashDataType {
    #! Creates the input type with the given fields
    constructor(hash<auto> fields) {
        addQoreFields(fields);
    }
}

#! The fields of the inputs of the tools of DebugMcpServer
public const DebugMcpFields = {
    "script": {
        "display_name": "Script",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The path of the Qore script to debug",
        "example_value": "/home/user/project/main.q",
    },
    "args": {
        "display_name": "Arguments",
        "type": AbstractDataProviderTypeMap."*softlist",
        "desc": "The arguments of the script, as strings",
    },
    "define": {
        "display_name": "Parse Defines",
        "type": AbstractDataProviderTypeMap."*softlist",
        "desc": "Parse defines of the script, each as `name` or `name=value`",
    },
    "time_zone": {
        "display_name": "Time Zone",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "The time zone of the script: a region name, such as `Europe/Prague`, or a UTC offset, such as "
            "`+01:00`",
    },
    "url": {
        "display_name": "URL",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The URL of the debug server, such as `ws://localhost:8001` or `wss://qorus:8011/debug`; `ws://` is "
            "added if it has no scheme",
        "example_value": "ws://localhost:8001",
    },
    "token": {
        "display_name": "Token",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "The token of the debug server, if it requires one; it is sent as a bearer token",
        "sensitive": True,
    },
    "location": {
        "display_name": "Location",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "Where the breakpoint pauses threads: `[file] line[:column]`, a statement of a line with the column "
            "counted in characters from 1, or `function[(parameter types)]`; without a file, the file of the current "
            "thread",
        "example_value": "main.q 12",
    },
    "condition": {
        "display_name": "Condition",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "An expression with the variables of the frame; the breakpoint pauses a thread only when it is true",
        "example_value": "count > 10",
    },
    "hit_condition": {
        "display_name": "Hit Condition",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "When the breakpoint pauses a thread by the number of times that it is reached: `n` or `>= n` (from "
            "the nth time), `== n` (only the nth time), `> n`, or `% n` (every nth time)",
        "example_value": ">= 3",
    },
    "log_message": {
        "display_name": "Log Message",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "A message that the breakpoint logs instead of pausing a thread (a logpoint), with expressions in "
            "braces, as `count={count}`; the messages are given in `logOutput` of the reports of the tools",
    },
    "id": {
        "display_name": "Breakpoint",
        "type": AbstractDataProviderTypeMap."int",
        "desc": "The number of the breakpoint",
    },
    "filters": {
        "display_name": "Filters",
        "type": AbstractDataProviderTypeMap."softlist",
        "desc": "The exceptions that pause threads: `user`, `system`, `uncaught` (exceptions that no `try` statement "
            "catches), exception codes, such as `FILE-OPEN-ERROR`, `all` or `none`",
        "example_value": ("uncaught",),
    },
    "mode": {
        "display_name": "Mode",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "How the thread is resumed: `run` until a breakpoint, an exception or the end (the default), `step` "
            "to the next statement, also into calls, `next` to the next statement of the function, or `finish` until "
            "the function returns: the thread pauses at the next statement of the caller, and the stop gives the "
            "function and the value that it returned in `returned`",
        "allowed_values": (
            <AllowedValueInfo>{
                "value": "run",
                "display_name": "Run",
                "short_desc": "Run until a breakpoint, an exception or the end",
                "desc": "Runs the thread until a thread pauses at a breakpoint or an exception, or the program ends",
            },
            <AllowedValueInfo>{
                "value": "step",
                "display_name": "Step Into",
                "short_desc": "Run to the next statement, into calls",
                "desc": "Runs the thread to the next statement, also into the functions that the statement calls",
            },
            <AllowedValueInfo>{
                "value": "next",
                "display_name": "Step Over",
                "short_desc": "Run to the next statement of the function",
                "desc": "Runs the thread to the next statement of its function",
            },
            <AllowedValueInfo>{
                "value": "finish",
                "display_name": "Step Out",
                "short_desc": "Run until the function returns",
                "desc": "Runs the thread until its function returns, and pauses it at the next statement of the "
                    "caller",
            },
        ),
    },
    "thread": {
        "display_name": "Thread",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "The thread (TID); the current thread if not given, which is the thread that paused last",
    },
    "frame": {
        "display_name": "Frame",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "The frame of the call stack, as given by `qore_debug_stack`, where 0 is the top frame; the current "
            "frame if not given",
    },
    "path": {
        "display_name": "Path",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "The path of a value whose elements, keys or members are returned, as `h`, `h.lines[1]` or "
            "`obj.member{key}`; the local variables of the frame if not given",
    },
    "start": {
        "display_name": "Start",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "The index of the first element, key or member that is returned",
    },
    "expression": {
        "display_name": "Expression",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "A Qore expression, evaluated with the variables of the frame and the code of the program",
        "example_value": "h.lines.size()",
    },
    "command": {
        "display_name": "Command",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "A command of the `qdbg` command-line debugger, such as `break list`, `thread list`, "
            "`thread var h.name = \"x\"` or `help`",
        "example_value": "thread list",
    },
    "timeout_ms": {
        "display_name": "Timeout",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "The time in milliseconds that the tool waits for a thread to pause; then the program is paused; the "
            "default is 30000",
    },
};

#! An MCP server that lets coding agents debug %Qore programs; see @ref debugmcpserverintro
/** One debug session is active at a time: a script launched in the process of the server, or a debug server that
    the server attached to; the tools are processed one at a time
*/
public class DebugMcpServer inherits McpServerHandler::McpServerHandler {
    public {
        #! the instructions of the server for the agents
        const Instructions = "Debugs Qore programs. Start with qore_debug_launch (a script, which pauses at its start) "
            "or qore_debug_attach (a debug server such as qdbg-server or Qorus). Set breakpoints with "
            "qore_debug_set_breakpoint, then resume with qore_debug_continue, which returns when a thread pauses, the "
            "program ends, or its timeout passes (then the program is paused). Read qore_debug_stack, "
            "qore_debug_variables and qore_debug_evaluate at a pause. Each tool reports the events (pauses with their "
            "location and exceptions, with hints for common Qore pitfalls), the output of the program and the state "
            "since the last tool. qore_debug_command runs any command of the qdbg debugger (\"help\" lists them). End "
            "with qore_debug_terminate.";
    }

    private {
        #! the debug session
        *DebugMcpSession session;
        #! serializes the tools
        Mutex toolLock();
        #! True if scripts can be launched in the process of the server
        bool allowLaunch;
        #! the logger of the debug sessions
        *Logger::LoggerInterface sessionLogger;
    }

    #! Creates the server
    /** @param logger the logger of the server
        @param auth the authenticator of the clients of the server
        @param opts options: \c allow_launch: @ref False if scripts cannot be launched in the process of the server,
        only debug servers attached to (default: @ref True); \c session_logger: the logger of the debug sessions
    */
    constructor(Logger::LoggerInterface logger, HttpServer::AbstractAuthenticator auth, *hash<auto> opts)
            : McpServerHandler(logger, auth) {
        allowLaunch = opts.allow_launch ?? True;
        sessionLogger = opts.session_logger;
        setServerInfo("qdbg-mcp", DebugMcpServerVersion, {"title": "Qore debugger"});
        setInstructions(Instructions);
        registerTools();
    }

    #! Ends the debug session
    destructor() {
        endSession();
    }

    #! Ends the debug session: a launched program ends, and the connection to a debug server is closed
    endSession() {
        AutoLock al(toolLock);
        endSessionIntern();
    }

    #! Returns the debug session, if any
    *DebugMcpSession getSession() {
        return session;
    }

    #! Registers the tools of the server
    private registerTools() {
        if (allowLaunch) {
            addTool("qore_debug_launch", "Launches a Qore script in the debugger; it pauses at its start, so "
                "that breakpoints can be set before it runs; an earlier session ends",
                ("script", "args", "define", "time_zone"), \launchTool());
        }
        addTool("qore_debug_attach", "Attaches to a debug server, such as qdbg-server or Qorus, whose programs "
            "are then debugged; an earlier session ends",
            ("url", "token"), \attachTool());
        addTool("qore_debug_set_breakpoint", "Sets a breakpoint on a line, a statement of a line or the start "
            "of a function of the current program, with an optional condition, hit condition or log message; returns "
            "its number",
            ("location", "condition", "hit_condition", "log_message"), \setBreakpointTool());
        addTool("qore_debug_delete_breakpoint", "Deletes a breakpoint", ("id",),
            \deleteBreakpointTool());
        addTool("qore_debug_exception_filter", "Sets the exceptions that pause threads",
            ("filters",), \exceptionFilterTool());
        addTool("qore_debug_continue", "Resumes a paused thread: runs it, steps into or over its statement, or "
            "runs it until its function returns; returns when a thread pauses, the program ends, or the timeout "
            "passes, and then the program is paused",
            ("mode", "thread", "timeout_ms"), \continueTool());
        addTool("qore_debug_pause", "Pauses the threads of the current program", ("timeout_ms",),
            \pauseTool());
        addTool("qore_debug_stack", "Returns the call stack of a paused thread", ("thread",),
            \stackTool());
        addTool("qore_debug_variables", "Returns the local variables of a frame of a paused thread with their "
            "types and values (a variable that was declared but not assigned is marked unassigned), or the elements, "
            "keys or members of a value",
            ("thread", "frame", "path", "start"), \variablesTool());
        addTool("qore_debug_evaluate", "Evaluates a Qore expression with the variables of a frame of a paused "
            "thread; it can call the functions and methods of the program",
            ("expression", "thread", "frame"), \evaluateTool());
        addTool("qore_debug_command", "Runs a command of the qdbg command-line debugger, such as \"break list\", "
            "\"thread list\", \"thread var h.name = 1\" or \"help\"; a command that resumes a thread returns when a "
            "thread pauses or the timeout passes",
            ("command", "timeout_ms"), \commandTool());
        addTool("qore_debug_status", "Returns the state of the debugger, and the events and the output of the "
            "program since the last tool",
            NOTHING, \statusTool());
        addTool("qore_debug_terminate", "Ends the debug session: a launched program ends, and the connection to "
            "a debug server is closed while its programs continue",
            NOTHING, \terminateTool());
    }

    #! Registers a tool whose arguments are the given fields of DebugMcpFields
    private addTool(string name, string desc, *list<string> fields, code<hash<auto>(*hash<auto>)> tool) {
        registerTool(name, desc, fields ? toolInput(fields) : NOTHING, hash<auto> sub (*hash<auto> args) {
            DebugMcpServer::checkArguments(name, fields, args);
            return tool(args);
        });
    }

    #! Checks that the arguments of a tool are its fields
    /** A misspelled argument would otherwise be ignored, so that the tool would do something else than was asked,
        as set a breakpoint that pauses threads instead of a logpoint

        @throw DEBUG-MCP-ERROR an argument is not a field of the tool
    */
    static checkArguments(string name, *list<string> fields, *hash<auto> args) {
        *list<string> unknown = keys (args - (fields ?? ()));
        if (unknown) {
            throw "DEBUG-MCP-ERROR", sprintf("%s: unknown argument%s %s; the arguments of the tool are: %s", name,
                unknown.size() == 1 ? "" : "s", (map sprintf("%y", $1), unknown).join(", "),
                fields ? fields.join(", ") : "none");
        }
    }

    #! Returns the input type of a tool with the given fields of DebugMcpFields
    private static DebugMcpToolInput toolInput(list<string> fields) {
        return new DebugMcpToolInput(DebugMcpFields{fields});
    }

    #! Returns the debug session
    /** @throw DEBUG-MCP-ERROR there is no debug session
    */
    private DebugMcpSession checkSession() {
        if (!session) {
            throw "DEBUG-MCP-ERROR", "there is no debug session; start one with qore_debug_launch or "
                "qore_debug_attach";
        }
        return session;
    }

    #! Ends the debug session; toolLock must be held
    private endSessionIntern() {
        if (session) {
            on_exit remove session;
            session.close();
        }
    }

    #! Launches a script
    private hash<auto> launchTool(*hash<auto> args) {
        AutoLock al(toolLock);
        endSessionIntern();
        DebugMcpLocalSession local(sessionLogger);
        session = local;
        on_error {
            local.close();
            remove session;
        }
        hash<auto!> opts = {};
        if (args.define) {
            opts.define = args.define;
        }
        if (args.time_zone) {
            opts.time_zone = args.time_zone;
        }
        hash<auto> rv = local.launch(args.script, args.args, opts);
        return rv + {"exceptionFilters": local.getExceptionFilters()};
    }

    #! Attaches to a debug server
    private hash<auto> attachTool(*hash<auto> args) {
        AutoLock al(toolLock);
        endSessionIntern();
        DebugMcpRemoteSession remote(args.url, args.token, sessionLogger);
        session = remote;
        on_error {
            remote.close();
            remove session;
        }
        hash<auto> rv;
        try {
            rv = remote.connect();
        } catch (hash<ExceptionInfo> ex) {
            # nothing listens at the address: the server is not running, or it listens on another port or host
            if (ex.err == "HTTPCLIENT-CONNECT-ERROR" && ex.desc =~ /Connection refused/) {
                throw ex.err, sprintf("%s; no debug server listens at %y: check that the debug server runs and the "
                    "port that it printed when it started (qdbg-server prints \"debug server listening on port "
                    "<port>\")", ex.desc, args.url), ex.arg;
            }
            rethrow;
        }
        return rv + {"exceptionFilters": remote.getExceptionFilters()};
    }

    #! Sets a breakpoint with its options
    private hash<auto> setBreakpointTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        hash<auto> rv = s.execute("break create " + args.location);
        if (!rv.ok) {
            return rv;
        }
        # the number of the breakpoint, from the event of its creation
        *int id = (map $1.breakpoint, rv.events ?? (), $1.change == "create")[0];
        if (!exists id) {
            return rv + {"ok": False, "error": "the breakpoint was not created"};
        }
        list<string> cmds = ();
        if (args.condition.val()) {
            push cmds, sprintf("break condition %d %s", id, args.condition);
        }
        if (args.hit_condition.val()) {
            push cmds, sprintf("break hitcondition %d %s", id, args.hit_condition);
        }
        if (args.log_message.val()) {
            push cmds, sprintf("break log %d %s", id, args.log_message);
        }
        if (!cmds) {
            return rv + {"breakpoint": id};
        }
        hash<auto> opt_rv = s.execute(cmds);
        # the breakpoint is not kept without its options
        if (!opt_rv.ok) {
            s.execute(sprintf("break delete %d", id));
            return opt_rv + {"events": rv.events};
        }
        # the event of the new breakpoint tells where it is; the events of setting its options say nothing more
        return opt_rv + {"breakpoint": id, "events": rv.events
            + (select opt_rv.events ?? (), $1.breakpoint != id)};
    }

    #! Deletes a breakpoint
    private hash<auto> deleteBreakpointTool(*hash<auto> args) {
        AutoLock al(toolLock);
        return checkSession().execute(sprintf("break delete %d", args.id));
    }

    #! Sets the exception filters
    private hash<auto> exceptionFilterTool(*hash<auto> args) {
        AutoLock al(toolLock);
        foreach auto f in (args.filters) {
            if (f.typeCode() != NT_STRING || f !~ /^[A-Za-z0-9_.:-]+$/) {
                throw "DEBUG-MCP-ERROR", sprintf("invalid exception filter %y", f);
            }
        }
        DebugMcpSession s = checkSession();
        hash<auto> rv = s.execute("exception filter " + (args.filters ?: ("none",)).join(" "));
        return rv + {"exceptionFilters": s.getExceptionFilters()};
    }

    #! Resumes a thread
    private hash<auto> continueTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        string cmd;
        switch (args.mode ?? "run") {
            case "run": cmd = "go run"; break;
            case "step": cmd = "go step"; break;
            case "next": cmd = "go stepover"; break;
            case "finish": cmd = "go untilreturn"; break;
            default:
                throw "DEBUG-MCP-ERROR", sprintf("unknown mode %y; expecting run, step, next or finish", args.mode);
        }
        if (exists args.thread) {
            s.selectThread(args.thread);
        }
        return s.execute(cmd, args.timeout_ms);
    }

    #! Pauses the program
    private hash<auto> pauseTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        int old_timeout = s.waitTimeout;
        if (exists args.timeout_ms) {
            s.waitTimeout = args.timeout_ms;
        }
        on_exit s.waitTimeout = old_timeout;
        return s.pause();
    }

    #! Returns the call stack of a thread
    private hash<auto> stackTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        list<hash<auto>> frames = s.getStack(args.thread);
        return s.makeReport({"frames": frames});
    }

    #! Returns the variables of a frame or the elements of a value
    private hash<auto> variablesTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        hash<auto> v = s.getVariables(args.thread, args.frame, args.path, args.start ?? 0);
        return s.makeReport(v);
    }

    #! Evaluates an expression
    private hash<auto> evaluateTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        hash<auto> v = s.evaluate(args.expression, args.thread, args.frame);
        return s.makeReport(v);
    }

    #! Runs a command of qdbg
    private hash<auto> commandTool(*hash<auto> args) {
        AutoLock al(toolLock);
        # the commands that end qdbg or run files are not commands of a session
        if (args.command =~ /^\s*(quit|script|session\s+(save|load)|history)\b/) {
            throw "DEBUG-MCP-ERROR", sprintf("the command %y is not available; qore_debug_terminate ends the "
                "session", args.command);
        }
        return checkSession().execute(args.command, args.timeout_ms);
    }

    #! Returns the state of the debugger
    private hash<auto> statusTool(*hash<auto> args) {
        AutoLock al(toolLock);
        if (!session) {
            return {"ok": True, "state": {"session": False}};
        }
        return session.makeReport({"session": session instanceof DebugMcpLocalSession ? "launch" : "attach"});
    }

    #! Ends the debug session
    private hash<auto> terminateTool(*hash<auto> args) {
        AutoLock al(toolLock);
        DebugMcpSession s = checkSession();
        hash<auto> rv;
        if (s instanceof DebugMcpLocalSession && !s.isProgramEnded()) {
            rv = cast<DebugMcpLocalSession>(s).terminate();
        } else {
            rv = s.makeReport();
        }
        bool remote = s instanceof DebugMcpRemoteSession;
        endSessionIntern();
        rv += {"sessionEnded": True, "state": {"session": False}};
        # the threads that the session paused continue when the last client of the debug server disconnects
        if (remote) {
            rv.text = (rv.text ?? "") + "Detached from the debug server; the paused threads of its programs "
                "continue unless another client of the server keeps them paused\n";
        }
        return rv;
    }
}

#! the version of DebugMcpServer, which the server reports
public const DebugMcpServerVersion = "1.0";
}
