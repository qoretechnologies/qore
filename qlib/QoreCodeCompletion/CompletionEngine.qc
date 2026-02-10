# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CompletionEngine class definition

/** CompletionEngine.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreCodeCompletion {

#! Structured completion item for rich completion
public hashdecl CompletionItem {
    #! Display label for the completion
    string label;
    #! Kind of completion: "keyword", "function", "class", "variable", "constant",
    #! "namespace", "method", "member", "hashdecl", "enum", "enum_value"
    string kind;
    #! Optional type signature or detail
    *string detail;
    #! Optional documentation comment
    *string documentation;
    #! Whether the item is callable (method/function)
    bool isCallable = False;
    #! Whether the callable has only a single no-argument variant
    bool noArgsOnly = False;
}

#! Completion context: no completable token
public const CC_NONE = 0;
#! Completion context: bare identifier (keywords, types, all symbols)
public const CC_IDENTIFIER = 1;
#! Completion context: after "new " -- class names only
public const CC_NEW = 2;
#! Completion context: after "." -- instance members/methods
public const CC_DOT = 3;
#! Completion context: after "::" -- namespace/class static/enum members
public const CC_SCOPE = 4;
#! Completion context: inside function/method call arguments (after "(" or ",")
public const CC_CALL = 5;

#! Qore keywords and built-in type names for identifier completion
public const QoreKeywords = (
    # Control flow
    "if", "else", "while", "do", "for", "foreach", "switch", "case", "default",
    "break", "continue", "return", "throw", "rethrow", "try", "catch",
    # Declarations
    "class", "inherits", "abstract", "final", "public", "private", "static",
    "synchronized", "deprecated", "namespace", "sub", "new", "delete",
    # Threading
    "background", "thread_exit",
    # On-block
    "on_exit", "on_error", "on_success",
    # Variable scope
    "our", "my",
    # Other declarations
    "const", "hashdecl", "enum", "typedef",
    # Built-in type names
    "int", "string", "float", "number", "bool", "date", "binary",
    "hash", "list", "auto", "nothing", "object",
    "softint", "softstring", "softbool", "softlist", "softdate",
    "softfloat", "softnumber",
    "code", "reference", "timeout", "data",
    # Built-in values
    "True", "False", "NOTHING", "NULL",
    # Expression keywords
    "map", "select", "foldl", "foldr", "exists", "instanceof",
    "elements", "keys", "cast", "trim",
);

#! Abstract interface for symbol resolution backends
/** Consumers implement this class to provide symbol resolution from their backend
    (e.g., Reflection API for runtime, CSTSearcher for static analysis).
*/
public class AbstractCompletionProvider {
    #! Get all visible symbols for bare identifier completion
    /** @param prefix The prefix to filter by
        @return A list of matching completion items
    */
    abstract list<hash<CompletionItem>> getVisibleSymbols(string prefix);

    #! Get class names for "new" completion
    /** @param prefix The prefix to filter by
        @return A list of matching class completion items
    */
    abstract list<hash<CompletionItem>> getClassNames(string prefix);

    #! Get instance members/methods for dot completion (after ".")
    /** @param target the identifier before "."
        @param prefix The prefix to filter by
        @return A list of matching member/method completion items
    */
    abstract list<hash<CompletionItem>> getInstanceMembers(string target, string prefix);

    #! Get namespace/class/enum members for scope completion (after "::")
    /** @param target the identifier before "::"
        @param prefix The prefix to filter by
        @return A list of matching scope member completion items
    */
    abstract list<hash<CompletionItem>> getScopeMembers(string target, string prefix);

    #! Returns the active parse options for the current context
    /** Override this to return the parse options from your runtime/document context.
        The default implementation returns 0 (no restrictions).
        @return bitfield of PO_* parse option flags
    */
    int getParseOptions() {
        return 0;
    }
}

#! Shared completion engine with context detection, dispatch, filtering, and sorting
/** This class provides the shared logic for code completion across different consumers
    (REPL, Language Server, etc.). It delegates symbol resolution to an
    @ref AbstractCompletionProvider "AbstractCompletionProvider" implementation.

    @par Example
    @code{.py}
CompletionEngine engine(myProvider);
list<hash<CompletionItem>> items = engine.complete("Qore::Thread::M");
    @endcode
*/
public class CompletionEngine {
    private {
        AbstractCompletionProvider provider;
    }

    #! Creates a new CompletionEngine with the given provider
    /** @param provider The symbol resolution backend to use
    */
    constructor(AbstractCompletionProvider provider) {
        self.provider = provider;
    }

    #! Main entry: detect context and dispatch
    /** @param input The current input line up to cursor
        @return A list of matching completion items
    */
    list<hash<CompletionItem>> complete(string input) {
        return completeForContext(extractCompletionContext(input));
    }

    #! Complete for a pre-extracted context
    /** Lets consumers handle custom contexts (e.g., REPL commands) before delegating.
        @param ctx A context hash with keys: kind, prefix, target, line_before
        @return A list of matching completion items
    */
    list<hash<CompletionItem>> completeForContext(hash<auto> ctx) {
        switch (ctx.kind) {
            case CC_IDENTIFIER:
                return completeIdentifier(ctx);
            case CC_NEW:
                return completeNew(ctx);
            case CC_DOT:
                return completeDot(ctx);
            case CC_SCOPE:
                return completeScope(ctx);
            case CC_CALL:
                return completeIdentifier(ctx);
        }
        return ();
    }

    #! Extracts completion context from the current input line
    /** @param input The current input line
        @return A hash with keys: kind (int), prefix (string), target (*string), line_before (string)
    */
    static hash<auto> extractCompletionContext(string input) {
        # Scan backward from end collecting identifier chars
        int pos = input.length() - 1;
        while (pos >= 0 && (input[pos] =~ /[a-zA-Z_0-9]/)) {
            pos--;
        }
        string prefix = input.substr(pos + 1);
        string before_prefix = pos >= 0 ? input.substr(0, pos + 1) : "";

        # Check what's immediately before prefix
        if (before_prefix.length() > 0) {
            string last_char = before_prefix[before_prefix.length() - 1];

            # Dot completion
            if (last_char == ".") {
                string target_before = before_prefix.substr(0, before_prefix.length() - 1);
                string target = collectTargetBackward(target_before);
                return {
                    "kind": CC_DOT,
                    "prefix": prefix,
                    "target": target,
                    "line_before": before_prefix,
                };
            }

            # Scope completion (::)
            if (before_prefix.length() >= 2
                    && before_prefix.substr(before_prefix.length() - 2) == "::") {
                string target_before = before_prefix.substr(0, before_prefix.length() - 2);
                string target = collectTargetBackward(target_before);
                return {
                    "kind": CC_SCOPE,
                    "prefix": prefix,
                    "target": target,
                    "line_before": before_prefix,
                };
            }

            # Check for "new" keyword before the prefix
            string trimmed_before = rtrim(before_prefix);
            if (trimmed_before.length() >= 3) {
                # Get the last word before the prefix
                int wp = trimmed_before.length() - 1;
                while (wp >= 0 && trimmed_before[wp] =~ /[a-zA-Z_0-9]/) {
                    wp--;
                }
                string last_word = trimmed_before.substr(wp + 1);
                if (last_word == "new") {
                    return {
                        "kind": CC_NEW,
                        "prefix": prefix,
                        "line_before": before_prefix,
                    };
                }
            }
        }

        # Bare identifier or nothing
        if (prefix.length() == 0) {
            # Check if we're inside a call context (after "(" or ",")
            string trimmed_bp = rtrim(before_prefix);
            if (trimmed_bp.length() > 0) {
                string tc = trimmed_bp[trimmed_bp.length() - 1];
                if (tc == "(" || tc == ",") {
                    return {
                        "kind": CC_CALL,
                        "prefix": "",
                        "line_before": before_prefix,
                    };
                }
            }
            return {
                "kind": CC_NONE,
                "prefix": "",
                "line_before": before_prefix,
            };
        }

        return {
            "kind": CC_IDENTIFIER,
            "prefix": prefix,
            "line_before": before_prefix,
        };
    }

    #! Collects a target identifier or scoped name backward from a position
    /** @param input The input string to scan backward from the end
        @return The collected target name (may include :: separators)
    */
    static string collectTargetBackward(string input) {
        int pos = input.length() - 1;

        # Skip trailing whitespace
        while (pos >= 0 && input[pos] == " ") {
            pos--;
        }

        # Collect identifier chars and :: chains
        string result = "";
        while (pos >= 0) {
            if (input[pos] =~ /[a-zA-Z_0-9]/) {
                result = input[pos] + result;
                pos--;
            } else if (pos >= 1 && input[pos] == ":" && input[pos - 1] == ":") {
                result = "::" + result;
                pos -= 2;
            } else {
                break;
            }
        }

        return result;
    }

    #! Computes the word start position using the same break characters as linenoise
    /** @param input The full input line
        @return The position of the first character of the current word

        @note Break characters must match linenoise's breakChars in linenoise.cpp
    */
    static int computeWordStart(string input) {
        # These must match linenoise's breakChars: " =+-/\\*?\"'`&<>:;|@{([])}";
        string break_chars = " =+-/\\*?\"'`&<>:;|@{([])}";
        int pos = input.length() - 1;
        while (pos >= 0) {
            if (break_chars.find(input[pos]) >= 0) {
                return pos + 1;
            }
            pos--;
        }
        return 0;
    }

    #! Checks if a candidate matches a prefix (case-sensitive, empty prefix matches all)
    /** @param candidate The candidate string to check
        @param prefix The prefix to match against (case-sensitive)
        @return True if candidate matches the prefix
    */
    static bool matchesPrefix(string candidate, string prefix) {
        if (prefix.length() == 0) {
            return True;
        }
        return candidate.equalPartial(prefix);
    }

    #! Completes bare identifiers: keywords and symbols from provider
    private list<hash<CompletionItem>> completeIdentifier(hash<auto> ctx) {
        list<hash<CompletionItem>> result = ();
        hash<string, bool> seen;

        # Keywords (only if prefix >= 2 chars to reduce noise)
        if (ctx.prefix.length() >= 2) {
            int po = provider.getParseOptions();

            foreach string kw in (QoreKeywords) {
                if (!matchesPrefix(kw, ctx.prefix) || seen{kw}) {
                    continue;
                }
                # Skip thread-control keywords when PO_NO_THREAD_CONTROL is set
                if ((po & PO_NO_THREAD_CONTROL)
                        && (kw == "background" || kw == "thread_exit")) {
                    continue;
                }
                seen{kw} = True;
                result += <CompletionItem>{"label": kw, "kind": "keyword"};
            }
        }

        # Visible symbols from provider
        foreach hash<CompletionItem> item in (provider.getVisibleSymbols(ctx.prefix)) {
            if (!seen{item.label}) {
                seen{item.label} = True;
                result += item;
            }
        }

        return sort(result, int sub (hash<CompletionItem> a, hash<CompletionItem> b) {
            return a.label <=> b.label;
        });
    }

    #! Completes class names after "new"
    private list<hash<CompletionItem>> completeNew(hash<auto> ctx) {
        list<hash<CompletionItem>> result = ();
        hash<string, bool> seen;

        foreach hash<CompletionItem> item in (provider.getClassNames(ctx.prefix)) {
            if (!seen{item.label}) {
                seen{item.label} = True;
                result += item;
            }
        }

        return sort(result, int sub (hash<CompletionItem> a, hash<CompletionItem> b) {
            return a.label <=> b.label;
        });
    }

    #! Completes instance members after "."
    private list<hash<CompletionItem>> completeDot(hash<auto> ctx) {
        list<hash<CompletionItem>> result = ();
        hash<string, bool> seen;

        foreach hash<CompletionItem> item in (provider.getInstanceMembers(ctx.target, ctx.prefix)) {
            if (!seen{item.label}) {
                seen{item.label} = True;
                result += item;
            }
        }

        return sort(result, int sub (hash<CompletionItem> a, hash<CompletionItem> b) {
            return a.label <=> b.label;
        });
    }

    #! Completes members after "::"
    private list<hash<CompletionItem>> completeScope(hash<auto> ctx) {
        list<hash<CompletionItem>> result = ();
        hash<string, bool> seen;

        foreach hash<CompletionItem> item in (provider.getScopeMembers(ctx.target, ctx.prefix)) {
            if (!seen{item.label}) {
                seen{item.label} = True;
                result += item;
            }
        }

        return sort(result, int sub (hash<CompletionItem> a, hash<CompletionItem> b) {
            return a.label <=> b.label;
        });
    }
}

} # namespace QoreCodeCompletion
