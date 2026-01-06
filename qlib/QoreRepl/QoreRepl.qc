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

%modern
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

        #! Command history
        list<string> history = ();

        #! Whether to show execution timing
        bool show_timing = False;
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
        return "Commands:
  /help              Show this help
  /quit              Exit the REPL (also /exit, /q)

Session:
  /reset             Reset the Program state
  /clear             Clear pending multi-line input
  /history           Show command history
  /timing            Toggle execution timing display

Introspection:
  /vars              List global variables
  /functions         List user-defined functions (also /funcs)
  /classes           List user-defined classes
  /constants         List user-defined constants (also /consts)
  /namespaces        List namespaces (also /ns)
  /modules           List loaded modules
  /type <expr>       Show the type of an expression
  /describe <name>   Describe a class, function, or namespace

Files:
  /load <file>       Load and execute a file
  /save <file>       Save session history to a file
  /pwd               Show current working directory
  /cd <dir>          Change working directory

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
        string result = "Global variables:\n";
        foreach string name in (keys user_vars) {
            result += sprintf("  %s = %s\n", name, formatValue(user_vars{name}));
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
        string result = "User-defined functions:\n";
        foreach string name in (funcs) {
            result += sprintf("  %s()\n", name);
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
        string result = "User-defined classes:\n";
        foreach string name in (user_classes) {
            result += sprintf("  %s\n", name);
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
        string result = "User-defined constants:\n";
        foreach string name in (user_consts) {
            result += sprintf("  %s\n", name);
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
        string result = "Namespaces:\n";
        foreach Reflection::Namespace ns in (namespaces) {
            result += sprintf("  %s\n", ns.getPathName());
        }
        return result;
    }

    #! Formats loaded modules for display
    private string formatModules() {
        *list<string> features = pgm.getFeatureList();
        if (!features) {
            return "No modules loaded";
        }
        string result = "Loaded modules:\n";
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
            string type_name = result.type();
            if (result.typeCode() == NT_OBJECT) {
                type_name = result.className();
            }
            return {"value": sprintf("Type: %s", type_name)};
        } catch (hash<ExceptionInfo> ex) {
            return {"error": ex};
        }
    }

    #! Describes a symbol (class, function, or namespace)
    private *hash<auto> describeSymbol(string name) {
        # Try as a class first
        try {
            Reflection::Class cls = Reflection::Class::forName(pgm, name);
            return {"value": formatClassDescription(cls)};
        } catch () {
            # Not a class, continue
        }

        # Try as a function
        try {
            Reflection::Function func = Reflection::Function::forName(pgm, name);
            return {"value": formatFunctionDescription(func)};
        } catch () {
            # Not a function, continue
        }

        # Try as a namespace
        try {
            Reflection::Namespace ns = Reflection::Namespace::forName(pgm, name);
            return {"value": formatNamespaceDescription(ns)};
        } catch () {
            # Not a namespace either
        }

        return {"error": <ExceptionInfo>{"err": "REPL-ERROR", "desc": sprintf("Symbol not found: %s", name)}};
    }

    #! Formats a class description
    private string formatClassDescription(Reflection::Class cls) {
        string result = sprintf("Class: %s\n", cls.getPathName());

        *string mod = cls.getModuleName();
        if (mod) {
            result += sprintf("  Module: %s\n", mod);
        }

        list<string> mods = cls.getModifierList();
        if (mods) {
            result += sprintf("  Modifiers: %s\n", mods.join(" "));
        }

        # Show parent classes if any
        list<hash<auto>> parents = cls.getParentClasses();
        if (parents) {
            list<string> parent_names = ();
            foreach hash<auto> p in (parents) {
                parent_names += p.cls.getPathName();
            }
            result += sprintf("  Inherits: %s\n", parent_names.join(", "));
        }

        # List methods
        list<Reflection::AbstractMethod> methods = cls.getMethods();
        if (methods) {
            result += "  Methods:\n";
            foreach Reflection::AbstractMethod m in (methods) {
                result += sprintf("    %s()\n", m.getName());
            }
        }

        # List members
        list<Reflection::AbstractMember> members = cls.getMembers();
        if (members) {
            result += "  Members:\n";
            foreach Reflection::AbstractMember m in (members) {
                result += sprintf("    %s\n", m.getName());
            }
        }

        return result;
    }

    #! Formats a function description
    private string formatFunctionDescription(Reflection::Function func) {
        string result = sprintf("Function: %s\n", func.getName());

        *string mod = func.getModuleName();
        if (mod) {
            result += sprintf("  Module: %s\n", mod);
        }

        # Get variants
        list<Reflection::FunctionVariant> variants = func.getVariants();
        if (variants) {
            result += "  Variants:\n";
            foreach Reflection::FunctionVariant v in (variants) {
                result += sprintf("    %s\n", v.toString());
            }
        }

        return result;
    }

    #! Formats a namespace description
    private string formatNamespaceDescription(Reflection::Namespace ns) {
        string result = sprintf("Namespace: %s\n", ns.getPathName());

        *string mod = ns.getModuleName();
        if (mod) {
            result += sprintf("  Module: %s\n", mod);
        }

        list<Reflection::Class> classes = ns.getClasses();
        if (classes) {
            result += sprintf("  Classes: %d\n", classes.size());
        }

        list<Reflection::Function> funcs = ns.getFunctions();
        if (funcs) {
            result += sprintf("  Functions: %d\n", funcs.size());
        }

        list<Reflection::Constant> consts = ns.getConstants();
        if (consts) {
            result += sprintf("  Constants: %d\n", consts.size());
        }

        list<Reflection::Namespace> subns = ns.getNamespaces();
        if (subns) {
            result += sprintf("  Sub-namespaces: %d\n", subns.size());
        }

        return result;
    }

    #! Formats command history for display
    private string formatHistory() {
        if (!history) {
            return "No command history";
        }
        string result = "Command history:\n";
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
            result += sprintf("  %3d  %s\n", i++, display);
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
}

} # namespace QoreRepl
