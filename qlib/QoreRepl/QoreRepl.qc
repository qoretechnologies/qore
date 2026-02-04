# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRepl class definition

/** QoreRepl.qc Copyright 2026 Qore Technologies, s.r.o.

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
%disable-warning deprecated

public namespace QoreRepl {

#! QoreRepl module version
public const Version = "1.1";

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
        const Version = "1.1";
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

        #! Command history
        list<string> history = ();

        #! Whether to show execution timing
        bool show_timing = False;

        #! Path to history file
        *string history_file;

        #! Default history file name
        const HistoryFile = ".qore-repl-history";

        #! Maximum history length
        const MaxHistoryLen = 500;

        #! Color output helper
        Util::TerminalColor color();

        #! List of all REPL commands for completion
        const ReplCommands = (
            "/help", "/?",
            "/quit", "/exit", "/q",
            "/version",
            "/reset", "/clear",
            "/vars",
            "/functions", "/funcs",
            "/classes",
            "/constants", "/consts",
            "/namespaces", "/ns",
            "/modules",
            "/type",
            "/describe",
            "/history",
            "/timing",
            "/load",
            "/save",
            "/pwd",
            "/cd",
        );

        #! Canonical commands for abbreviation resolution (excludes short aliases like /q, /?, /ns)
        const CanonicalCommands = (
            "/help",
            "/quit",
            "/exit",
            "/version",
            "/reset",
            "/clear",
            "/vars",
            "/functions",
            "/classes",
            "/constants",
            "/namespaces",
            "/modules",
            "/type",
            "/describe",
            "/history",
            "/timing",
            "/load",
            "/save",
            "/pwd",
            "/cd",
        );
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

        # Initialize history file path (only if filesystem access is allowed)
        if (!isFilesystemRestricted()) {
            if (opts.history_file) {
                history_file = opts.history_file;
            } else {
                # Default to $HOME/.qore-repl-history (or $USERPROFILE on Windows)
                *string home = ENV.HOME ?? ENV.USERPROFILE;
                if (home) {
                    history_file = home + DirSep + HistoryFile;
                }
            }
        }
    }

    #! Returns True if filesystem access is restricted by parse options
    private bool isFilesystemRestricted() {
        return (pgm.getParseOptions() & PO_NO_FILESYSTEM) != 0;
    }

    #! Keywords that indicate the code is a statement, not a pure expression
    const StatementKeywords = {
        # Declarations
        "our": True,
        "my": True,
        "const": True,
        "class": True,
        "sub": True,
        "namespace": True,
        "hashdecl": True,
        "public": True,
        "private": True,
        "static": True,
        "abstract": True,
        "final": True,
        "synchronized": True,
        "deprecated": True,
        "module": True,
        # Type declarations (used in typed function/variable declarations)
        "int": True,
        "float": True,
        "number": True,
        "string": True,
        "bool": True,
        "date": True,
        "binary": True,
        "hash": True,
        "list": True,
        "object": True,
        "auto": True,
        "any": True,
        "nothing": True,
        "softint": True,
        "softfloat": True,
        "softnumber": True,
        "softstring": True,
        "softbool": True,
        "softdate": True,
        "softlist": True,
        "timeout": True,
        "code": True,
        "reference": True,
        "data": True,
        # Control flow
        "if": True,
        "while": True,
        "do": True,
        "for": True,
        "foreach": True,
        "switch": True,
        "try": True,
        "throw": True,
        "rethrow": True,
        "return": True,
        "break": True,
        "continue": True,
        "thread_exit": True,
        # Context statements
        "context": True,
        "summarize": True,
        "subcontext": True,
        # On-block statements
        "on_exit": True,
        "on_error": True,
        "on_success": True,
        # Other
        "delete": True,
        "background": True,
    };

    #! Wraps pure expressions in return() so their value is displayed
    /** This method detects if the code is a pure expression (vs a statement)
        and wraps it in "return (...);" so the REPL can display the result.

        @param code The code to potentially wrap (already terminated with ;)
        @return The wrapped code if it's a pure expression, otherwise unchanged

        @par Examples
        - "1 + 1;" -> "return (1 + 1);"
        - "myFunc();" -> "return (myFunc());"
        - "our int x = 5;" -> "our int x = 5;" (unchanged - declaration)
        - "if (True) { }" -> "if (True) { }" (unchanged - control flow)
    */
    private string wrapExpressionIfNeeded(string code) {
        string trimmed = trim(code);

        # Empty or just semicolon
        if (trimmed == "" || trimmed == ";") {
            return code;
        }

        # Multi-statement code (contains ; before the end) - don't wrap
        # This is a simple heuristic; we look for ; not at the end
        string without_trailing = trimmed;
        if (without_trailing.endsWith(";")) {
            without_trailing = without_trailing.substr(0, without_trailing.size() - 1);
        }
        if (without_trailing =~ /;/) {
            return code;
        }

        # Check if it starts with a statement keyword
        # Extract the first word (identifier) from the code
        *list<*string> match = (trimmed =~ x/^([a-zA-Z_][a-zA-Z0-9_]*)/);
        if (match && StatementKeywords{match[0]}) {
            return code;
        }

        # Check for block statements (starts with {)
        if (trimmed =~ /^\{/) {
            return code;
        }

        # It looks like a pure expression - mark it for special handling
        # We return a special marker that eval() will detect
        string expr = without_trailing;
        return sprintf("__REPL_EXPR__:%s", expr);
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
            # Add command to history
            history += input;
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

        # Add to history
        history += code;

        # Empty input
        if (trim(code) == "") {
            return {"value": NOTHING};
        }

        # Ensure code ends with proper terminator to avoid parse errors
        # that would roll back program state. We add a semicolon proactively
        # for expressions that don't already have a terminator.
        code = ensureTerminated(code);

        # Try to detect pure expressions so their value is displayed
        # This makes the REPL more intuitive - "1 + 1" will show "=> 2"
        string wrapped_code = wrapExpressionIfNeeded(code);

        # Generate unique label
        string label = sprintf("<repl:%d>", ++eval_counter);

        # Check if this is a pure expression (marked by wrapExpressionIfNeeded)
        bool is_expression = False;
        string expr_func_name = "";
        if (wrapped_code =~ /^__REPL_EXPR__:/) {
            is_expression = True;
            # Extract the expression (everything after the marker)
            string expr = wrapped_code.substr(14);  # len("__REPL_EXPR__:") = 14
            expr_func_name = sprintf("__repl_expr_%d__", eval_counter);
            # Create a temporary function that returns the expression value
            wrapped_code = sprintf("auto sub %s() { return (%s); }", expr_func_name, expr);
        }

        # Parse the code - use parse() which combines parsePending + parseCommit
        # Note: With PO_ALLOW_REPARSE, parse failures perform atomic rollback that
        # preserves previously committed program state (variables, functions, classes).
        # The ensureTerminated() call above helps avoid common parse errors like
        # "unexpected end of file" when users enter expressions without a trailing semicolon.
        #
        # For expression evaluation: if parsing as a function fails, fall back to
        # treating it as a regular statement
        try {
            pgm.parse(wrapped_code, label);
        } catch (hash<ExceptionInfo> ex) {
            # If expression wrapping failed, try original code
            if (is_expression) {
                is_expression = False;
                try {
                    pgm.parse(code, label);
                } catch (hash<ExceptionInfo> ex2) {
                    return {"error": ex2};
                }
            } else {
                return {"error": ex};
            }
        }

        # Execute with optional timing
        try {
            date start;
            if (show_timing) {
                start = now_us();
            }

            # Run the program to register any new definitions
            pgm.run();

            # If this was an expression, call the temporary function to get the result
            auto result;
            if (is_expression) {
                result = pgm.callFunction(expr_func_name);
            }

            if (show_timing) {
                date elapsed = now_us() - start;
                return {"value": result, "timing": elapsed};
            }
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
                return val;
            case NT_HASH:
            case NT_LIST:
                return sprintf("%Y", val);
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

    #! Pads a colored string to a visible width
    private string padColored(string text, string visible_text, int width) {
        int padding = width - visible_text.length();
        if (padding > 0) {
            return text + strmul(" ", padding);
        }
        return text;
    }

    #! Formats a help line with proper alignment
    private string helpLine(string cmd, string desc, int width = 18) {
        return sprintf("  %s %s\n", padColored(color.value(cmd), cmd, width), desc);
    }

    #! Returns help text for special commands
    string getHelpText() {
        string help = sprintf("%s\n", color.key("Commands:"));
        help += helpLine("/help", "Show this help");
        help += helpLine("/quit", "Exit the REPL (also /exit, /q)");
        help += helpLine("/version", "Show QoreRepl and Qore version");
        help += sprintf("\n%s\n", color.key("Session:"));
        help += helpLine("/reset", "Reset the Program state");
        help += helpLine("/clear", "Clear pending multi-line input");
        help += helpLine("/history", "Show command history");
        help += helpLine("/timing", "Toggle execution timing display");
        help += sprintf("\n%s\n", color.key("Introspection:"));
        help += helpLine("/vars", "List global variables");
        help += helpLine("/functions", "List user-defined functions (also /funcs)");
        help += helpLine("/classes", "List user-defined classes");
        help += helpLine("/constants", "List user-defined constants (also /consts)");
        help += helpLine("/namespaces", "List namespaces (also /ns)");
        help += helpLine("/modules", "List loaded modules");
        help += helpLine("/type <expr>", "Show the type of an expression");
        help += helpLine("/describe <name>", "Describe a class, function, or namespace");
        help += sprintf("\n%s\n", color.key("Files:"));
        help += helpLine("/load <file>", "Load and execute a file");
        help += helpLine("/save <file>", "Save session history to a file");
        help += helpLine("/pwd", "Show current working directory");
        help += helpLine("/cd <dir>", "Change working directory");
        help += "\nQore expressions and statements are evaluated directly.\n";
        help += sprintf("Multi-line input is supported - the prompt changes to '%s' when more input is needed.\n",
            color.value("...>"));
%ifndef NoLinenoise
        help += sprintf("\n%s\n", color.key("Keyboard shortcuts (linenoise):"));
        help += helpLine("Up/Down", "Navigate history");
        help += helpLine("Ctrl+R", "Reverse search history");
        help += helpLine("Tab", "Complete REPL commands");
        help += helpLine("Ctrl+C", "Cancel current input / exit");
%endif
        return help;
    }

    #! Resolves abbreviated commands to their full form
    /** @param command The command to resolve (e.g., "/desc", "/cl")
        @return A hash with either:
            - "command": the resolved full command if unambiguous
            - "error": an error hash if ambiguous or nothing if no resolution needed
    */
    private *hash<auto> resolveCommandAbbreviation(string command) {
        # Check if it's already a known command (exact match in ReplCommands)
        if (inlist(command, ReplCommands)) {
            return NOTHING;
        }

        # Must start with / to be a command abbreviation
        if (!command.startsWith("/")) {
            return NOTHING;
        }

        # Find all canonical commands that start with this prefix
        list<string> matches = ();
        foreach string canonical in (CanonicalCommands) {
            if (canonical.equalPartial(command)) {
                matches += canonical;
            }
        }

        if (matches.size() == 0) {
            # No matches - let the switch handle the "unknown command" error
            return NOTHING;
        }

        if (matches.size() == 1) {
            # Unambiguous match
            return {"command": matches[0]};
        }

        # Ambiguous - multiple matches
        string msg = sprintf("Ambiguous command '%s'. Did you mean: %s?", command, matches.join(", "));
        return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": msg}};
    }

    #! Processes special commands starting with /
    private *hash<auto> processCommand(string cmd) {
        # Parse command and arguments
        list<string> parts = cmd.split(" ");
        string command = parts[0].lwr();
        list<string> args = parts.size() > 1 ? parts[1..] : ();

        # Try to resolve abbreviated commands
        *hash<auto> resolved = resolveCommandAbbreviation(command);
        if (resolved.error) {
            return resolved;
        }
        if (resolved.command) {
            command = resolved.command;
        }

        switch (command) {
            case "/help":
            case "/?":
                return {"value": getHelpText()};

            case "/quit":
            case "/exit":
            case "/q":
                return ReplExit;

            case "/version":
                return {"value": sprintf("QoreRepl %s (Qore %s)", QoreRepl::Version, Qore::VersionString)};

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

            case "/functions":
            case "/funcs":
                return {"value": formatFunctions()};

            case "/classes":
                return {"value": formatClasses()};

            case "/constants":
            case "/consts":
                return {"value": formatConstants()};

            case "/namespaces":
            case "/ns":
                return {"value": formatNamespaces()};

            case "/modules":
                return {"value": formatModules()};

            case "/type":
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/type requires an expression"}};
                }
                return getExpressionType(args.join(" "));

            case "/describe":
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/describe requires a name"}};
                }
                # Join args to support namespace-qualified names like "Qore::File" typed with spaces
                string name = args.size() == 1 ? args[0] : args.join("");
                return describeSymbol(name);

            case "/history":
                return {"value": formatHistory()};

            case "/timing":
                show_timing = !show_timing;
                return {"value": sprintf("Timing display %s", show_timing ? "enabled" : "disabled")};

            case "/load":
                if (isFilesystemRestricted()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/load is disabled: filesystem access is restricted (PO_NO_FILESYSTEM)"}};
                }
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/load requires a filename"}};
                }
                # Join args to support paths with spaces
                return loadFile(args.join(" "));

            case "/save":
                if (isFilesystemRestricted()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/save is disabled: filesystem access is restricted (PO_NO_FILESYSTEM)"}};
                }
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/save requires a filename"}};
                }
                # Join args to support paths with spaces
                return saveHistory(args.join(" "));

            case "/pwd":
                if (isFilesystemRestricted()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/pwd is disabled: filesystem access is restricted (PO_NO_FILESYSTEM)"}};
                }
                return {"value": getcwd()};

            case "/cd":
                if (isFilesystemRestricted()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/cd is disabled: filesystem access is restricted (PO_NO_FILESYSTEM)"}};
                }
                if (!args.size()) {
                    return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": "/cd requires a directory"}};
                }
                # Join args to support paths with spaces
                return changeDirectory(args.join(" "));

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
        # Filter out system variables (Qore::*)
        hash<auto> user_vars = {};
        foreach string name in (keys vars) {
            if (name !~ /^Qore::/) {
                user_vars{name} = vars{name};
            }
        }
        if (!user_vars) {
            return "No user-defined global variables";
        }
        string result = sprintf("%s\n", color.key("Global variables:"));
        foreach string name in (keys user_vars) {
            result += sprintf("  %s = %s\n", color.value(name), formatValue(user_vars{name}));
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
        #   - bitwise operators (&, |, ^): often split across lines in complex expressions
        #   - ternary operator (?): right-hand side expected
        # Operators NOT included (these typically end expressions):
        #   - semicolon (;): statement terminator
        #   - closing brackets/parens (], )): end of expression
        #   - increment/decrement (++, --): unary postfix, usually end expressions
        # Note: Two-character operators like && and || are not detected by single-char check,
        # but the component chars (& and |) will trigger continuation when they appear alone.
        string trimmed = rtrim(code);
        if (trimmed.length() > 0) {
            string last = trimmed[trimmed.length() - 1];
            if (last == "," || last == "+" || last == "-" || last == "*" ||
                last == "/" || last == "." || last == "=" || last == "\\" ||
                last == "&" || last == "|" || last == "^" || last == "?") {
                return False;
            }
        }

        return True;
    }

    #! Formats user-defined functions for display
    private string formatFunctions() {
        *list<string> funcs = pgm.getUserFunctionList();
        if (!funcs) {
            return "No user-defined functions";
        }
        string result = sprintf("%s\n", color.key("User-defined functions:"));
        foreach string name in (funcs) {
            result += sprintf("  %s()\n", color.value(name));
        }
        return result;
    }

    #! Formats user-defined classes for display
    private string formatClasses() {
        Reflection::Namespace root = Reflection::Namespace::forName(pgm, "::");
        list<Reflection::Class> classes = root.getClasses();
        # Filter to user-defined classes (no module name)
        list<string> user_classes = ();
        foreach Reflection::Class cls in (classes) {
            if (!cls.getModuleName()) {
                user_classes += cls.getName();
            }
        }
        if (!user_classes) {
            return "No user-defined classes";
        }
        string result = sprintf("%s\n", color.key("User-defined classes:"));
        foreach string name in (user_classes) {
            result += sprintf("  %s\n", color.type(name));
        }
        return result;
    }

    #! Formats user-defined constants for display
    private string formatConstants() {
        Reflection::Namespace root = Reflection::Namespace::forName(pgm, "::");
        list<Reflection::Constant> constants = root.getConstants();
        # Filter to user-defined constants (no module name)
        list<string> user_consts = ();
        foreach Reflection::Constant c in (constants) {
            if (!c.getModuleName()) {
                user_consts += c.getName();
            }
        }
        if (!user_consts) {
            return "No user-defined constants";
        }
        string result = sprintf("%s\n", color.key("User-defined constants:"));
        foreach string name in (user_consts) {
            result += sprintf("  %s\n", color.value(name));
        }
        return result;
    }

    #! Formats namespaces for display
    private string formatNamespaces() {
        Reflection::Namespace root = Reflection::Namespace::forName(pgm, "::");
        list<Reflection::Namespace> namespaces = root.getNamespaces();
        if (!namespaces) {
            return "No namespaces defined (only root ::)";
        }
        string result = sprintf("%s\n", color.key("Namespaces:"));
        foreach Reflection::Namespace ns in (namespaces) {
            result += sprintf("  %s\n", color.type(ns.getPathName()));
        }
        return result;
    }

    #! Formats loaded modules for display
    private string formatModules() {
        *list<string> features = pgm.getFeatureList();
        if (!features) {
            return "No modules loaded";
        }
        string result = sprintf("%s\n", color.key("Loaded modules:"));
        foreach string name in (features) {
            result += sprintf("  %s\n", name);
        }
        return result;
    }

    #! Gets the type of an expression
    private *hash<auto> getExpressionType(string expr) {
        # Parse and evaluate to get the type
        # Use a unique variable name for each invocation to avoid conflicts
        try {
            string var_name = sprintf("__type_var_%d__", ++eval_counter);
            string code = sprintf("our auto %s = (%s);", var_name, expr);
            string label = sprintf("<type:%d>", eval_counter);
            pgm.parse(code, label);
            pgm.run();
            auto result = pgm.getGlobalVariable(var_name);
            # Use Reflection to get the full type name (e.g., hash<StatInfo> instead of just hash)
            Reflection::Type t = Reflection::Type::getType(result);
            string type_name = t.getName();
            return {"value": sprintf("%s %s", color.key("Type:"), color.type(type_name))};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Describes a symbol (class, function, or namespace)
    private *hash<auto> describeSymbol(string name) {
        # Check for hash<TypeName> or *hash<TypeName> syntax
        *list<*string> match = (name =~ x/^\*?hash<([^>]+)>$/);
        if (match) {
            name = match[0];
        }

        # Try as a hashdecl first (try standard library, then REPL program)
        try {
            Reflection::TypedHash th = Reflection::TypedHash::forName(name);
            return {"value": formatHashdeclDescription(th)};
        } catch () {
            try {
                Reflection::TypedHash th = Reflection::TypedHash::forName(pgm, name);
                return {"value": formatHashdeclDescription(th)};
            } catch () {
                # Not a hashdecl, continue
            }
        }

        # Try as a class (try standard library, then REPL program)
        try {
            Reflection::Class cls = Reflection::Class::forName(name);
            return {"value": formatClassDescription(cls)};
        } catch () {
            try {
                Reflection::Class cls = Reflection::Class::forName(pgm, name);
                return {"value": formatClassDescription(cls)};
            } catch () {
                # Not a class, continue
            }
        }

        # Try as a function (try standard library, then REPL program)
        try {
            Reflection::Function func = Reflection::Function::forName(name);
            return {"value": formatFunctionDescription(func)};
        } catch () {
            try {
                Reflection::Function func = Reflection::Function::forName(pgm, name);
                return {"value": formatFunctionDescription(func)};
            } catch () {
                # Not a function, continue
            }
        }

        # Try as a constant (try standard library, then REPL program)
        try {
            Reflection::Constant c = Reflection::Constant::forName(name);
            return {"value": formatConstantDescription(c)};
        } catch () {
            try {
                Reflection::Constant c = Reflection::Constant::forName(pgm, name);
                return {"value": formatConstantDescription(c)};
            } catch () {
                # Not a constant, continue
            }
        }

        # Try as an enum (try standard library, then REPL program)
        # Note: Check enums BEFORE namespaces since enums are more specific
        try {
            Reflection::Enum en = Reflection::Enum::forName(name);
            return {"value": formatEnumDescription(en)};
        } catch () {
            try {
                Reflection::Enum en = Reflection::Enum::forName(pgm, name);
                return {"value": formatEnumDescription(en)};
            } catch () {
                # Not an enum, continue
            }
        }

        # Try as a namespace (try standard library, then REPL program)
        try {
            Reflection::Namespace ns = Reflection::Namespace::forName(name);
            return {"value": formatNamespaceDescription(ns)};
        } catch () {
            try {
                Reflection::Namespace ns = Reflection::Namespace::forName(pgm, name);
                return {"value": formatNamespaceDescription(ns)};
            } catch () {
                # Not a namespace either
            }
        }

        return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": sprintf("Symbol not found: %s", name)}};
    }

    #! Formats a hashdecl description
    private string formatHashdeclDescription(Reflection::TypedHash th) {
        string result = sprintf("%s %s\n", color.key("Hashdecl:"), color.type(th.getName()));

        *string mod = th.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        # Show members
        list<Reflection::TypedHashMember> members = th.getMembers();
        if (members) {
            result += sprintf("  %s\n", color.key("Members:"));
            foreach Reflection::TypedHashMember m in (members) {
                Reflection::Type t = m.getType();
                string line = sprintf("%s %s", color.type(t.getName()), m.getName());
                # Add default value if present
                auto def = m.getDefaultValue();
                if (exists def) {
                    line += sprintf(" = %s", color.value(formatValue(def)));
                }
                result += sprintf("    %s\n", line);
            }
        }

        return result;
    }

    #! Formats an enum description
    private string formatEnumDescription(Reflection::Enum en) {
        string result = sprintf("%s %s\n", color.key("Enum:"), color.type(en.getName()));

        *string mod = en.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        # Show base type
        Reflection::Type baseType = en.getBaseType();
        result += sprintf("  %s %s\n", color.key("Base type:"), color.type(baseType.getName()));

        # Show all values
        list<Reflection::EnumMember> members = en.getMembers();
        if (members) {
            result += sprintf("  %s\n", color.key("Values:"));
            foreach Reflection::EnumMember m in (members) {
                auto val = m.getValue();
                result += sprintf("    %s = %s\n", m.getName(), color.value(formatValue(val)));
            }
        }

        return result;
    }

    #! Formats a constant description
    private string formatConstantDescription(Reflection::Constant c) {
        string result = sprintf("%s %s\n", color.key("Constant:"), color.value(c.getName()));

        *string mod = c.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        Reflection::Type t = c.getType();
        result += sprintf("  %s %s\n", color.key("Type:"), color.type(t.getName()));

        auto val = c.getValue();
        result += sprintf("  %s %s\n", color.key("Value:"), formatValue(val));

        return result;
    }

    #! Formats a class description
    private string formatClassDescription(Reflection::Class cls) {
        string result = sprintf("%s %s\n", color.key("Class:"), color.type(cls.getPathName()));

        *string mod = cls.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        list<string> mods = cls.getModifierList();
        if (mods) {
            result += sprintf("  %s %s\n", color.key("Modifiers:"), mods.join(" "));
        }

        # Show parent classes if any
        list<hash<auto>> parents = cls.getParentClasses();
        if (parents) {
            list<string> parent_names = ();
            foreach hash<auto> p in (parents) {
                parent_names += color.type(p.cls.getPathName());
            }
            result += sprintf("  %s %s\n", color.key("Inherits:"), parent_names.join(", "));
        }

        # List methods with full signatures
        list<Reflection::AbstractMethod> methods = cls.getMethods();
        if (methods) {
            result += sprintf("  %s\n", color.key("Methods:"));
            foreach Reflection::AbstractMethod m in (methods) {
                # Get all variants for this method to show full signatures
                list<Reflection::AbstractMethodVariant> variants = m.getVariants();
                foreach Reflection::AbstractMethodVariant v in (variants) {
                    result += sprintf("    %s\n", formatMethodSignature(v));
                }
            }
        }

        # List members with types
        list<Reflection::AbstractMember> members = cls.getMembers();
        if (members) {
            result += sprintf("  %s\n", color.key("Members:"));
            foreach Reflection::AbstractMember m in (members) {
                Reflection::Type t = m.getType();
                list<string> modifiers = m.getModifierList();
                # Remove "public" from modifiers (default visibility)
                modifiers = (map $1, modifiers, $1 != "public");
                string mod_str = modifiers ? modifiers.join(" ") + " " : "";
                result += sprintf("    %s%s %s\n", mod_str, color.type(t.getName()), m.getName());
            }
        }

        return result;
    }

    #! Formats a function description
    private string formatFunctionDescription(Reflection::Function func) {
        string result = sprintf("%s %s\n", color.key("Function:"), color.value(func.getName()));

        *string mod = func.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        # Get variants
        list<Reflection::FunctionVariant> variants = func.getVariants();
        if (variants) {
            result += sprintf("  %s\n", color.key("Variants:"));
            foreach Reflection::FunctionVariant v in (variants) {
                result += sprintf("    %s\n", formatFunctionSignature(v));
            }
        }

        return result;
    }

    #! Formats a namespace description
    private string formatNamespaceDescription(Reflection::Namespace ns) {
        string result = sprintf("%s %s\n", color.key("Namespace:"), color.type(ns.getPathName()));

        *string mod = ns.getModuleName();
        if (mod) {
            result += sprintf("  %s %s\n", color.key("Module:"), mod);
        }

        list<Reflection::Class> classes = ns.getClasses();
        if (classes) {
            result += sprintf("  %s %s\n", color.key("Classes:"), color.number(string(classes.size())));
        }

        list<Reflection::Function> funcs = ns.getFunctions();
        if (funcs) {
            result += sprintf("  %s %s\n", color.key("Functions:"), color.number(string(funcs.size())));
        }

        list<Reflection::Constant> consts = ns.getConstants();
        if (consts) {
            result += sprintf("  %s %s\n", color.key("Constants:"), color.number(string(consts.size())));
        }

        list<Reflection::Namespace> subns = ns.getNamespaces();
        if (subns) {
            result += sprintf("  %s %s\n", color.key("Sub-namespaces:"), color.number(string(subns.size())));
        }

        return result;
    }

    #! Formats a method signature with colors
    private string formatMethodSignature(Reflection::AbstractMethodVariant v) {
        string sig = v.toString();
        # Remove "public " prefix (default visibility)
        sig =~ s/^public //;
        # Remove "nothing " return type (void)
        sig =~ s/^nothing //;
        return colorizeSignature(sig);
    }

    #! Formats a function variant signature with colors
    private string formatFunctionSignature(Reflection::FunctionVariant v) {
        string sig = v.toString();
        return colorizeSignature(sig);
    }

    #! Colorizes a method/function signature string
    /** Takes a signature like "int ClassName::methodName(string arg)" and colorizes it
    */
    private string colorizeSignature(string sig) {
        # Pattern: [modifiers] [return_type] name(params)
        # Split into parts before and after the opening paren
        int paren_pos = sig.find("(");
        if (paren_pos < 0) {
            return sig;
        }

        string before_paren = sig.substr(0, paren_pos);
        string params_part = sig.substr(paren_pos);

        # Split before_paren into words
        list<string> words = before_paren.split(" ");
        words = (map trim($1), words, trim($1).length() > 0);

        if (!words) {
            return sig;
        }

        # Last word is the method/function name (possibly with Class::)
        string name = words[words.size() - 1];
        list<string> colored_parts = ();

        # Process words before the name (modifiers and return type)
        # Known modifiers that should NOT be colorized as types
        hash<string, bool> modifiers = {
            "private": True, "public": True, "abstract": True, "static": True,
            "final": True, "synchronized": True, "deprecated": True,
        };
        for (int i = 0; i < words.size() - 1; i++) {
            string word = words[i];
            # Check if it's a known modifier first
            if (modifiers{word}) {
                colored_parts += word;
            } else {
                # Assume it's a type (return type, generic type, namespaced type, etc.)
                colored_parts += color.type(word);
            }
        }

        # Colorize the method/function name
        colored_parts += color.value(name);

        # Colorize parameters
        string colored_params = colorizeParams(params_part);

        return colored_parts.join(" ") + colored_params;
    }

    #! Colorizes the parameters part of a signature
    private string colorizeParams(string params_str) {
        # params_str is like "(type1 name1, type2 name2 = default)"
        if (!params_str.startsWith("(") || !params_str.endsWith(")")) {
            return params_str;
        }

        string inner = params_str.substr(1, params_str.length() - 2);
        if (inner.length() == 0) {
            return "()";
        }

        # Split by comma, but be careful of nested types like hash<string, int>
        list<string> params = splitParams(inner);
        list<string> colored_params = ();

        foreach string param in (params) {
            param = trim(param);
            if (param.length() == 0) {
                continue;
            }

            # Find the parameter name (last identifier before any = sign)
            int eq_pos = param.find("=");
            string type_and_name;
            string default_part = "";
            if (eq_pos >= 0) {
                type_and_name = trim(param.substr(0, eq_pos));
                default_part = " =" + param.substr(eq_pos + 1);
            } else {
                type_and_name = param;
            }

            # Split type and name - name is the last word
            int last_space = type_and_name.rfind(" ");
            if (last_space >= 0) {
                string type_part = type_and_name.substr(0, last_space);
                string name_part = type_and_name.substr(last_space + 1);
                colored_params += color.type(type_part) + " " + name_part + default_part;
            } else {
                # No space, might just be a type or name
                colored_params += param;
            }
        }

        return "(" + colored_params.join(", ") + ")";
    }

    #! Splits parameter string by comma, respecting nested brackets and string literals
    private list<string> splitParams(string params) {
        list<string> result = ();
        int depth = 0;  # Tracks nesting depth for < > ( ) { } [ ]
        int start = 0;
        *string in_string;  # Tracks if we're inside a string literal (' or ")

        for (int i = 0; i < params.length(); i++) {
            string c = params[i];

            # Handle string literals
            if (in_string) {
                if (c == in_string) {
                    # Check for escape (preceding backslash)
                    if (i > 0 && params[i - 1] != "\\") {
                        in_string = NOTHING;
                    }
                }
                continue;
            }

            # Check for string start
            if (c == "\"" || c == "'") {
                in_string = c;
                continue;
            }

            # Track nesting for all bracket types
            if (c == "<" || c == "(" || c == "{" || c == "[") {
                depth++;
            } else if (c == ">" || c == ")" || c == "}" || c == "]") {
                depth--;
            } else if (c == "," && depth == 0) {
                result += params.substr(start, i - start);
                start = i + 1;
            }
        }

        # Add the last parameter
        if (start < params.length()) {
            result += params.substr(start);
        }

        return result;
    }

    #! Formats command history for display
    private string formatHistory() {
        if (!history) {
            return "No command history";
        }
        string result = sprintf("%s\n", color.key("Command history:"));
        int i = 1;
        foreach string cmd in (history) {
            # Truncate long commands and show first line only
            string display = cmd;
            int nl = display.find("\n");
            if (nl >= 0) {
                display = display.substr(0, nl) + "...";
            }
            if (display.length() > 60) {
                display = display.substr(0, 57) + "...";
            }
            result += sprintf("  %s  %s\n", color.number(sprintf("%3d", i++)), display);
        }
        return result;
    }

    #! Saves session history to a file
    private *hash<auto> saveHistory(string filename) {
        try {
            File f();
            f.open2(filename, O_CREAT | O_WRONLY | O_TRUNC);
            foreach string cmd in (history) {
                # Skip commands starting with / (they're REPL commands, not Qore code)
                if (cmd !~ /^\//) {
                    f.write(cmd + "\n");
                }
            }
            return {"value": sprintf("Session saved to: %s", filename)};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Changes the current working directory
    private *hash<auto> changeDirectory(string dir) {
        try {
            chdir(dir);
            return {"value": sprintf("Changed to: %s", getcwd())};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Runs the interactive REPL loop
    /** This method provides a complete interactive loop with optional linenoise support
        for line editing and persistent history.

        @param options Optional configuration:
        - \c no_banner: If True, don't print the welcome banner

        @note When linenoise is available, provides:
        - Line editing with arrow keys
        - Persistent history (Up/Down)
        - Reverse history search (Ctrl+R)
        - Tab completion for REPL commands
    */
    run(*hash<auto> options) {
        # Print banner unless disabled
        if (!options.no_banner) {
            printOutput(sprintf("%s %s %s\n%s %s %s %s %s\n",
                color.info("Qore"), color.value(Qore::VersionString), color.info("Interactive Shell"),
                "Type", color.value("/help"), "for commands,", color.value("/quit"), "to exit"));
        }

%ifndef NoLinenoise
        # Initialize linenoise
        Linenoise::history_set_max_len(MaxHistoryLen);
        Linenoise::set_callback(\completion());
        loadHistoryFile();
%endif

        # Main REPL loop
        while (True) {
            *string line = readLine(getPrompt());

            # Handle EOF or Ctrl+C
            if (!exists line) {
                if (needsMoreInput()) {
                    # In multi-line mode, cancel and reset
                    printOutput("\n");
                    reset();
                    continue;
                }
                # Exit the REPL
                break;
            }

            # Add non-empty lines to linenoise history
%ifndef NoLinenoise
            if (line.length() > 0) {
                Linenoise::history_add(line);
            }
%endif

            # Evaluate the input
            *hash<auto> result = eval(line);

            # Handle exit request
            if (result.exit) {
                break;
            }

            # Display error or result
            if (result.error) {
                printError(color.error("Error: ") + formatError(result.error) + "\n");
            } else if (exists result.value) {
                string output = formatValue(result.value);
                # Only add newline if output doesn't already end with one
                if (!output.endsWith("\n")) {
                    output += "\n";
                }
                printOutput(output);
            }

            # Display timing if enabled
            if (result.timing) {
                printOutput(sprintf("   (%s)\n", result.timing.format("HH:mm:SS.xx")));
            }
        }

%ifndef NoLinenoise
        saveHistoryFile();
%endif
    }

    #! Reads a line of input from the user
    /** @param prompt The prompt to display
        @return The line read, or NOTHING on EOF/Ctrl+C
    */
    private *string readLine(string prompt) {
%ifndef NoLinenoise
        return Linenoise::line(prompt);
%else
        stdout.printf(prompt);
        stdout.sync();
        return stdin.readLine(False);
%endif
    }

    #! Prints output to stdout with CRLF handling for linenoise
    /** @param text The text to print
    */
    private printOutput(string text) {
%ifndef NoLinenoise
        # Linenoise requires CRLF for proper line handling in raw mode
        # First normalize any existing \r\n to \n to avoid \r\r\n
        text = replace(text, "\r\n", "\n");
        stdout.print(replace(text, "\n", "\r\n"));
%else
        stdout.print(text);
%endif
    }

    #! Prints error output to stderr with CRLF handling for linenoise
    /** @param text The text to print
    */
    private printError(string text) {
%ifndef NoLinenoise
        # Linenoise requires CRLF for proper line handling in raw mode
        # First normalize any existing \r\n to \n to avoid \r\r\n
        text = replace(text, "\r\n", "\n");
        stderr.print(replace(text, "\n", "\r\n"));
%else
        stderr.print(text);
%endif
    }

    #! Provides tab completion for REPL commands
    /** @param input The current input string
        @return A list of matching command completions
    */
    private softlist<string> completion(string input) {
        # Only complete commands starting with /
        if (!input.startsWith("/")) {
            return ();
        }

        # Lowercase input for case-insensitive matching (consistent with command execution)
        string input_lower = input.lwr();

        # Find commands that match the input prefix
        list<string> matches = ();
        foreach string cmd in (ReplCommands) {
            if (cmd.equalPartial(input_lower)) {
                matches += cmd;
            }
        }
        return matches;
    }

    #! Loads history from the history file
    private loadHistoryFile() {
%ifndef NoLinenoise
        if (!history_file) {
            return;
        }
        try {
            # Create the history file if it doesn't exist
            if (!is_file(history_file)) {
                File f();
                f.open2(history_file, O_CREAT | O_WRONLY, 0600);
            }
            Linenoise::history_load(history_file);
        } catch (hash<ExceptionInfo> ex) {
            # Warn but don't fail - history is optional
            stderr.printf("Warning: Could not load history file: %s\n", ex.desc);
        }
%endif
    }

    #! Saves history to the history file
    private saveHistoryFile() {
%ifndef NoLinenoise
        if (!history_file) {
            return;
        }
        try {
            Linenoise::history_save(history_file);
        } catch (hash<ExceptionInfo> ex) {
            # Warn but don't fail - history is optional
            stderr.printf("Warning: Could not save history file: %s\n", ex.desc);
        }
%endif
    }

    #! Returns completions for a given input (for testing)
    /** @param input The input string to complete
        @return A list of matching completions
    */
    softlist<string> getCompletions(string input) {
        return completion(input);
    }
}

} # namespace QoreRepl
