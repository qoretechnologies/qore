# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore REPL action session class definition

/** QoreReplActionSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

%requires Util

public namespace QoreRepl {

#! Action-based REPL session for Creator WebSocket integration
/** This class provides a transport-agnostic action API for REPL use and emits
    a list of events for each action.
*/
public class QoreReplActionSession {
    public {
        #! Protocol version
        const ProtocolVersion = "1.0";
    }

    private {
        #! REPL instance
        QoreRepl repl;

        #! Session ID
        string session_id;
    }

    #! Creates a new action session
    constructor(*hash<auto> options) {
        repl = new QoreRepl(options);
        session_id = get_random_string(16);
    }

    #! Returns session info
    hash<auto> getSessionInfo() {
        return {
            "sessionId": session_id,
            "version": ProtocolVersion,
            "qoreVersion": Qore::VersionString,
        };
    }

    #! Returns the REPL instance
    QoreRepl getRepl() {
        return repl;
    }

    #! Returns the session ID
    string getSessionId() {
        return session_id;
    }

    #! Returns prompt events without evaluation
    list<hash<auto>> getPromptEvents(*string request_id) {
        return (
            makePromptEvent(request_id),
        );
    }

    #! Handles a REPL action
    list<hash<auto>> handleAction(string action, *hash<auto> args) {
        switch (action) {
            case "repl-eval":
                return handleEval(args);

            case "repl-command":
                return handleCommand(args);

            case "repl-interrupt":
                return handleInterrupt(args);

            case "repl-close":
                return handleClose(args);
        }

        throw "INVALID-ACTION", sprintf("Unsupported action %y", action);
    }

    private list<hash<auto>> handleEval(*hash<auto> args) {
        string code = getRequiredStringArg(args, "code");
        *string request_id = args.request_id ?? args.id;
        return evalWithCode(code, request_id);
    }

    private list<hash<auto>> handleCommand(*hash<auto> args) {
        string command = getRequiredStringArg(args, "command");
        *string request_id = args.request_id ?? args.id;
        if (command !~ /^\//) {
            command = "/" + command;
        }
        return evalWithCode(command, request_id);
    }

    private list<hash<auto>> handleInterrupt(*hash<auto> args) {
        *string request_id = args.request_id ?? args.id;
        return (
            makeErrorEvent(request_id, "NOT-IMPLEMENTED", "Interrupt is not currently supported"),
        );
    }

    private list<hash<auto>> handleClose(*hash<auto> args) {
        *string request_id = args.request_id ?? args.id;
        return (
            makeExitEvent("user_request", request_id),
        );
    }

    private list<hash<auto>> evalWithCode(string code, *string request_id) {
        date start = now_us();
        *hash<auto> result = repl.eval(code);
        int duration_ms = (now_us() - start).durationMicroseconds() / 1000;

        if (!exists result) {
            return (
                makePromptEvent(request_id),
            );
        }

        if (result.exit) {
            return (
                makeExitEvent("user_request", request_id),
            );
        }

        list<hash<auto>> events = ();
        if (result.error) {
            events += makeErrorEvent(request_id, result.error.err, result.error.desc, result.error);
        } else {
            events += makeResultEvent(request_id, result.value, duration_ms);
        }
        events += makePromptEvent(request_id);
        return events;
    }

    private hash<auto> makePromptEvent(*string request_id) {
        return {
            "event": "repl-prompt",
            "data": {
                "request_id": request_id,
                "value": repl.getPrompt(),
                "needs_more_input": repl.needsMoreInput(),
            },
        };
    }

    private hash<auto> makeResultEvent(*string request_id, auto value, int duration_ms) {
        hash<auto> data = {
            "request_id": request_id,
            "value": QoreRepl::formatValue(value),
            "duration_ms": duration_ms,
        };
        if (exists value) {
            data.value_type = value.fullType();
        }
        return {
            "event": "repl-result",
            "data": data,
        };
    }

    private hash<auto> makeExitEvent(string reason, *string request_id) {
        return {
            "event": "repl-exit",
            "data": {
                "request_id": request_id,
                "reason": reason,
            },
        };
    }

    private hash<auto> makeErrorEvent(*string request_id, string error, string description,
            *hash<ExceptionInfo> ex) {
        hash<auto> data = {
            "request_id": request_id,
            "code": error,
            "message": description,
        };
        if (ex && ex.err) {
            data.details = ex;
        }
        return {
            "event": "repl-error",
            "data": data,
        };
    }

    private string getRequiredStringArg(*hash<auto> args, string name) {
        if (!args || !exists args{name}) {
            throw "MISSING-PARAMETER", sprintf("Missing '%s' parameter", name);
        }
        if (args{name}.typeCode() != NT_STRING) {
            throw "INVALID-PARAMETER", sprintf("Parameter '%s' must be a string", name);
        }
        return args{name};
    }
}

} # namespace QoreRepl
