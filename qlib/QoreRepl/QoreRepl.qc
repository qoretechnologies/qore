# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRepl class definition

/** QoreRepl.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args
%disable-warning deprecated

public namespace QoreRepl {

#! REPL result indicating the command requests exit
public const ReplExit = {"exit": True};

#! Core REPL class providing evaluation functionality
/** This class can be used standalone for programmatic REPL integration
    (e.g., WebSocket handlers) or with QoreReplClient for CLI usage.

    @par Example: Basic Usage
    @code{.py}
QoreRepl repl();
*hash<auto> result = repl.eval("1 + 1");
if (result.value) {
    printf("=> %s\n", QoreRepl::formatValue(result.value));
}
    @endcode

    @par Example: Multi-line Input
    @code{.py}
QoreRepl repl();
repl.eval("class Foo {");      # Returns NOTHING - incomplete
repl.eval("    int x = 1;");   # Still incomplete
*hash<auto> result = repl.eval("}");  # Now complete
    @endcode
*/
public class QoreRepl {
    public {
        #! Default prompt for new input
        const DefaultPrompt = "qore> ";

        #! Continuation prompt for multi-line input
        const ContinuationPrompt = "...> ";

        #! Version string
        const Version = "1.0";
    }

    private {
        #! The Program instance for evaluation
        Program pgm;

        #! Accumulated input for multi-line statements
        string pending_input = "";

        #! Counter for generating unique labels
        int eval_counter = 0;

        #! Options hash
        hash<auto> opts;
    }

    #! Creates a new REPL instance
    /** @param options Optional configuration:
        - \c program: Use an existing Program instead of creating a new one
        - \c parse_options: Parse options for the new Program (default: PO_NEW_STYLE)

        @note When creating a new Program, PO_ALLOW_REPARSE is enabled to allow multiple
        parse/commit cycles required for REPL functionality. The Program is created as
        independent (no parent) to avoid inheriting restrictive parse options.
    */
    constructor(*hash<auto> options) {
        opts = options ?? {};
        if (opts.program) {
            pgm = opts.program;
        } else {
            # Default parse options for REPL:
            # - PO_NEW_STYLE for modern Qore syntax
            # - PO_ALLOW_REPARSE to enable multiple parse/commit cycles
            # - PO_ALLOW_STATEMENT_NO_EFFECT to allow expression evaluation (e.g., "1 + 1")
            int parse_opts = (opts.parse_options ?? PO_NEW_STYLE) | PO_ALLOW_REPARSE | PO_ALLOW_STATEMENT_NO_EFFECT;
            # Create as independent Program (True = no parent) to avoid inheriting restrictions
            pgm = new Program(parse_opts, True);
        }
    }

    #! Evaluates input and returns the result
    /** @param input The code to evaluate

        @return A hash with the result:
        - \c value: The result value (if successful)
        - \c error: Exception info hash (if an error occurred)
        - \c exit: True if the user requested exit
        - Returns NOTHING if more input is needed (multi-line)

        @par Example
        @code{.py}
*hash<auto> result = repl.eval("1 + 1");
if (!exists result) {
    # Need more input
} else if (result.exit) {
    # User wants to exit
} else if (result.error) {
    # Handle error
} else {
    # Use result.value
}
        @endcode
    */
    *hash<auto> eval(string input) {
        # Check for special commands first
        if (input =~ /^\//) {
            return processCommand(input);
        }

        # Accumulate input
        if (pending_input.length()) {
            pending_input += "\n" + input;
        } else {
            pending_input = input;
        }

        # Check if input is complete
        if (!isComplete(pending_input)) {
            return NOTHING;
        }

        # Get the complete input and reset
        string code = pending_input;
        pending_input = "";

        # Empty input
        if (trim(code) == "") {
            return {"value": NOTHING};
        }

        # Ensure code ends with proper terminator to avoid parse errors
        # that would roll back program state. We add a semicolon proactively
        # for expressions that don't already have a terminator.
        code = ensureTerminated(code);

        # Generate unique label
        string label = sprintf("<repl:%d>", ++eval_counter);

        # Parse the code - use parse() which combines parsePending + parseCommit
        # Note: With PO_ALLOW_REPARSE, parse failures perform atomic rollback that
        # preserves previously committed program state (variables, functions, classes).
        # The ensureTerminated() call above helps avoid common parse errors like
        # "unexpected end of file" when users enter expressions without a trailing semicolon.
        try {
            pgm.parse(code, label);
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }

        # Execute
        try {
            auto result = pgm.run();
            return {"value": result};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Ensures code ends with a proper statement terminator
    /** Adds a semicolon to code that doesn't end with a terminator character.
        This prevents parse errors like "unexpected end of file" that would
        roll back program state.

        @param code The code to check
        @return The code with terminator added if needed
    */
    private string ensureTerminated(string code) {
        string trimmed = rtrim(code);
        if (trimmed.length() == 0) {
            return code;
        }

        string last = trimmed[trimmed.length() - 1];
        # These characters indicate the code is already properly terminated
        if (last == ";" || last == "}") {
            return code;
        }

        # Add semicolon for expressions and statements without terminator
        return trimmed + ";";
    }

    #! Returns True if the REPL is waiting for more input
    bool needsMoreInput() {
        return pending_input.length() > 0;
    }

    #! Returns the current prompt string
    /** @return DefaultPrompt for new input, ContinuationPrompt for multi-line
    */
    string getPrompt() {
        return needsMoreInput() ? ContinuationPrompt : DefaultPrompt;
    }

    #! Resets the pending input state
    reset() {
        pending_input = "";
    }

    #! Returns the underlying Program instance
    /** Useful for inspection or advanced manipulation
    */
    Program getProgram() {
        return pgm;
    }

    #! Formats a value for display
    /** @param val The value to format
        @return A string representation suitable for REPL output
    */
    static string formatValue(auto val) {
        switch (val.typeCode()) {
            case NT_NOTHING:
                return "";
            case NT_STRING:
                return sprintf("%y", val);
            case NT_HASH:
            case NT_LIST:
                return sprintf("%N", val);
            default:
                return sprintf("%y", val);
        }
    }

    #! Formats an exception for display
    /** @param ex The exception info hash
        @return A formatted error message
    */
    static string formatError(hash<ExceptionInfo> ex) {
        string msg = sprintf("%s: %s", ex.err, ex.desc);
        if (ex.arg) {
            msg += sprintf(" (%y)", ex.arg);
        }
        return msg;
    }

    #! Returns help text for special commands
    static string getHelpText() {
        return "Special commands:
  /help     - Show this help
  /quit     - Exit the REPL (also /exit, /q)
  /reset    - Reset the Program state
  /clear    - Clear pending multi-line input
  /vars     - List global variables
  /load <f> - Load and execute a file

Qore expressions and statements are evaluated directly.
Multi-line input is supported - the prompt changes to '...>' when more input is needed.
";
    }

    #! Processes special commands starting with /
    private *hash<auto> processCommand(string cmd) {
        # Parse command and arguments
        list<string> parts = cmd.split(" ");
        string command = parts[0].lwr();
        list<string> args = parts.size() > 1 ? parts[1..] : ();

        switch (command) {
            case "/help":
            case "/?":
                return {"value": getHelpText()};

            case "/quit":
            case "/exit":
            case "/q":
                return ReplExit;

            case "/reset":
                # Create a new Program with the same options
                int parse_opts = (opts.parse_options ?? PO_NEW_STYLE) | PO_ALLOW_REPARSE | PO_ALLOW_STATEMENT_NO_EFFECT;
                pgm = new Program(parse_opts, True);
                eval_counter = 0;
                return {"value": "Program state reset"};

            case "/clear":
                pending_input = "";
                return {"value": "Pending input cleared"};

            case "/vars":
                return {"value": formatGlobalVars()};

            case "/load":
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/load requires a filename"}};
                }
                return loadFile(args[0]);

            default:
                return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": sprintf("Unknown command: %s (try /help)", command)}};
        }
    }

    #! Formats global variables for display
    private string formatGlobalVars() {
        hash<auto> vars = pgm.getGlobalVars();
        if (!vars) {
            return "No global variables defined";
        }
        string result = "Global variables:\n";
        foreach string name in (keys vars) {
            result += sprintf("  %s = %s\n", name, formatValue(vars{name}));
        }
        return result;
    }

    #! Loads and executes a file
    /** @param filename The file path to load
        @return A result hash with either 'value' (success message) or 'error' (exception info)

        @note Parsing is only allowed when there are no active background threads
        in the Program. If threads are active, an error is returned.
    */
    private *hash<auto> loadFile(string filename) {
        try {
            # Check for active threads before parsing
            if (pgm.getThreadCount() > 0) {
                return {"error": <ExceptionInfo>{
                    "err": "REPL-ERROR",
                    "desc": "Cannot load file while background threads are active. Wait for threads to complete first.",
                }};
            }
            string code = ReadOnlyFile::readTextFile(filename);
            pgm.parse(code, filename);
            pgm.run();
            return {"value": sprintf("Loaded: %s", filename)};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Checks if the input is syntactically complete
    /** This is a heuristic check that tracks brace/bracket/paren depth
        and string state to determine if more input is needed.

        @param code The accumulated code to check
        @return True if the code appears complete, False if more input is needed
    */
    private bool isComplete(string code) {
        int braces = 0;     # { }
        int brackets = 0;   # [ ]
        int parens = 0;     # ( )
        bool in_string = False;
        bool in_regex = False;
        string string_char;
        bool escape_next = False;

        for (int i = 0; i < code.length(); i++) {
            string c = code[i];

            # Handle escape sequences
            if (escape_next) {
                escape_next = False;
                continue;
            }

            if (c == "\\") {
                escape_next = True;
                continue;
            }

            # Inside string
            if (in_string) {
                if (c == string_char) {
                    in_string = False;
                }
                continue;
            }

            # Inside regex
            if (in_regex) {
                if (c == "/") {
                    in_regex = False;
                }
                continue;
            }

            # Check for comments - skip to end of line
            if (c == "#") {
                # Find end of line
                int eol = code.find("\n", i);
                if (eol < 0) {
                    break;
                }
                i = eol;
                continue;
            }

            # Check for string start
            if (c == '"' || c == "'") {
                in_string = True;
                string_char = c;
                continue;
            }

            # Track depth
            switch (c) {
                case "{": braces++; break;
                case "}": braces--; break;
                case "[": brackets++; break;
                case "]": brackets--; break;
                case "(": parens++; break;
                case ")": parens--; break;
            }
        }

        # Incomplete if any depth is positive or still in string
        if (braces > 0 || brackets > 0 || parens > 0 || in_string) {
            return False;
        }

        # Check for trailing operators that suggest continuation
        # These operators at the end of a line typically indicate the expression continues
        # on the next line. This list covers:
        #   - comma (,): list/argument continuation
        #   - arithmetic (+, -, *, /): binary operators
        #   - member access (.): object member continuation
        #   - assignment (=): right-hand side expected
        #   - line continuation (\): explicit continuation
        # Note: This list should be maintained if Qore adds new binary operators that
        # commonly appear at line endings in multi-line expressions.
        string trimmed = rtrim(code);
        if (trimmed.length() > 0) {
            string last = trimmed[trimmed.length() - 1];
            if (last == "," || last == "+" || last == "-" || last == "*" ||
                last == "/" || last == "." || last == "=" || last == "\\") {
                return False;
            }
        }

        return True;
    }
}

} # namespace QoreRepl
