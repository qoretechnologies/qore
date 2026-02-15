# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CodeFormatter.qc Main formatter class

/*  CodeFormatter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main QoreCodeFormat namespace
public namespace QoreCodeFormat {

#! AST node type constants
public const ANT_Declaration = 1;
public const ANT_Expression = 2;
public const ANT_Name = 3;
public const ANT_ParseOption = 5;
public const ANT_Statement = 6;

#! Declaration kind constants
public const ADK_Class = 1;
public const ADK_Closure = 2;
public const ADK_Constant = 3;
public const ADK_Function = 4;
public const ADK_Hash = 5;
public const ADK_HashMember = 6;
public const ADK_MemberGroup = 7;
public const ADK_Namespace = 8;
public const ADK_Superclass = 9;
public const ADK_Variable = 10;
public const ADK_VarList = 11;
public const ADK_Typedef = 12;
public const ADK_Enum = 13;
public const ADK_EnumMember = 14;
public const ADK_Module = 15;

#! Statement kind constants
public const ASK_Block = 1;
public const ASK_Break = 2;
public const ASK_Call = 3;
public const ASK_Context = 4;
public const ASK_Continue = 5;
public const ASK_DoWhile = 6;
public const ASK_Expression = 7;
public const ASK_For = 8;
public const ASK_Foreach = 9;
public const ASK_If = 10;
public const ASK_OnBlockExit = 11;
public const ASK_Rethrow = 12;
public const ASK_Return = 13;
public const ASK_Summarize = 14;
public const ASK_Switch = 15;
public const ASK_ThreadExit = 16;
public const ASK_Throw = 17;
public const ASK_Try = 18;
public const ASK_While = 19;
public const ASK_Assert = 20;
public const ASK_Debug = 21;

#! Expression kind constants
public const AEK_Access = 1;
public const AEK_Assignment = 2;
public const AEK_Backquote = 3;
public const AEK_Binary = 4;
public const AEK_Call = 5;
public const AEK_Case = 6;
public const AEK_Cast = 7;
public const AEK_Closure = 8;
public const AEK_ConstrInit = 9;
public const AEK_ContextMod = 10;
public const AEK_ContextRow = 11;
public const AEK_Decl = 12;
public const AEK_Find = 13;
public const AEK_Hash = 14;
public const AEK_HashElement = 15;
public const AEK_ImplicitArg = 16;
public const AEK_ImplicitElem = 17;
public const AEK_Index = 18;
public const AEK_List = 19;
public const AEK_Literal = 20;
public const AEK_Name = 21;
public const AEK_Range = 22;
public const AEK_Regex = 23;
public const AEK_RegexSubst = 24;
public const AEK_RegexTrans = 25;
public const AEK_Returns = 26;
public const AEK_SwitchBody = 27;
public const AEK_Ternary = 28;
public const AEK_Unary = 29;
public const AEK_HashdeclHash = 30;
public const AEK_StringConcat = 31;

#! Operator string mapping
public const OperatorStrings = {
    "AOK_Plus": "+",
    "AOK_Minus": "-",
    "AOK_Multiply": "*",
    "AOK_Divide": "/",
    "AOK_Modulo": "%",
    "AOK_UnaryPlus": "+",
    "AOK_UnaryMinus": "-",
    "AOK_BinaryAnd": "&",
    "AOK_BinaryOr": "|",
    "AOK_BinaryXor": "^",
    "AOK_BinaryNot": "~",
    "AOK_ShiftLeft": "<<",
    "AOK_ShiftRight": ">>",
    "AOK_Assignment": "=",
    "AOK_PlusEquals": "+=",
    "AOK_MinusEquals": "-=",
    "AOK_MultiplyEquals": "*=",
    "AOK_DivideEquals": "/=",
    "AOK_ModuloEquals": "%=",
    "AOK_AndEquals": "&=",
    "AOK_OrEquals": "|=",
    "AOK_XorEquals": "^=",
    "AOK_ShiftLeftEquals": "<<=",
    "AOK_ShiftRightEquals": ">>=",
    "AOK_Equals": "==",
    "AOK_NotEquals": "!=",
    "AOK_Comparison": "<=>",
    "AOK_GreaterThan": ">",
    "AOK_GreaterThanOrEquals": ">=",
    "AOK_LessThan": "<",
    "AOK_LessThanOrEquals": "<=",
    "AOK_AbsoluteEquals": "===",
    "AOK_AbsoluteNotEquals": "!==",
    "AOK_LogicalAnd": "&&",
    "AOK_LogicalOr": "||",
    "AOK_LogicalNot": "!",
    "AOK_NullCoalesce": "??",
    "AOK_ValueCoalesce": "?*",
    "AOK_RegexMatch": "=~",
    "AOK_RegexNotMatch": "!~",
    "AOK_Background": "background",
    "AOK_Chomp": "chomp",
    "AOK_Delete": "delete",
    "AOK_Elements": "elements",
    "AOK_Exists": "exists",
    "AOK_Extract": "extract",
    "AOK_Foldl": "foldl",
    "AOK_Foldr": "foldr",
    "AOK_Instanceof": "instanceof",
    "AOK_Keys": "keys",
    "AOK_Map": "map",
    "AOK_New": "new",
    "AOK_Pop": "pop",
    "AOK_Push": "push",
    "AOK_Remove": "remove",
    "AOK_Select": "select",
    "AOK_Shift": "shift",
    "AOK_Splice": "splice",
    "AOK_Trim": "trim",
    "AOK_Unshift": "unshift",
    "AOK_Reference": "\\",
    "AOK_PreIncrement": "++",
    "AOK_PostIncrement": "++",
    "AOK_PreDecrement": "--",
    "AOK_PostDecrement": "--",
    "AOK_BrokenLogicalOr": "||",
    "AOK_BrokenBinaryOr": "|",
    "AOK_BrokenBinaryXor": "^",
    "AOK_WS_LOGICAL_LE": "<=",
    "AOK_WS_LOGICAL_GE": ">=",
    "AOK_WS_LOGICAL_NE": "!=",
    "AOK_WS_LOGICAL_EQ": "==",
    "AOK_WS_ABSOLUTE_EQ": "===",
    "AOK_WS_ABSOLUTE_NE": "!==",
    "AOK_WS_LOGICAL_CMP": "<=>",
    "AOK_WS_LOGICAL_AND": "&&",
    "AOK_WS_LOGICAL_OR": "||",
    "AOK_WS_BROKEN_LOGICAL_OR": "||",
    "AOK_WS_SHIFT_RIGHT": ">>",
    "AOK_WS_SHIFT_LEFT": "<<",
    "AOK_WS_PLUS_EQUALS": "+=",
    "AOK_WS_MINUS_EQUALS": "-=",
    "AOK_WS_AND_EQUALS": "&=",
    "AOK_WS_OR_EQUALS": "|=",
    "AOK_WS_MODULO_EQUALS": "%=",
    "AOK_WS_MULTIPLY_EQUALS": "*=",
    "AOK_WS_DIVIDE_EQUALS": "/=",
    "AOK_WS_XOR_EQUALS": "^=",
    "AOK_WS_SHIFT_RIGHT_EQUALS": ">>=",
    "AOK_WS_SHIFT_LEFT_EQUALS": "<<=",
};

#! Operator precedence table (higher number = tighter binding)
public const OperatorPrecedence = {
    "AOK_NullCoalesce": 1, "AOK_ValueCoalesce": 1,
    "AOK_LogicalOr": 2, "AOK_BrokenLogicalOr": 2,
    "AOK_WS_LOGICAL_OR": 2, "AOK_WS_BROKEN_LOGICAL_OR": 2,
    "AOK_LogicalAnd": 3, "AOK_WS_LOGICAL_AND": 3,
    "AOK_BinaryOr": 4, "AOK_BrokenBinaryOr": 4,
    "AOK_BinaryXor": 5, "AOK_BrokenBinaryXor": 5,
    "AOK_BinaryAnd": 6,
    "AOK_Equals": 7, "AOK_NotEquals": 7,
    "AOK_AbsoluteEquals": 7, "AOK_AbsoluteNotEquals": 7,
    "AOK_WS_LOGICAL_EQ": 7, "AOK_WS_LOGICAL_NE": 7,
    "AOK_WS_ABSOLUTE_EQ": 7, "AOK_WS_ABSOLUTE_NE": 7,
    "AOK_Comparison": 8, "AOK_WS_LOGICAL_CMP": 8,
    "AOK_GreaterThan": 8, "AOK_GreaterThanOrEquals": 8,
    "AOK_LessThan": 8, "AOK_LessThanOrEquals": 8,
    "AOK_WS_LOGICAL_GE": 8, "AOK_WS_LOGICAL_LE": 8,
    "AOK_RegexMatch": 8, "AOK_RegexNotMatch": 8,
    "AOK_ShiftLeft": 9, "AOK_ShiftRight": 9,
    "AOK_WS_SHIFT_LEFT": 9, "AOK_WS_SHIFT_RIGHT": 9,
    "AOK_Plus": 10, "AOK_Minus": 10,
    "AOK_Multiply": 11, "AOK_Divide": 11, "AOK_Modulo": 11,
};

#! Parse option kind to directive string mapping
public const ParseOptionStrings = {
    "APOK_NEW_STYLE": "%new-style",
    "APOK_MODERN": "%modern",
    "APOK_REQUIRE_TYPES": "%require-types",
    "APOK_STRICT_ARGS": "%strict-args",
    "APOK_ENABLE_ALL_WARNINGS": "%enable-all-warnings",
    "APOK_DISABLE_ALL_WARNINGS": "%disable-all-warnings",
    "APOK_ASSUME_LOCAL": "%assume-local",
    "APOK_ASSUME_GLOBAL": "%assume-global",
    "APOK_REQUIRE_OUR": "%require-our",
    "APOK_REQUIRE_DOLLAR": "%require-dollar",
    "APOK_ALLOW_BARE_REFS": "%allow-bare-refs",
    "APOK_OLD_STYLE": "%old-style",
    "APOK_LOCKDOWN": "%lockdown",
    "APOK_ENABLE_DEBUG": "%enable-debug",
    "APOK_DISABLE_DEBUG": "%disable-debug",
    "APOK_STRICT_BOOLEAN_EVAL": "%strict-boolean-eval",
    "APOK_PERL_BOOLEAN_EVAL": "%perl-boolean-eval",
    "APOK_STRONG_ENCAPSULATION": "%strong-encapsulation",
    "APOK_ALLOW_INJECTION": "%allow-injection",
    "APOK_ALLOW_DEBUGGER": "%allow-debugger",
    "APOK_ALLOW_WEAK_REFERENCES": "%allow-weak-references",
    "APOK_ALLOW_RETURNS": "%allow-returns",
    "APOK_STRICT_TYPES": "%strict-types",
    "APOK_LOOSE_TYPES": "%loose-types",
    "APOK_LOOSE_ARGS": "%loose-args",
    "APOK_NO_GLOBAL_VARS": "%no-global-vars",
    "APOK_NO_CHILD_PO_RESTRICTIONS": "%no-child-po-restrictions",
    "APOK_NO_CLASS_DEFS": "%no-class-defs",
    "APOK_NO_CONSTANT_DEFS": "%no-constant-defs",
    "APOK_NO_DATABASE": "%no-database",
    "APOK_NO_DEBUGGING": "%no-debugging",
    "APOK_NO_EXTERNAL_PROCESS": "%no-external-process",
    "APOK_NO_EXTERNAL_INFO": "%no-external-info",
    "APOK_NO_EXTERNAL_ACCESS": "%no-external-access",
    "APOK_NO_FILESYSTEM": "%no-filesystem",
    "APOK_NO_GUI": "%no-gui",
    "APOK_NO_IO": "%no-io",
    "APOK_NO_LOCALE_CONTROL": "%no-locale-control",
    "APOK_NO_MODULES": "%no-modules",
    "APOK_NO_NAMESPACE_DEFS": "%no-namespace-defs",
    "APOK_NO_NETWORK": "%no-network",
    "APOK_NO_NEW": "%no-new",
    "APOK_NO_PROCESS_CONTROL": "%no-process-control",
    "APOK_NO_REFLECTION": "%no-reflection",
    "APOK_NO_SUBROUTINE_DEFS": "%no-subroutine-defs",
    "APOK_NO_TERMINAL_IO": "%no-terminal-io",
    "APOK_NO_THREADS": "%no-threads",
    "APOK_NO_THREAD_CLASSES": "%no-thread-classes",
    "APOK_NO_THREAD_CONTROL": "%no-thread-control",
    "APOK_NO_THREAD_INFO": "%no-thread-info",
    "APOK_NO_TOP_LEVEL_STATEMENTS": "%no-top-level-statements",
    "APOK_NO_TRANSIENT": "%no-transient",
    "APOK_NO_TYPEDEF": "%no-typedef",
    "APOK_NO_ENUM": "%no-enum",
    "APOK_NO_UNCONTROLLED_APIS": "%no-uncontrolled-apis",
    "APOK_LOCK_WARNINGS": "%lock-warnings",
    "APOK_LOCK_OPTIONS": "%lock-options",
    "APOK_ALLOW_STATEMENT_NO_EFFECT": "%allow-statement-no-effect",
    "APOK_BROKEN_INT_ASSIGNMENTS": "%broken-int-assignments",
    "APOK_BROKEN_LIST_PARSING": "%broken-list-parsing",
    "APOK_BROKEN_LOGIC_PRECEDENCE": "%broken-logic-precedence",
    "APOK_BROKEN_LOOP_STATEMENT": "%broken-loop-statement",
    "APOK_BROKEN_OPERATORS": "%broken-operators",
    "APOK_BROKEN_NAMESPACE_RESOLUTION": "%broken-namespace-resolution",
    "APOK_BROKEN_REFERENCES": "%broken-references",
    "APOK_BROKEN_RANGE": "%broken-range",
    "APOK_BROKEN_SPRINTF": "%broken-sprintf",
    "APOK_BROKEN_CAST": "%broken-cast",
    "APOK_BROKEN_VARARGS": "%broken-varargs",
    "APOK_CORRECT_INT_ASSIGNMENTS": "%correct-int-assignments",
    "APOK_CORRECT_LIST_PARSING": "%correct-list-parsing",
    "APOK_CORRECT_LOGIC_PRECEDENCE": "%correct-logic-precedence",
    "APOK_CORRECT_LOOP_STATEMENT": "%correct-loop-statement",
    "APOK_CORRECT_OPERATORS": "%correct-operators",
    "APOK_CORRECT_NAMESPACE_RESOLUTION": "%correct-namespace-resolution",
    "APOK_CORRECT_REFERENCES": "%correct-references",
    "APOK_CORRECT_RANGE": "%correct-range",
    "APOK_CORRECT_SPRINTF": "%correct-sprintf",
    "APOK_CORRECT_CAST": "%correct-cast",
    "APOK_CORRECT_VARARGS": "%correct-varargs",
};

#! Main code formatter class
/** Formats Qore source code in canonical style.

    @par Example:
    @code{.py}
    QoreCodeFormat::CodeFormatter fmt();
    string formatted = fmt.format(source_code);
    @endcode
*/
public class CodeFormatter {
    public {
        #! Maximum line length before wrapping
        const MAX_LINE_LENGTH = 120;

        #! Double-indent string for continuation lines (8 spaces)
        const CONTINUATION_INDENT = "        ";
    }

    private {
        #! Source emitter
        SourceEmitter emitter();

        #! Original source lines (for literal extraction)
        list<string> sourceLines;

        #! Original source text
        string sourceText;

        #! Comment attacher
        *CommentAttacher commentAttacher;

        #! All comments sorted by location
        list<hash<CommentInfo>> allComments;

        #! Index tracking which comments have been emitted
        int nextCommentIdx = 0;

        #! Whether we're currently inside a class declaration
        bool inClassContext = False;
    }

    #! Format Qore source code in canonical style
    /** @param source the Qore source code to format
        @return formatted source code

        @throw FORMAT-ERROR if the source code has parse errors
    */
    string format(string source) {
        sourceText = source;
        sourceLines = source.split("\n");
        emitter.reset();
        nextCommentIdx = 0;
        inClassContext = False;

        # Parse the source
        AstParser parser();
        *AstTree tree = parser.parseString(source);

        if (!exists tree || parser.getErrorCount() > 0) {
            string msg = "parse errors in source code";
            *list<hash<auto>> diags = parser.getDiagnostics();
            if (exists diags && diags.size() > 0) {
                msg += ": " + diags[0].message;
            }
            throw "FORMAT-ERROR", msg;
        }

        # Get CST nodes and translate to old AST format
        *list<hash<auto>> rawNodes = tree.getNodesInfo();
        *list<hash<auto>> nodes = exists rawNodes ? CSTTranslator::translate(rawNodes, source) : NOTHING;
        *list<hash<auto>> comments = tree.getComments();

        # Extract parse directives from source (they don't appear in AST)
        list<hash<auto>> parseDirectives = extractParseDirectives(comments, nodes);

        # Set up comment attacher
        if (exists comments && comments.size() > 0) {
            commentAttacher = new CommentAttacher(comments, nodes ?? (), source);
            allComments = commentAttacher.getAllComments();
        } else {
            allComments = ();
        }

        if (!exists nodes || nodes.size() == 0) {
            # No AST nodes - just emit parse directives and comments
            for (int i = 0; i < parseDirectives.size(); ++i) {
                emitCommentsBeforeLine1(parseDirectives[i].line);
                emitter.emitLineAtColumn0(parseDirectives[i].text);
            }
            emitRemainingComments();
            return emitter.getOutput();
        }

        # Determine which source lines contain AST nodes (real code)
        # Build a set of source lines covered by AST nodes
        # Parse directives that are NOT on lines covered by AST nodes should be emitted
        # at their source position relative to AST nodes.

        # Since AST locations can be unreliable for parse directives,
        # use a simpler strategy: emit all items (parse directives, comments, AST nodes)
        # in source line order.

        # Create a merged list of things to emit in order
        list<hash<auto>> emitItems = ();

        # Add parse directives
        for (int i = 0; i < parseDirectives.size(); ++i) {
            emitItems += {
                "type": "parse_directive",
                "line1": parseDirectives[i].line,
                "data": parseDirectives[i],
            };
        }

        # Add AST nodes - use their actual source line position
        for (int i = 0; i < nodes.size(); ++i) {
            # Find the actual source line where the code starts
            # Since AST locations may include preceding parse directives, find the real start
            int nodeLine1 = findRealStartLine1(nodes[i]);
            emitItems += {
                "type": "node",
                "line1": nodeLine1,
                "data": nodes[i],
            };
        }

        # Sort by source line
        emitItems = sort(emitItems, int sub (hash<auto> a, hash<auto> b) {
            return a.line1 <=> b.line1;
        });

        # Emit all items in order
        bool firstNode = True;
        for (int i = 0; i < emitItems.size(); ++i) {
            hash<auto> item = emitItems[i];

            # Emit comments before this item
            emitCommentsBeforeLine1(item.line1);

            if (item.type == "parse_directive") {
                emitter.emitLineAtColumn0(item.data.text);
            } else {
                # Blank line between top-level declarations
                if (!firstNode && item.data.nodetype == ANT_Declaration) {
                    emitter.blankLine();
                }
                emitNode(item.data);
                firstNode = False;
            }
        }

        # Emit any remaining comments
        emitRemainingComments();

        return emitter.getOutput();
    }

    #! Find the real source line (1-indexed) where an AST node's code starts
    /** AST node locations may include preceding parse directives in their range.
        This method finds where the actual code keyword starts by using the node's
        name location if available, or by scanning forward past non-code lines.
    */
    private int findRealStartLine1(hash<auto> node) {
        # If the node has a name with a location, use the name's line as the anchor
        # since name locations point to the actual keyword/identifier, not preceding
        # parse directives. Back up from the name line to find the first code line
        # of the declaration (e.g., modifiers before the class keyword).
        if (exists node.name && exists node.name.loc) {
            int nameLine0 = node.name.loc.start_line;
            # Scan backward from the name to find the declaration's start
            # looking for modifier keywords on preceding lines
            int lineIdx = nameLine0;
            while (lineIdx > node.loc.start_line) {
                string prevLine = trim(sourceLines[lineIdx - 1]);
                # Check if prev line is a parse directive, comment, or blank
                # Matches: empty, # line comments, % directives, /* block comment starts,
                # */ block comment ends, * block comment continuation lines
                if (prevLine == "" || prevLine =~ /^[#%]/ || prevLine =~ /^\/?\*/ || prevLine =~ /^\*\//) {
                    break;
                }
                # Could be a modifier line - check if it looks like code
                --lineIdx;
            }
            return lineIdx + 1;  # convert to 1-indexed
        }

        # Fallback: scan forward from the node's start past parse directives
        int lineIdx = node.loc.start_line;
        if (lineIdx < 0) {
            lineIdx = 0;
        }

        while (lineIdx < sourceLines.size()) {
            string line = trim(sourceLines[lineIdx]);
            if (line =~ /^%/) {
                ++lineIdx;
            } else {
                break;
            }
        }

        return lineIdx + 1;  # convert to 1-indexed
    }

    #! Extract parse directives from source text
    /** Parse directives start at column 0 with % and are not part of the AST.
        Lines inside block comments (e.g., @@code examples) are excluded.
        @param comments comment list from getComments() with 1-indexed loc
        @param nodes node list from getNodesInfo() for detecting directives inside node ranges
        @return list of hashes with keys "line" (1-indexed), "text" (directive text)
    */
    private list<hash<auto>> extractParseDirectives(*list<hash<auto>> comments, *list<hash<auto>> nodes) {
        # Build set of lines inside block comments (1-indexed)
        hash<string, bool> excludedLines();
        if (exists comments) {
            foreach hash<auto> c in (comments) {
                int kind = c.kind;
                if (kind == ACK_Block || kind == ACK_DocBlock) {
                    int fl = c.loc.firstLine;
                    int ll = c.loc.lastLine;
                    for (int ln = fl; ln <= ll; ++ln) {
                        excludedLines{string(ln)} = True;
                    }
                }
            }
        }

        # Build set of lines strictly INSIDE AST node source ranges (1-indexed)
        # Parse directives inside nodes are included in source-extracted text
        # and should not be emitted separately
        if (exists nodes) {
            collectNodeInteriorLines(nodes, \excludedLines);
        }

        list<hash<auto>> result = ();
        for (int i = 0; i < sourceLines.size(); ++i) {
            string line = sourceLines[i];
            if (line =~ /^%/) {
                int line1 = i + 1;
                # Skip lines inside block comments or AST node interiors
                if (exists excludedLines{string(line1)}) {
                    continue;
                }
                result += {"line": line1, "text": trim(line)};
            }
        }
        return result;
    }

    #! Collect 1-indexed line numbers that are strictly inside top-level AST node ranges
    /** Only marks interior lines for top-level nodes (which contain expressions
        emitted via source extraction). Does not recurse into nodes with child
        declarations (class/namespace bodies) since those have separate AST children.
    */
    private collectNodeInteriorLines(list<hash<auto>> nodes, reference<hash<string, bool>> lines) {
        foreach hash<auto> node in (nodes) {
            if (!exists node || !exists node.loc) {
                continue;
            }
            # Convert 0-indexed to 1-indexed
            int startLine1 = node.loc.start_line + 1;
            int endLine1 = node.loc.end_line + 1;
            # Mark interior lines (between start+1 and end)
            # Only for nodes that don't have separately-emitted child declarations
            for (int ln = startLine1 + 1; ln <= endLine1; ++ln) {
                lines{string(ln)} = True;
            }
        }
    }

    #! Emit comments that occur before the given 1-indexed line
    private emitCommentsBeforeLine1(int line1) {
        while (nextCommentIdx < allComments.size()) {
            hash<CommentInfo> c = allComments[nextCommentIdx];
            # Comments use 1-indexed lines from getComments()
            if (c.loc.firstLine < line1) {
                emitComment(c);
                ++nextCommentIdx;
            } else {
                break;
            }
        }
    }

    #! Emit any remaining comments after all nodes
    private emitRemainingComments() {
        while (nextCommentIdx < allComments.size()) {
            emitComment(allComments[nextCommentIdx]);
            ++nextCommentIdx;
        }
    }

    #! Skip (advance past) comments that fall within the given 1-indexed line range
    /** Called after emitting a node whose source text was extracted verbatim,
        to prevent comments embedded in that source text from being double-emitted.
    */
    private skipCommentsInRange(int startLine1, int endLine1) {
        while (nextCommentIdx < allComments.size()) {
            hash<CommentInfo> c = allComments[nextCommentIdx];
            if (c.loc.firstLine >= startLine1 && c.loc.lastLine <= endLine1) {
                ++nextCommentIdx;
            } else {
                break;
            }
        }
    }

    #! Emit a single comment
    private emitComment(hash<CommentInfo> c) {
        string text = c.text;
        if (c.kind == ACK_Line || c.kind == ACK_DocLine) {
            # Line comments - ensure space after # or #!
            if (text =~ /^#[^!\s]/ && text != "#") {
                text = "# " + text.substr(1);
            } else if (text =~ /^#![^\s]/) {
                text = "#! " + text.substr(2);
            }
            emitter.emitLine(text);
        } else {
            # Block comments - use emitMultiLine for multi-line comments to
            # normalize indentation (strip base indent, re-indent at current level)
            if (text =~ /\n/) {
                emitter.emitMultiLine(text);
                emitter.newline();
            } else {
                emitter.emitLine(text);
            }
        }
    }

    #! Emit an AST node
    private emitNode(hash<auto> node) {
        switch (node.nodetype) {
            case ANT_Declaration:
                emitDeclaration(node);
                break;
            case ANT_Expression:
                emitExpression(node);
                emitter.emitLine(";");
                break;
            case ANT_ParseOption:
                emitParseOption(node);
                break;
            case ANT_Statement:
                emitStatement(node);
                break;
            case ANT_Name:
                emitter.emitLine(node.name ?? "");
                break;
        }
        # Skip comments that fall within this node's source range, since they
        # may have been included in source-extracted text (avoiding double-emission)
        if (exists node.loc) {
            int endLine1 = node.loc.end_line + 1;
            skipCommentsInRange(node.loc.start_line + 1, endLine1);
        }
    }

    #! Emit a parse option directive at column 0
    private emitParseOption(hash<auto> node) {
        # Extract the directive text from source
        string text = extractSourceText(node.loc);
        # Ensure it's at column 0
        text = trim(text);
        emitter.emitLineAtColumn0(text);
    }

    #! Emit a declaration
    private emitDeclaration(hash<auto> node) {
        switch (node.kind) {
            case ADK_Class:
                emitClassDecl(node);
                break;
            case ADK_Function:
                emitFunctionDecl(node);
                break;
            case ADK_Namespace:
                emitNamespaceDecl(node);
                break;
            case ADK_Constant:
                emitConstantDecl(node);
                break;
            case ADK_Variable:
                emitVariableDecl(node);
                break;
            case ADK_Hash:
                emitHashDecl(node);
                break;
            case ADK_Typedef:
                emitTypedefDecl(node);
                break;
            case ADK_MemberGroup:
                emitMemberGroupDecl(node);
                break;
            case ADK_Enum:
                emitEnumDecl(node);
                break;
            case ADK_Module:
                emitModuleDecl(node);
                break;
            case ADK_Closure:
                emitClosureDecl(node);
                break;
            default:
                # Fallback: extract from source
                emitter.emitLine(extractSourceText(node.loc));
                break;
        }
    }

    #! Get the effective line length including current indentation
    private int currentIndentWidth() {
        return emitter.getIndentLevel() * 4;
    }

    #! Check if an operator name represents a compound assignment (+=, -=, etc.)
    private bool isCompoundAssignOp(string op) {
        switch (op) {
            case "AOK_PlusEquals":
            case "AOK_MinusEquals":
            case "AOK_MultiplyEquals":
            case "AOK_DivideEquals":
            case "AOK_ModuloEquals":
            case "AOK_AndEquals":
            case "AOK_OrEquals":
            case "AOK_XorEquals":
            case "AOK_ShiftLeftEquals":
            case "AOK_ShiftRightEquals":
            case "AOK_WS_PLUS_EQUALS":
            case "AOK_WS_MINUS_EQUALS":
            case "AOK_WS_AND_EQUALS":
            case "AOK_WS_OR_EQUALS":
            case "AOK_WS_MODULO_EQUALS":
            case "AOK_WS_MULTIPLY_EQUALS":
            case "AOK_WS_DIVIDE_EQUALS":
            case "AOK_WS_XOR_EQUALS":
            case "AOK_WS_SHIFT_RIGHT_EQUALS":
            case "AOK_WS_SHIFT_LEFT_EQUALS":
                return True;
        }
        return False;
    }

    #! Flatten a left-associative binary chain of the same operator
    /** Returns list of hashes: first element has "operand" only;
        subsequent elements have "op" (string) and "operand" (expression string).
        Mixed operators stop at the boundary.
    */
    private list<hash<auto>> flattenBinaryChain(hash<auto> node) {
        string myOp = node.op ?? "AOK_Plus";
        list<hash<auto>> result = ();

        # Recurse into left child if it's the same binary operator
        if (exists node.left && exists node.left.kind && node.left.kind == AEK_Binary
                && exists node.left.op && node.left.op == myOp) {
            result = flattenBinaryChain(node.left);
        } else {
            result += {"operand": emitExpressionStr(node.left)};
        }

        # Add right operand with operator
        string opStr = resolveOperator(myOp);
        string rightStr = emitExpressionStr(node.right);
        # Parenthesize right child if lower or equal precedence (right-assoc protection)
        int myPrec = getOperatorPrecedence(myOp);
        if (exists node.right.kind && node.right.kind == AEK_Binary && exists node.right.op) {
            if (getOperatorPrecedence(node.right.op) <= myPrec) {
                rightStr = "(" + rightStr + ")";
            }
        }
        if (exists node.right.kind && node.right.kind == AEK_Ternary) {
            rightStr = "(" + rightStr + ")";
        }
        result += {"op": opStr, "operand": rightStr};

        return result;
    }

    #! Emit expression with line-length-aware wrapping
    /** Unified helper for statement/declaration emitters.
        1. Builds single-line: prefix + emitExpressionStr(exprNode) + suffix
        2. Checks if it fits within MAX_LINE_LENGTH
        3. If fits and no newlines → emitLine
        4. If too long and wrappable (binary/ternary/string concat) → wrap
        5. Fallback → emitMultiLineLine
    */
    private emitExprLine(string prefix, *hash<auto> exprNode, string suffix) {
        string exprStr = emitExpressionStr(exprNode);
        string fullLine = prefix + exprStr + suffix;
        int lineLen = currentIndentWidth() + fullLine.size();

        if (lineLen <= MAX_LINE_LENGTH && fullLine !~ /\n/) {
            emitter.emitLine(fullLine);
            return;
        }

        # Try wrapping based on expression kind
        if (exists exprNode && exists exprNode.kind) {
            if (exprNode.kind == AEK_Binary && !isCompoundAssignOp(exprNode.op ?? "")) {
                emitWrappedBinaryLine(prefix, exprNode, suffix);
                return;
            }
            # Compound assignment (AEK_Binary with +=, -=, etc.) — wrap the RHS
            if (exprNode.kind == AEK_Binary && isCompoundAssignOp(exprNode.op ?? "")
                    && exists exprNode.right) {
                string opStr = resolveOperator(exprNode.op ?? "");
                string assignPrefix = prefix + emitExpressionStr(exprNode.left) + " " + opStr + " ";
                if (exists exprNode.right.kind && exprNode.right.kind == AEK_Binary
                        && !isCompoundAssignOp(exprNode.right.op ?? "")) {
                    emitWrappedBinaryLine(assignPrefix, exprNode.right, suffix);
                    return;
                }
                if (exists exprNode.right.kind && exprNode.right.kind == AEK_Ternary) {
                    emitWrappedTernaryLine(assignPrefix, exprNode.right, suffix);
                    return;
                }
                if (exists exprNode.right.kind && exprNode.right.kind == AEK_StringConcat) {
                    emitWrappedStringConcatLine(assignPrefix, exprNode.right, suffix);
                    return;
                }
            }
            if (exprNode.kind == AEK_Ternary) {
                emitWrappedTernaryLine(prefix, exprNode, suffix);
                return;
            }
            if (exprNode.kind == AEK_StringConcat) {
                emitWrappedStringConcatLine(prefix, exprNode, suffix);
                return;
            }
            if (exprNode.kind == AEK_Assignment && exists exprNode.right
                    && exists exprNode.right.kind && exprNode.right.kind == AEK_Binary) {
                # Assignment with binary RHS: wrap the RHS
                string assignPrefix = prefix + emitExpressionStr(exprNode.left) + " = ";
                emitWrappedBinaryLine(assignPrefix, exprNode.right, suffix);
                return;
            }
            if (exprNode.kind == AEK_Assignment && exists exprNode.right
                    && exists exprNode.right.kind && exprNode.right.kind == AEK_Ternary) {
                string assignPrefix = prefix + emitExpressionStr(exprNode.left) + " = ";
                emitWrappedTernaryLine(assignPrefix, exprNode.right, suffix);
                return;
            }
            if (exprNode.kind == AEK_Assignment && exists exprNode.right
                    && exists exprNode.right.kind && exprNode.right.kind == AEK_StringConcat) {
                string assignPrefix = prefix + emitExpressionStr(exprNode.left) + " = ";
                emitWrappedStringConcatLine(assignPrefix, exprNode.right, suffix);
                return;
            }
        }

        # Fallback: emit as multi-line (for hash/closure/etc that contain newlines)
        if (fullLine =~ /\n/) {
            emitter.emitMultiLineLine(fullLine);
        } else {
            emitter.emitLine(fullLine);
        }
    }

    #! Emit a wrapped binary expression line with operator-leading continuation
    private emitWrappedBinaryLine(string prefix, hash<auto> node, string suffix) {
        list<hash<auto>> chain = flattenBinaryChain(node);
        string baseIndent = strmul("    ", emitter.getIndentLevel());
        string contIndent = baseIndent + "    ";  # single-indent (+4) continuation

        # Emit first operand with prefix
        emitter.emitLine(prefix + chain[0].operand);

        # Emit continuation operands
        for (int i = 1; i < chain.size(); ++i) {
            string line = chain[i].op + " " + chain[i].operand;
            if (i == chain.size() - 1) {
                line += suffix;
            }
            emitter.emitLineAtColumn0(contIndent + line);
        }

        # Skip comments within the expression's source range
        if (exists node.loc) {
            skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
        }
    }

    #! Emit a wrapped ternary expression line
    private emitWrappedTernaryLine(string prefix, hash<auto> node, string suffix) {
        string condStr = emitExpressionStr(node.condition);
        string trueStr = emitExpressionStr(node.exprTrue);
        string falseStr = emitExpressionStr(node.exprFalse);
        string baseIndent = strmul("    ", emitter.getIndentLevel());
        string contIndent = baseIndent + "    ";  # single-indent (+4) continuation

        emitter.emitLine(prefix + condStr);
        emitter.emitLineAtColumn0(contIndent + "? " + trueStr);
        emitter.emitLineAtColumn0(contIndent + ": " + falseStr + suffix);

        if (exists node.loc) {
            skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
        }
    }

    #! Emit a wrapped string concatenation line (implicit adjacent strings)
    private emitWrappedStringConcatLine(string prefix, hash<auto> node, string suffix) {
        if (!exists node.parts || node.parts.size() == 0) {
            emitter.emitLine(prefix + suffix);
            return;
        }
        string baseIndent = strmul("    ", emitter.getIndentLevel());
        string contIndent = baseIndent + "    ";  # single-indent (+4) continuation

        # Emit first part with prefix
        emitter.emitLine(prefix + emitExpressionStr(node.parts[0]));

        # Emit remaining parts at continuation indent
        for (int i = 1; i < node.parts.size(); ++i) {
            string line = emitExpressionStr(node.parts[i]);
            if (i == node.parts.size() - 1) {
                line += suffix;
            }
            emitter.emitLineAtColumn0(contIndent + line);
        }

        if (exists node.loc) {
            skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
        }
    }

    #! Emit a declaration header that may span multiple lines
    /** If the full line (indent + header + suffix) exceeds MAX_LINE_LENGTH, the
        parameter list (or inherits list) is broken across lines with each subsequent
        line double-indented (8 spaces) relative to the declaration start to avoid
        confusion with the body.

        @param prefix text before the opening paren (e.g., "int sub foo")
        @param params comma-separated items to potentially wrap (e.g., "int a, string b")
        @param openDelim opening delimiter ("(" or " inherits ")
        @param closeDelim closing delimiter (")")
        @param suffix text after the close delimiter (e.g., " {")
    */
    private emitWrappedHeader(string prefix, string params, string openDelim, string closeDelim,
            string suffix) {
        string oneLine = prefix + openDelim + params + closeDelim + suffix;
        int lineLen = currentIndentWidth() + oneLine.size();

        if (lineLen <= MAX_LINE_LENGTH || params == "") {
            # Fits on one line
            emitter.emitLine(oneLine);
            return;
        }

        # Break across multiple lines with double-indent continuation
        string baseIndent = strmul("    ", emitter.getIndentLevel());
        string contIndent = baseIndent + CONTINUATION_INDENT;

        # Emit the prefix + opening delimiter on the first line
        emitter.emitLine(prefix + openDelim);

        # Split params by ", " and emit each on its own continuation line
        list<string> items = params.split(", ");
        for (int i = 0; i < items.size(); ++i) {
            string item = items[i];
            if (i < items.size() - 1) {
                item += ",";
            }
            # Handle items that span multiple lines (e.g., complex type names)
            if (item =~ /\n/) {
                list<string> sublines = item.split("\n");
                for (int j = 0; j < sublines.size(); ++j) {
                    string subline = trim(sublines[j]);
                    if (subline != "") {
                        emitter.emitLineAtColumn0(contIndent + subline);
                    }
                }
            } else {
                emitter.emitLineAtColumn0(contIndent + item);
            }
        }

        # Emit the close delimiter + suffix
        emitter.emitLineAtColumn0(baseIndent + closeDelim + suffix);
    }

    #! Emit modifier string
    private string getModifierStr(*string modStr) {
        if (!exists modStr || modStr == "") {
            return "";
        }
        return modStr;
    }

    #! Emit class declaration
    /** Multi-line class declarations use double-indent (8 spaces) for
        continuation lines (e.g., long inherits lists).
    */
    private emitClassDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        # Build class prefix (everything before inherits)
        string prefix = "";
        if (mods != "") {
            prefix += mods + " ";
        }
        prefix += "class " + name;

        # Build inherits items list
        list<string> inhItems = ();
        if (exists node.inherits && node.inherits.size() > 0) {
            for (int i = 0; i < node.inherits.size(); ++i) {
                string inhMods = getModifierStr(node.inherits[i].modifiers);
                string item = "";
                if (inhMods != "") {
                    item += inhMods + " ";
                }
                item += node.inherits[i].name.name;
                inhItems += item;
            }
        }

        # Check if forward declaration (no body)
        if (!exists node.declarations || node.declarations.size() == 0) {
            # Check source to see if it's really a forward decl (ends with ;)
            string srcText = extractSourceText(node.loc);
            if (srcText =~ /;\s*$/) {
                if (inhItems.size() > 0) {
                    emitter.emitLine(prefix + " inherits " + inhItems.join(", ") + ";");
                } else {
                    emitter.emitLine(prefix + ";");
                }
                return;
            }
        }

        # Emit class header with potential wrapping
        if (inhItems.size() > 0) {
            emitWrappedClassHeader(prefix, inhItems, " {");
        } else {
            emitter.emitLine(prefix + " {");
        }
        emitter.indent();

        # Emit class body
        bool savedClassContext = inClassContext;
        inClassContext = True;
        if (exists node.declarations) {
            for (int i = 0; i < node.declarations.size(); ++i) {
                hash<auto> decl = node.declarations[i];

                # Blank line between class body declarations
                if (i > 0) {
                    emitter.newline();
                }

                # Emit comments before this declaration
                # Use findRealStartLine1() to get the actual code line past doc comments,
                # since AST loc.start_line may extend back to include doc comments or
                # preceding whitespace, causing comments to be placed inside the body.
                if (exists decl.loc) {
                    emitCommentsBeforeLine1(findRealStartLine1(decl));
                }

                emitDeclaration(decl);
            }
        }
        inClassContext = savedClassContext;

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit a class header with inherits that may need line wrapping
    /** @param prefix class prefix (e.g., "public class Name")
        @param inhItems list of superclass names/items
        @param suffix text after the inherits list (e.g., " {")
    */
    private emitWrappedClassHeader(string prefix, list<string> inhItems, string suffix) {
        string inheritsStr = inhItems.join(", ");
        string fullHeader = prefix + " inherits " + inheritsStr + suffix;
        int lineLen = currentIndentWidth() + fullHeader.size();

        if (lineLen <= MAX_LINE_LENGTH) {
            emitter.emitLine(fullHeader);
            return;
        }

        # Wrap: put inherits on continuation lines
        string baseIndent = strmul("    ", emitter.getIndentLevel());
        string contIndent = baseIndent + CONTINUATION_INDENT;

        # Try putting entire "inherits ..." on one continuation line
        string contLine = "inherits " + inheritsStr + suffix;
        if ((contIndent + contLine).size() <= MAX_LINE_LENGTH) {
            emitter.emitLine(prefix);
            emitter.emitLineAtColumn0(contIndent + contLine);
            return;
        }

        # Break inherits items across multiple continuation lines
        emitter.emitLine(prefix);
        for (int i = 0; i < inhItems.size(); ++i) {
            string line;
            if (i == 0) {
                line = "inherits " + inhItems[i];
            } else {
                line = inhItems[i];
            }
            if (i < inhItems.size() - 1) {
                line += ",";
            } else {
                line += suffix;
            }
            emitter.emitLineAtColumn0(contIndent + line);
        }
    }

    #! Emit function/method declaration
    /** Multi-line function declarations use double-indent (8 spaces) for
        continuation lines to distinguish them from the function body.
    */
    private emitFunctionDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string prefix = "";
        if (mods != "") {
            prefix += mods + " ";
        }

        # Return type
        if (exists node.returnType) {
            prefix += emitExpressionStr(node.returnType) + " ";
        }

        # For standalone functions (not class methods), emit `sub` keyword
        if (!inClassContext) {
            prefix += "sub ";
        }
        prefix += name;

        # Parameters
        string params = "";
        if (exists node.params) {
            params = emitExpressionStrInner(node.params);
        }

        # Constructor initializers
        string initStr = "";
        if (exists node.inits) {
            initStr = " : " + emitExpressionStr(node.inits);
        }

        # Check for abstract (no body)
        if (!exists node.body) {
            emitWrappedHeader(prefix, params, "(", ")", initStr + ";");
            return;
        }

        # Body
        emitWrappedHeader(prefix, params, "(", ")", initStr + " {");
        emitter.indent();
        emitStatementBody(node.body);
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit namespace declaration
    /** Namespace body is left-justified (no indentation) to save horizontal space.
    */
    private emitNamespaceDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string header = "";
        if (mods != "") {
            header += mods + " ";
        }
        header += "namespace " + name;

        if (!exists node.declarations || node.declarations.size() == 0) {
            emitter.emitLine(header + " {}");
            return;
        }

        emitter.emitLine(header + " {");
        # No indent — namespace body is left-justified
        emitter.newline();

        for (int i = 0; i < node.declarations.size(); ++i) {
            hash<auto> decl = node.declarations[i];
            if (i > 0) {
                emitter.newline();
            }
            if (exists decl.loc) {
                emitCommentsBeforeLine1(findRealStartLine1(decl));
            }
            emitDeclaration(decl);
        }

        emitter.newline();
        emitter.emitLine("}");
    }

    #! Emit constant declaration
    private emitConstantDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string prefix = "";
        if (mods != "") {
            prefix += mods + " ";
        }
        prefix += "const " + name + " = ";

        if (exists node.value) {
            emitExprLine(prefix, node.value, ";");
        } else {
            emitter.emitLine(prefix + ";");
        }
        # Skip comments within the constant's source range (they may be included
        # in source-extracted expression text, e.g., hash literals with inline comments)
        if (exists node.loc) {
            skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
        }
    }

    #! Emit variable declaration
    private emitVariableDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string line = "";
        if (mods != "") {
            line += mods + " ";
        }
        if (exists node.typeName && exists node.typeName.name && node.typeName.name != "") {
            line += node.typeName.name + " ";
        }
        line += name;
        emitter.emitLine(line + ";");
    }

    #! Emit hashdecl declaration
    private emitHashDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string header = "";
        if (mods != "") {
            header += mods + " ";
        }
        header += "hashdecl " + name;

        # Check for inherits
        string srcText = extractSourceText(node.loc);
        *string inheritsStr;
        if (srcText =~ /inherits\s+(\S+)/) {
            inheritsStr = ($1);
        }

        if (exists inheritsStr) {
            header += " inherits " + inheritsStr;
        }

        emitter.emitLine(header + " {");
        emitter.indent();

        if (exists node.declarations) {
            for (int i = 0; i < node.declarations.size(); ++i) {
                hash<auto> decl = node.declarations[i];
                if (exists decl.loc) {
                    emitCommentsBeforeLine1(findRealStartLine1(decl));
                }
                emitHashMemberDecl(decl);
            }
        }

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit hash member declaration
    private emitHashMemberDecl(hash<auto> node) {
        string typeName = "";
        if (exists node.typeName && exists node.typeName.name) {
            typeName = node.typeName.name;
        }
        string name = node.name.name;

        string prefix = "";
        if (typeName != "") {
            prefix += typeName + " ";
        }
        prefix += name;
        if (exists node.init) {
            emitExprLine(prefix + " = ", node.init, ";");
            if (exists node.loc) {
                skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
            }
        } else {
            emitter.emitLine(prefix + ";");
        }
    }

    #! Emit typedef declaration
    private emitTypedefDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;
        string typeName = "";
        if (exists node.typeName && exists node.typeName.name) {
            typeName = node.typeName.name;
        }

        string line = "";
        if (mods != "") {
            line += mods + " ";
        }
        line += "typedef " + name + " = " + typeName;
        emitter.emitLine(line + ";");
    }

    #! Emit member group declaration
    private emitMemberGroupDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);

        emitter.emitLine(mods + " {");
        emitter.indent();

        if (exists node.members) {
            for (int i = 0; i < node.members.size(); ++i) {
                hash<auto> member = node.members[i];
                if (exists member.loc) {
                    emitCommentsBeforeLine1(findRealStartLine1(member));
                }
                # Member group members are expressions (usually DeclExpr)
                emitExprLine("", member, ";");
                # Skip comments within this member's source range (they are included
                # in the source-extracted expression text)
                if (exists member.loc) {
                    skipCommentsInRange(member.loc.start_line + 1, member.loc.end_line + 1);
                }
            }
        }

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit enum declaration
    private emitEnumDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string name = node.name.name;

        string header = "";
        if (mods != "") {
            header += mods + " ";
        }
        header += "enum " + name;

        emitter.emitLine(header + " {");
        emitter.indent();

        if (exists node.declarations) {
            for (int i = 0; i < node.declarations.size(); ++i) {
                hash<auto> decl = node.declarations[i];
                if (exists decl.loc) {
                    emitCommentsBeforeLine1(findRealStartLine1(decl));
                }
                string memberName = decl.name.name;
                if (exists decl.value) {
                    emitExprLine(memberName + " = ", decl.value, ",");
                } else {
                    emitter.emitLine(memberName + ",");
                }
                if (exists decl.loc) {
                    skipCommentsInRange(decl.loc.start_line + 1,
                        decl.loc.end_line + 1);
                }
            }
        }

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit module declaration
    private emitModuleDecl(hash<auto> node) {
        string name = node.name.name;

        emitter.emitLine("module " + name + " {");
        emitter.indent();

        if (exists node.attributes) {
            for (int i = 0; i < node.attributes.size(); ++i) {
                hash<auto> attr = node.attributes[i];
                emitExprLine(attr.key + " = ", attr.value, ";");
                # Skip comments inside source-extracted attribute values (e.g., init closures)
                if (exists attr.value && exists attr.value.loc) {
                    skipCommentsInRange(attr.value.loc.start_line + 1, attr.value.loc.end_line + 1);
                }
            }
        }

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit closure declaration
    private emitClosureDecl(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);

        string prefix = "";
        if (mods != "") {
            prefix += mods + " ";
        }

        if (exists node.returnType) {
            prefix += emitExpressionStr(node.returnType) + " ";
        }

        prefix += "sub";

        string params = "";
        if (exists node.params) {
            params = emitExpressionStrInner(node.params);
        }

        if (!exists node.body) {
            emitter.emit(prefix + "(" + params + ")");
            return;
        }

        emitWrappedHeader(prefix, params, "(", ")", " {");
        emitter.indent();
        emitStatementBody(node.body);
        emitter.dedent();
        emitter.emit("}");
    }

    #! Emit a statement
    private emitStatement(hash<auto> node) {
        switch (node.kind) {
            case ASK_Block:
                emitBlockStmt(node);
                break;
            case ASK_If:
                emitIfStmt(node);
                break;
            case ASK_While:
                emitWhileStmt(node);
                break;
            case ASK_DoWhile:
                emitDoWhileStmt(node);
                break;
            case ASK_For:
                emitForStmt(node);
                break;
            case ASK_Foreach:
                emitForeachStmt(node);
                break;
            case ASK_Switch:
                emitSwitchStmt(node);
                break;
            case ASK_Try:
                emitTryStmt(node);
                break;
            case ASK_Return:
                emitReturnStmt(node);
                break;
            case ASK_Throw:
                emitThrowStmt(node);
                break;
            case ASK_Break:
                emitter.emitLine("break;");
                break;
            case ASK_Continue:
                emitter.emitLine("continue;");
                break;
            case ASK_Rethrow:
                emitter.emitLine("rethrow;");
                break;
            case ASK_ThreadExit:
                emitter.emitLine("thread_exit;");
                break;
            case ASK_Expression:
                emitExpressionStmt(node);
                break;
            case ASK_Call:
                emitCallStmt(node);
                break;
            case ASK_OnBlockExit:
                emitOnBlockExitStmt(node);
                break;
            case ASK_Assert:
                emitAssertStmt(node);
                break;
            case ASK_Debug:
                emitDebugStmt(node);
                break;
            default:
                # Fallback: extract from source
                string srcText = extractSourceText(node.loc);
                # Don't add ; if source already ends with ; or } (block statements)
                if (srcText =~ /[;}]\s*$/) {
                    if (srcText =~ /\n/) {
                        emitter.emitMultiLineLine(srcText);
                    } else {
                        emitter.emitLine(srcText);
                    }
                } else {
                    string text = srcText + ";";
                    if (text =~ /\n/) {
                        emitter.emitMultiLineLine(text);
                    } else {
                        emitter.emitLine(text);
                    }
                }
                break;
        }
    }

    #! Emit block statement contents (inside braces)
    private emitStatementBody(*hash<auto> body) {
        if (!exists body) {
            return;
        }
        if (body.kind == ASK_Block) {
            # Block statement - emit individual statements
            if (exists body.statements) {
                for (int i = 0; i < body.statements.size(); ++i) {
                    hash<auto> stmt = body.statements[i];
                    if (exists stmt.loc) {
                        emitCommentsBeforeLine1(findRealStartLine1(stmt));
                    }
                    # Use emitNode to dispatch on nodetype — blocks can contain declarations
                    # (e.g. global_variable_declaration) that have kind values colliding
                    # with statement kinds (ADK_Variable=10 == ASK_If=10)
                    emitNode(stmt);
                }
            }
        } else {
            # Single statement
            emitNode(body);
        }
    }

    #! Emit block statement
    private emitBlockStmt(hash<auto> node) {
        if (exists node.statements) {
            for (int i = 0; i < node.statements.size(); ++i) {
                hash<auto> stmt = node.statements[i];
                if (exists stmt.loc) {
                    emitCommentsBeforeLine1(findRealStartLine1(stmt));
                }
                emitNode(stmt);
            }
        }
    }

    #! Emit if statement
    private emitIfStmt(hash<auto> node) {
        emitExprLine("if (", node.condition, ") {");
        emitter.indent();
        emitStatementBody(node.stmtThen);
        emitter.dedent();

        if (exists node.stmtElse) {
            # Check if else-if: the else branch may be the If directly
            # or a Block wrapping a single If statement
            *hash<auto> elseIf;
            if (node.stmtElse.nodetype == ANT_Statement && node.stmtElse.kind == ASK_If) {
                elseIf = node.stmtElse;
            } else if (node.stmtElse.nodetype == ANT_Statement && node.stmtElse.kind == ASK_Block
                    && exists node.stmtElse.statements && node.stmtElse.statements.size() == 1
                    && node.stmtElse.statements[0].kind == ASK_If) {
                elseIf = node.stmtElse.statements[0];
            }
            if (exists elseIf) {
                emitter.emit("} else ");
                emitIfStmt(elseIf);
                return;
            }
            emitter.emitLine("} else {");
            emitter.indent();
            emitStatementBody(node.stmtElse);
            emitter.dedent();
        }
        emitter.emitLine("}");
    }

    #! Emit while statement
    private emitWhileStmt(hash<auto> node) {
        emitExprLine("while (", node.condition, ") {");
        emitter.indent();
        emitStatementBody(node.statement);
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit do-while statement
    private emitDoWhileStmt(hash<auto> node) {
        emitter.emitLine("do {");
        emitter.indent();
        emitStatementBody(node.statement);
        emitter.dedent();
        emitExprLine("} while (", node.condition, ");");
    }

    #! Emit for statement
    private emitForStmt(hash<auto> node) {
        string init = exists node.init ? emitExpressionStr(node.init) : "";
        string cond = exists node.condition ? emitExpressionStr(node.condition) : "";
        string iter = exists node.iteration ? emitExpressionStr(node.iteration) : "";
        string forText = "for (" + init + "; " + cond + "; " + iter + ") {";
        if (forText =~ /\n/) {
            emitter.emitMultiLineLine(forText);
        } else {
            emitter.emitLine(forText);
        }
        emitter.indent();
        emitStatementBody(node.statement);
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit foreach statement
    private emitForeachStmt(hash<auto> node) {
        string val = emitExpressionStr(node.value);
        emitExprLine("foreach " + val + " in (", node.source, ") {");
        emitter.indent();
        emitStatementBody(node.statement);
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit switch statement
    private emitSwitchStmt(hash<auto> node) {
        emitExprLine("switch (", node.variable, ") {");
        emitter.indent();

        if (exists node.body) {
            emitExpression(node.body);
        }

        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit try statement
    private emitTryStmt(hash<auto> node) {
        emitter.emitLine("try {");
        emitter.indent();
        emitStatementBody(node.tryStmt);
        emitter.dedent();

        string catchVar = "";
        if (exists node.catchVar) {
            catchVar = emitExpressionStr(node.catchVar);
        }
        emitter.emitLine("} catch (" + catchVar + ") {");
        emitter.indent();
        if (exists node.catchStmt) {
            emitStatementBody(node.catchStmt);
        }
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit return statement
    private emitReturnStmt(hash<auto> node) {
        if (exists node.retval) {
            emitExprLine("return ", node.retval, ";");
        } else {
            emitter.emitLine("return;");
        }
    }

    #! Emit throw statement
    private emitThrowStmt(hash<auto> node) {
        if (exists node.expression) {
            # throw args should not be wrapped in parens - use emitExpressionStrInner
            string exprStr = emitExpressionStrInner(node.expression);
            string fullLine = "throw " + exprStr + ";";
            if (fullLine =~ /\n/) {
                emitter.emitMultiLineLine(fullLine);
            } else {
                emitter.emitLine(fullLine);
            }
        } else {
            emitter.emitLine("throw;");
        }
    }

    #! Emit expression statement
    private emitExpressionStmt(hash<auto> node) {
        if (exists node.expression) {
            emitExprLine("", node.expression, ";");
        }
    }

    #! Emit call statement (bare function/method call as statement)
    private emitCallStmt(hash<auto> node) {
        if (exists node.call) {
            emitExprLine("", node.call, ";");
        }
    }

    #! Emit on_block_exit statement
    private emitOnBlockExitStmt(hash<auto> node) {
        string kind;
        switch (node.condition) {
            case 0: kind = "on_exit"; break;
            case 1: kind = "on_success"; break;
            case 2: kind = "on_error"; break;
            default: kind = "on_exit"; break;
        }
        emitter.emitLine(kind + " {");
        emitter.indent();
        if (exists node.statement) {
            emitStatementBody(node.statement);
        }
        emitter.dedent();
        emitter.emitLine("}");
    }

    #! Emit assert statement
    private emitAssertStmt(hash<auto> node) {
        # Use source extraction for assert since it has complex expressions
        string text = stripTrailingSemicolon(extractSourceText(node.loc)) + ";";
        if (text =~ /\n/) {
            emitter.emitMultiLineLine(text);
            if (exists node.loc) {
                skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
            }
        } else {
            emitter.emitLine(text);
        }
    }

    #! Emit debug statement
    private emitDebugStmt(hash<auto> node) {
        string text = extractSourceText(node.loc);
        # @debug(...); needs semicolon, @debug { ... } does not
        if (text =~ /;\s*$/) {
            text = stripTrailingSemicolon(text) + ";";
        }
        if (text =~ /\n/) {
            emitter.emitMultiLineLine(text);
            if (exists node.loc) {
                skipCommentsInRange(node.loc.start_line + 1, node.loc.end_line + 1);
            }
        } else {
            emitter.emitLine(text);
        }
    }

    #! Emit expression (to the emitter)
    private emitExpression(hash<auto> node) {
        emitter.emit(emitExpressionStr(node));
    }

    #! Emit expression as string, stripping outer parentheses for list expressions
    /** Used when the caller already provides surrounding delimiters (e.g., function call
        parens, parameter list parens, closure parens).
    */
    private string emitExpressionStrInner(*hash<auto> node) {
        if (!exists node) {
            return "";
        }
        if (exists node.kind && node.kind == AEK_List) {
            # Emit list items as comma-separated without surrounding parens
            if (!exists node.elements || node.elements.size() == 0) {
                return "";
            }
            list<string> items = ();
            for (int i = 0; i < node.elements.size(); ++i) {
                items += emitExpressionStr(node.elements[i]);
            }
            return items.join(", ");
        }
        return emitExpressionStr(node);
    }

    #! Strip trailing semicolons from source-extracted text
    /** AST location ranges may extend past the expression to include statement
        terminators. This method strips those trailing semicolons.
    */
    private string stripTrailingSemicolon(string text) {
        return regex_subst(text, ";\\s*$", "");
    }

    #! Emit expression as string
    private string emitExpressionStr(*hash<auto> node) {
        if (!exists node) {
            return "";
        }
        if (!exists node.kind) {
            # Might be a name node
            if (exists node.name) {
                return node.name;
            }
            return stripTrailingSemicolon(extractSourceText(node.loc));
        }

        switch (node.kind) {
            case AEK_Access:
                return emitAccessExpr(node);
            case AEK_Assignment:
                return emitAssignmentExpr(node);
            case AEK_Binary:
                return emitBinaryExpr(node);
            case AEK_Call:
                return emitCallExpr(node);
            case AEK_Unary:
                return emitUnaryExpr(node);
            case AEK_Ternary:
                return emitTernaryExpr(node);
            case AEK_Index:
                return emitIndexExpr(node);
            case AEK_Cast:
                return emitCastExpr(node);
            case AEK_List:
                return emitListExpr(node);
            case AEK_Hash:
                return emitHashExpr(node);
            case AEK_HashElement:
                return emitHashElementExpr(node);
            case AEK_HashdeclHash:
                return emitHashdeclHashExpr(node);
            case AEK_Closure:
                return emitClosureExpr(node);
            case AEK_Decl:
                return emitDeclExpr(node);
            case AEK_Name:
                return node.name.name ?? stripTrailingSemicolon(extractSourceText(node.loc));
            case AEK_Literal:
                return emitLiteralExpr(node);
            case AEK_Regex:
                return emitRegexExpr(node);
            case AEK_RegexSubst:
                return emitRegexSubstExpr(node);
            case AEK_RegexTrans:
                return emitRegexTransExpr(node);
            case AEK_Range:
                return emitRangeExpr(node);
            case AEK_Returns:
                return emitReturnsExpr(node);
            case AEK_Find:
                return emitFindExpr(node);
            case AEK_SwitchBody:
                return emitSwitchBodyExpr(node);
            case AEK_Case:
                return emitCaseExpr(node);
            case AEK_Backquote:
                return "`" + (node.command ?? "") + "`";
            case AEK_ImplicitArg:
                return stripTrailingSemicolon(extractSourceText(node.loc));
            case AEK_ImplicitElem:
                return stripTrailingSemicolon(extractSourceText(node.loc));
            case AEK_ContextMod:
                return stripTrailingSemicolon(extractSourceText(node.loc));
            case AEK_ContextRow:
                return stripTrailingSemicolon(extractSourceText(node.loc));
            case AEK_ConstrInit:
                return emitConstrInitExpr(node);
            case AEK_StringConcat:
                return emitStringConcatExpr(node);
            default:
                return stripTrailingSemicolon(extractSourceText(node.loc));
        }
    }

    #! Emit literal expression, preserving source text when possible
    /** Falls back to value representation when source extraction returns invalid text
        (e.g., for case expressions where the AST location may include the colon).
    */
    private string emitLiteralExpr(hash<auto> node) {
        if (exists node.literalKind) {
            switch (node.literalKind) {
                case "string": {
                    if (exists node.value) {
                        # Try source extraction to preserve original quote style (single vs double)
                        # Use normalized extraction for multi-line strings to ensure idempotent
                        # indentation when strings are inside nested structures (hashes, lists)
                        string src = stripTrailingSemicolon(extractSourceText(node.loc));
                        if (src.size() >= 2 && (src[0] == "\"" || src[0] == "'")) {
                            if (src =~ /\n/) {
                                src = stripTrailingSemicolon(
                                    extractNormalizedSourceText(node.loc));
                            }
                            return src;
                        }
                        return "\"" + node.value + "\"";
                    }
                    break;
                }
                case "int":
                case "float":
                case "number": {
                    if (exists node.value) {
                        # Try source extraction to preserve hex/octal/number format
                        string src = stripTrailingSemicolon(extractSourceText(node.loc));
                        # Validate: must look like a valid numeric literal
                        # (digits, hex prefix, decimal points, exponents, number suffix)
                        if (src != "" && src =~ /^[-+]?(?:0[xX][0-9a-fA-F]+|[0-9][0-9_.]*(?:\.[0-9]*)?(?:[eE][-+]?[0-9]+)?n?)$/) {
                            return src;
                        }
                        return string(node.value);
                    }
                    break;
                }
                case "bool":
                    return node.value ? "True" : "False";
                case "nothing":
                    return "NOTHING";
                case "null":
                    return "NULL";
                case "ellipses":
                    return "...";
                case "date": {
                    if (exists node.value) {
                        string src = stripTrailingSemicolon(extractSourceText(node.loc));
                        if (src != "" && src !~ /^[;:,)\]}]/) {
                            return src;
                        }
                        return string(node.value);
                    }
                    break;
                }
                case "binary": {
                    if (exists node.value) {
                        return node.value;
                    }
                    break;
                }
            }
        }
        # Fallback: source extraction
        string src = stripTrailingSemicolon(extractSourceText(node.loc));
        if (src != "" && src !~ /^[;:,)\]}]/) {
            return src;
        }
        return "";
    }

    #! Emit access expression (a.b) with parenthesization of complex left sides
    private string emitAccessExpr(hash<auto> node) {
        string var = emitExpressionStr(node.variable);
        string member = emitExpressionStr(node.member);
        if (exists node.variable.kind
                && (node.variable.kind == AEK_Binary
                    || node.variable.kind == AEK_Ternary
                    || node.variable.kind == AEK_Assignment)) {
            var = "(" + var + ")";
        }
        return var + "." + member;
    }

    #! Emit assignment expression
    private string emitAssignmentExpr(hash<auto> node) {
        string left = emitExpressionStr(node.left);
        string right = emitExpressionStr(node.right);
        return left + " = " + right;
    }

    #! Resolve operator name to symbol string
    /** Handles both formats: "Plus" and "AOK_Plus"
    */
    private string resolveOperator(string op) {
        # Try direct lookup
        *string result = OperatorStrings{op};
        if (exists result) {
            return result;
        }
        # Try with AOK_ prefix
        result = OperatorStrings{"AOK_" + op};
        if (exists result) {
            return result;
        }
        # Return as-is
        return op;
    }

    #! Get the precedence of an operator
    private int getOperatorPrecedence(string op) {
        string key = op;
        if (key !~ /^AOK_/) {
            key = "AOK_" + key;
        }
        return OperatorPrecedence{key} ?? 0;
    }

    #! Emit binary expression with precedence-aware parenthesization
    private string emitBinaryExpr(hash<auto> node) {
        string left = emitExpressionStr(node.left);
        string right = emitExpressionStr(node.right);
        string op = node.op ?? "Plus";
        string opStr = resolveOperator(op);

        int myPrec = getOperatorPrecedence(op);
        # Parenthesize left child if lower precedence
        if (exists node.left.kind && node.left.kind == AEK_Binary && exists node.left.op) {
            if (getOperatorPrecedence(node.left.op) < myPrec) {
                left = "(" + left + ")";
            }
        }
        # Parenthesize right child if lower or equal precedence (right-assoc protection)
        if (exists node.right.kind && node.right.kind == AEK_Binary && exists node.right.op) {
            if (getOperatorPrecedence(node.right.op) <= myPrec) {
                right = "(" + right + ")";
            }
        }
        # Parenthesize ternary children
        if (exists node.left.kind && node.left.kind == AEK_Ternary) {
            left = "(" + left + ")";
        }
        if (exists node.right.kind && node.right.kind == AEK_Ternary) {
            right = "(" + right + ")";
        }

        return left + " " + opStr + " " + right;
    }

    #! Emit call expression
    private string emitCallExpr(hash<auto> node) {
        string target = emitExpressionStr(node.target);
        string args = exists node.args ? emitExpressionStrInner(node.args) : "";
        return target + "(" + args + ")";
    }

    #! Emit unary expression
    private string emitUnaryExpr(hash<auto> node) {
        string operand = emitExpressionStr(node.expression);
        string op = node.op ?? "";

        string opStr = resolveOperator(op);

        # Post-increment/decrement
        if (op == "PostIncrement" || op == "PostDecrement"
                || op == "AOK_PostIncrement" || op == "AOK_PostDecrement") {
            return operand + opStr;
        }

        # Word operators (delete, exists, etc)
        if (opStr =~ /^[a-z]/) {
            return opStr + " " + operand;
        }

        return opStr + operand;
    }

    #! Emit ternary expression
    private string emitTernaryExpr(hash<auto> node) {
        string cond = emitExpressionStr(node.condition);
        string exprTrue = emitExpressionStr(node.exprTrue);
        string exprFalse = emitExpressionStr(node.exprFalse);
        return cond + " ? " + exprTrue + " : " + exprFalse;
    }

    #! Emit index expression
    private string emitIndexExpr(hash<auto> node) {
        string var = emitExpressionStr(node.variable);
        string idx = emitExpressionStr(node.index);
        # Strip surrounding brackets if source extraction included them
        if (idx =~ /^\[.*\]$/) {
            idx = idx.substr(1, idx.size() - 2);
        }
        if (idx =~ /^\{.*\}$/) {
            idx = idx.substr(1, idx.size() - 2);
        }
        # indexKind: 0 = square brackets (list/array), 1 = curly brackets (hash)
        if (node.indexKind == 1) {
            return var + "{" + idx + "}";
        }
        return var + "[" + idx + "]";
    }

    #! Emit cast expression
    private string emitCastExpr(hash<auto> node) {
        string type = node.castType.name ?? "";
        string obj = emitExpressionStr(node.obj);
        return "cast<" + type + ">(" + obj + ")";
    }

    #! Emit list expression
    private string emitListExpr(hash<auto> node) {
        if (!exists node.elements || node.elements.size() == 0) {
            return "()";
        }

        list<string> items = ();
        for (int i = 0; i < node.elements.size(); ++i) {
            items += emitExpressionStr(node.elements[i]);
        }
        string result = items.join(", ");

        # Short lists on one line
        if (result.size() < 80) {
            return "(" + result + ")";
        }

        # Long lists multi-line — indent items to preserve nested structure
        list<string> indentedItems = ();
        foreach string item in (items) {
            list<string> lines = item.split("\n");
            string indented = SourceEmitter::INDENT_STR + lines[0];
            for (int j = 1; j < lines.size(); ++j) {
                indented += "\n" + SourceEmitter::INDENT_STR + lines[j];
            }
            indentedItems += indented;
        }
        if (items.size() == 1) {
            return "(\n" + indentedItems[0] + "\n)";
        }
        return "(\n" + indentedItems.join(",\n") + ",\n)";
    }

    #! Emit hash expression
    private string emitHashExpr(hash<auto> node) {
        # hashKind: 0 = curly braces (default), 1 = parentheses
        bool useParens = (node.hashKind == 1);
        string open = useParens ? "(" : "{";
        string close = useParens ? ")" : "}";

        if (!exists node.elements || node.elements.size() == 0) {
            return open + close;
        }

        list<string> items = ();
        for (int i = 0; i < node.elements.size(); ++i) {
            items += emitExpressionStr(node.elements[i]);
        }
        string result = items.join(", ");

        if (result.size() < 80) {
            return open + result + close;
        }

        # Indent all lines of each item to preserve nested structure
        list<string> indentedItems = ();
        foreach string item in (items) {
            list<string> lines = item.split("\n");
            string indented = SourceEmitter::INDENT_STR + lines[0];
            for (int j = 1; j < lines.size(); ++j) {
                indented += "\n" + SourceEmitter::INDENT_STR + lines[j];
            }
            indentedItems += indented;
        }
        return open + "\n" + indentedItems.join(",\n") + ",\n" + close;
    }

    #! Emit hash element expression
    private string emitHashElementExpr(hash<auto> node) {
        string key = emitExpressionStr(node.key);
        string val = emitExpressionStr(node.value);
        return key + ": " + val;
    }

    #! Emit hashdecl hash expression
    private string emitHashdeclHashExpr(hash<auto> node) {
        string type = node{"hashdecl"}.name ?? "";
        string hash_val = emitExpressionStr(node{"hash"});
        return "<" + type + ">" + hash_val;
    }

    #! Emit closure expression
    private string emitClosureExpr(hash<auto> node) {
        if (exists node.closure) {
            return emitClosureDeclStr(node.closure);
        }
        return extractSourceText(node.loc);
    }

    #! Emit closure declaration as inline string
    private string emitClosureDeclStr(hash<auto> node) {
        string mods = getModifierStr(node.modifiers);
        string header = "";
        if (mods != "") {
            header += mods + " ";
        }
        if (exists node.returnType) {
            header += emitExpressionStr(node.returnType) + " ";
        }
        header += "sub(";
        if (exists node.params) {
            header += emitExpressionStrInner(node.params);
        }
        header += ")";

        if (!exists node.body) {
            return header;
        }

        # Body is an ASK_Block whose loc already includes { and }
        return header + " " + extractNormalizedSourceText(node.body.loc);
    }

    #! Emit decl expression (variable declaration in expression context)
    private string emitDeclExpr(hash<auto> node) {
        return stripTrailingSemicolon(extractNormalizedSourceText(node.loc));
    }

    #! Emit range expression
    private string emitRangeExpr(hash<auto> node) {
        string left = exists node.left ? emitExpressionStr(node.left) : "";
        string right = exists node.right ? emitExpressionStr(node.right) : "";
        return left + ".." + right;
    }

    #! Emit returns expression
    private string emitReturnsExpr(hash<auto> node) {
        return "returns " + emitExpressionStr(node.typeName);
    }

    #! Emit find expression
    private string emitFindExpr(hash<auto> node) {
        return stripTrailingSemicolon(extractSourceText(node.loc));
    }

    #! Escape forward slashes in regex patterns for use with / delimiters
    private string escapeRegexSlashes(string pattern) {
        return regex_subst(pattern, "/", "\\/", RE_Global);
    }

    #! Build regex flags string from AST fields
    private string buildRegexFlags(hash<auto> node) {
        string flags = "";
        if (!node.caseSensitive) { flags += "i"; }
        if (node.extended) { flags += "x"; }
        if (node.dotAll) { flags += "s"; }
        if (node.multiline) { flags += "m"; }
        if (node.global) { flags += "g"; }
        return flags;
    }

    #! Emit regex match expression, reconstructing from AST fields
    private string emitRegexExpr(hash<auto> node) {
        # Always reconstruct from AST fields since source extraction may not
        # include trailing flags (loc end_column may exclude them)
        string prefix = node.extractRegex ? "x/" : "/";
        string pattern = escapeRegexSlashes(node.str ?? "");
        return prefix + pattern + "/" + buildRegexFlags(node);
    }

    #! Emit regex substitution expression, reconstructing from AST fields
    private string emitRegexSubstExpr(hash<auto> node) {
        # Escape / in source and target since AST stores unescaped patterns
        string source = escapeRegexSlashes(node.source ?? "");
        string target = escapeRegexSlashes(node.target ?? "");
        return "s/" + source + "/" + target + "/" + buildRegexFlags(node);
    }

    #! Emit regex transliteration expression, reconstructing from AST fields
    private string emitRegexTransExpr(hash<auto> node) {
        string source = escapeRegexSlashes(node.source ?? "");
        string target = escapeRegexSlashes(node.target ?? "");
        return "tr/" + source + "/" + target + "/";
    }

    #! Emit switch body expression
    private string emitSwitchBodyExpr(hash<auto> node) {
        if (!exists node.cases || node.cases.size() == 0) {
            return "";
        }

        string result = "";
        for (int i = 0; i < node.cases.size(); ++i) {
            result += emitCaseExpr(node.cases[i]);
        }
        return result;
    }

    #! Emit case expression
    private string emitCaseExpr(hash<auto> node) {
        if (node.defaultCase) {
            emitter.emitLine("default:");
        } else {
            string caseExpr = emitExpressionStr(node.caseExpr);
            emitter.emitLine("case " + caseExpr + ":");
        }
        emitter.indent();
        if (exists node.statements) {
            emitStatementBody(node.statements);
        }
        emitter.dedent();
        return "";
    }

    #! Emit constructor initializer expression
    private string emitConstrInitExpr(hash<auto> node) {
        if (!exists node.inits || node.inits.size() == 0) {
            return "";
        }

        list<string> items = ();
        for (int i = 0; i < node.inits.size(); ++i) {
            items += emitExpressionStr(node.inits[i]);
        }
        return items.join(", ");
    }

    #! Emit string concatenation expression (implicit adjacent strings)
    private string emitStringConcatExpr(hash<auto> node) {
        if (!exists node.parts || node.parts.size() == 0) {
            return "";
        }
        list<string> items = ();
        for (int i = 0; i < node.parts.size(); ++i) {
            items += emitExpressionStr(node.parts[i]);
        }
        return items.join(" ");
    }

    #! Extract source text from original source using location coordinates
    /** Location coordinates from getNodesInfo() are 0-indexed (start_line, start_column, end_line, end_column).
    */
    private string extractSourceText(hash<auto> loc) {
        if (!exists loc) {
            return "";
        }

        # Support both location formats
        int fl;
        int fc;
        int ll;
        int lc;
        if (exists loc.start_line) {
            # 0-indexed format from getNodesInfo()
            fl = loc.start_line;
            fc = loc.start_column;
            ll = loc.end_line;
            lc = loc.end_column;
        } else if (exists loc.firstLine) {
            # 1-indexed format from getComments()
            fl = loc.firstLine - 1;
            fc = loc.firstCol - 1;
            ll = loc.lastLine - 1;
            lc = loc.lastCol - 1;
        } else {
            return "";
        }

        if (fl < 0 || fl >= sourceLines.size()) {
            return "";
        }
        if (ll >= sourceLines.size()) {
            ll = sourceLines.size() - 1;
        }

        if (fl == ll) {
            # Single line extraction
            string line = sourceLines[fl];
            if (fc < 0) {
                fc = 0;
            }
            if (lc > line.size()) {
                lc = line.size();
            }
            if (fc >= line.size()) {
                return "";
            }
            return line.substr(fc, lc - fc);
        }

        # Multi-line extraction
        string result = "";
        for (int i = fl; i <= ll; ++i) {
            if (i >= sourceLines.size()) {
                break;
            }
            string line = sourceLines[i];
            if (i == fl) {
                if (fc < 0) {
                    fc = 0;
                }
                if (fc < line.size()) {
                    result += line.substr(fc);
                }
            } else if (i == ll) {
                if (lc > line.size()) {
                    lc = line.size();
                }
                result += "\n" + line.substr(0, lc);
            } else {
                result += "\n" + line;
            }
        }

        return result;
    }

    #! Extract source text with normalized indentation for expression embedding
    /** For multi-line text, strips common base indentation from lines 2+ so
        subsequent lines have only relative indentation. This prevents double-indentation
        when the text is embedded in expression strings and later re-indented.
    */
    private string extractNormalizedSourceText(hash<auto> loc) {
        string text = extractSourceText(loc);
        if (text !~ /\n/) {
            return text;
        }
        list<string> lines = text.split("\n");
        if (lines.size() <= 1) {
            return text;
        }

        # Find minimum indentation of non-empty subsequent lines
        int minIndent = 999;
        for (int i = 1; i < lines.size(); ++i) {
            string line = lines[i];
            if (regex_subst(line, "^\\s+$", "").empty()) {
                continue;
            }
            int indent = 0;
            while (indent < line.size() && (line[indent] == " " || line[indent] == "\t")) {
                ++indent;
            }
            if (indent < minIndent) {
                minIndent = indent;
            }
        }
        if (minIndent == 999 || minIndent == 0) {
            return text;
        }

        # Strip minIndent from subsequent lines
        string result = lines[0];
        for (int i = 1; i < lines.size(); ++i) {
            string line = lines[i];
            int stripped = 0;
            while (stripped < minIndent && stripped < line.size()
                    && (line[stripped] == " " || line[stripped] == "\t")) {
                ++stripped;
            }
            result += "\n" + (stripped > 0 ? line.substr(stripped) : line);
        }
        return result;
    }

    #! Node types that create an indent level when unclosed
    /** These are block-level constructs whose body is indented by one level.
        \c namespace_declaration is intentionally excluded because Qore convention
        is to keep namespace contents at the top level (left-justified).
    */
    public const IndentNodeTypes = {
        "block": True,
        "class_declaration": True,
        "enum_declaration": True,
        "switch_statement": True,
        "hash_literal": True,
    };

    #! Node types that create an extra indent level inside switch blocks
    public const SwitchCaseNodeTypes = {
        "switch_case": True,
        "default_case": True,
    };

    #! Calculates the indent level for the next line of input
    /** Uses namespace-aware brace counting (string/comment aware) as the primary method,
        enhanced with tree-sitter CST analysis to detect switch case/default statements
        (which get an extra indent level).

        @param code The accumulated code so far (incomplete)
        @return The indent level (number of indent steps) for the next line

        @par Example
        @code{.py}
int level = CodeFormatter::calculateIndentLevel("if (True) {");
# level == 1
string indent = strmul(SourceEmitter::INDENT_STR, level);
# indent == "    "
        @endcode
    */
    public static int calculateIndentLevel(string code) {
        # Use namespace-aware brace counting (handles nested/incomplete code reliably)
        int depth = calculateIndentLevelNsAware(code);

        # Use CST to detect switch cases for refinement
        try {
            AstParser parser();
            *AstTree tree = parser.parseString(code);
            if (exists tree) {
                *list<hash<auto>> nodes = tree.getNodesInfo();
                if (nodes) {
                    int lastLine = code.split("\n").size() - 1;
                    int case_depth = 0;
                    foreach hash<auto> node in (nodes) {
                        walkForCaseDepth(node, lastLine, \case_depth);
                    }
                    depth += case_depth;
                }
            }
        } catch () {
            # CST unavailable; brace counting result stands
        }

        return depth > 0 ? depth : 0;
    }

    #! Walks a CST node tree to detect switch case/default nodes
    /** @param node The CST node to examine
        @param lastLine The 0-based line number of the last line of input
        @param case_depth Reference to switch case depth counter
    */
    public static walkForCaseDepth(hash<auto> node, int lastLine, reference<int> case_depth) {
        string type = node.type ?? "";

        # Get end_line from the loc hash
        int end_line = -1;
        if (node.loc) {
            end_line = node.loc.end_line ?? -1;
        }

        # Detect switch case/default extending to end — adds extra indent
        if (SwitchCaseNodeTypes{type} && end_line >= lastLine) {
            case_depth += 1;
        }

        # Recurse into children
        if (node.children) {
            foreach hash<auto> child in (node.children) {
                walkForCaseDepth(child, lastLine, \case_depth);
            }
        }
    }

    #! Namespace-aware indent level calculation using brace counting
    /** Counts `{`/`}` while skipping strings and comments, and excludes braces
        that belong to namespace declarations (Qore convention: namespace contents
        are left-justified).

        @param code The accumulated code
        @return The indent level based on net open braces (excluding namespace braces)
    */
    public static int calculateIndentLevelNsAware(string code) {
        return countBraceDepth(code, True);
    }

    #! Fallback indent level calculation using simple brace counting
    /** Used when tree-sitter parsing fails. Counts `{`/`}` while skipping
        strings and comments.

        @param code The accumulated code
        @return The indent level based on net open braces
    */
    public static int calculateIndentLevelFallback(string code) {
        return countBraceDepth(code, False);
    }

    #! Core brace-counting implementation shared by namespace-aware and fallback modes
    /** Counts `{`/`}` while skipping strings and comments.  When @a ns_aware
        is True, braces belonging to namespace declarations are excluded from
        the depth count (Qore convention: namespace contents are left-justified).

        @param code The accumulated code
        @param ns_aware When True, exclude namespace braces from the count
        @return The indent level based on net open braces (clamped to >= 0)
    */
    private static int countBraceDepth(string code, bool ns_aware) {
        int depth = 0;
        bool in_string = False;
        string string_char;
        bool escape_next = False;
        bool in_block_comment = False;

        # Namespace tracking (only used when ns_aware is True)
        list<bool> ns_brace_stack = ();
        bool namespace_pending = False;

        for (int i = 0; i < code.length(); i++) {
            string c = code[i];

            # Block comment handling
            if (in_block_comment) {
                if (c == "*" && i + 1 < code.length() && code[i + 1] == "/") {
                    in_block_comment = False;
                    i++;
                }
                continue;
            }

            if (in_string) {
                if (escape_next) {
                    escape_next = False;
                    continue;
                }
                if (c == "\\") {
                    escape_next = True;
                    continue;
                }
                if (c == string_char) {
                    in_string = False;
                }
                continue;
            }

            # Check for block comment start
            if (c == "/" && i + 1 < code.length() && code[i + 1] == "*") {
                in_block_comment = True;
                i++;
                continue;
            }

            # Line comment - skip to end of line
            if (c == "#") {
                int eol = code.find("\n", i);
                if (eol < 0) {
                    break;
                }
                i = eol;
                continue;
            }

            # String start
            if (c == "\"" || c == "'") {
                in_string = True;
                string_char = c;
                continue;
            }

            # Check for "namespace" keyword at word boundary
            if (ns_aware && c == "n" && !namespace_pending) {
                string remainder = code.substr(i);
                if (remainder =~ /^namespace\b/) {
                    namespace_pending = True;
                    i += 8;  # skip "namespace" (9 chars - 1 since loop increments)
                    continue;
                }
            }

            # Reset namespace_pending on semicolon (forward declaration: "namespace Foo;")
            if (c == ";" && namespace_pending) {
                namespace_pending = False;
            }

            if (c == "{") {
                if (ns_aware && namespace_pending) {
                    # This brace belongs to a namespace — don't count it
                    ns_brace_stack += True;
                    namespace_pending = False;
                } else {
                    if (ns_aware) {
                        ns_brace_stack += False;
                    }
                    depth++;
                }
            } else if (c == "}") {
                if (ns_aware && ns_brace_stack.size() > 0) {
                    bool was_ns = ns_brace_stack.last();
                    splice ns_brace_stack, ns_brace_stack.size() - 1, 1;
                    if (!was_ns) {
                        depth--;
                    }
                } else {
                    depth--;
                }
            }
        }

        return depth > 0 ? depth : 0;
    }
}

} # namespace QoreCodeFormat
