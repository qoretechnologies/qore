# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SourceEmitter.qc Output buffer with indentation management

/*  SourceEmitter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Output buffer with indentation tracking for code emission
public class SourceEmitter {
    public {
        #! Indentation string (4 spaces)
        const INDENT_STR = "    ";
    }

    private {
        #! Output buffer
        string buf = "";

        #! Current indentation level
        int indentLevel = 0;

        #! Whether the current line needs indentation
        bool needsIndent = True;

        #! Whether the last emitted character was a newline
        bool atLineStart = True;

        #! Number of consecutive blank lines emitted
        int consecutiveBlankLines = 0;
    }

    #! Emit text to the output buffer
    emit(string text) {
        if (!text.empty()) {
            if (needsIndent && atLineStart) {
                buf += strmul(INDENT_STR, indentLevel);
                needsIndent = False;
                atLineStart = False;
            }
            buf += text;
            atLineStart = False;
            consecutiveBlankLines = 0;
        }
    }

    #! Emit multi-line text, dedenting then re-indenting each line at the current level
    emitMultiLine(string text) {
        if (text.empty()) {
            return;
        }
        list<string> parts = text.split("\n");
        if (parts.size() <= 1) {
            emit(text);
            return;
        }

        # Find minimum indentation of non-empty lines after the first
        int minIndent = 999;
        for (int i = 1; i < parts.size(); i++) {
            string part = parts[i];
            if (regex_subst(part, "^\\s+$", "").empty()) {
                continue;
            }
            int indent = 0;
            while (indent < part.size() && (part[indent] == " " || part[indent] == "\t")) {
                ++indent;
            }
            if (indent < minIndent) {
                minIndent = indent;
            }
        }
        if (minIndent == 999) {
            minIndent = 0;
        }

        # Emit first line with current indentation
        if (!parts[0].empty()) {
            if (needsIndent && atLineStart) {
                buf += strmul(INDENT_STR, indentLevel);
                needsIndent = False;
                atLineStart = False;
            }
            buf += parts[0];
            atLineStart = False;
        }

        # Emit subsequent lines, dedented and re-indented
        for (int i = 1; i < parts.size(); i++) {
            buf += "\n";
            string part = parts[i];
            # Strip minIndent characters of leading whitespace
            int stripped = 0;
            while (stripped < minIndent && stripped < part.size()
                    && (part[stripped] == " " || part[stripped] == "\t")) {
                ++stripped;
            }
            string dedented = stripped > 0 ? part.substr(stripped) : part;
            if (!dedented.empty()) {
                buf += strmul(INDENT_STR, indentLevel) + dedented;
                atLineStart = False;
            } else {
                atLineStart = True;
            }
            needsIndent = True;
        }
        consecutiveBlankLines = 0;
    }

    #! Emit a newline
    newline() {
        buf += "\n";
        needsIndent = True;
        atLineStart = True;
        ++consecutiveBlankLines;
    }

    #! Emit a blank line (only if not already after a blank line)
    blankLine() {
        if (consecutiveBlankLines < 2) {
            newline();
        }
    }

    #! Emit text at column 0 (for parse directives) - ignores current indentation
    emitAtColumn0(string text) {
        if (atLineStart) {
            buf += text;
            atLineStart = False;
            needsIndent = False;
            consecutiveBlankLines = 0;
        } else {
            # If we're mid-line, emit a newline first
            newline();
            buf += text;
            atLineStart = False;
            needsIndent = False;
            consecutiveBlankLines = 0;
        }
    }

    #! Emit text followed by a newline at column 0
    emitLineAtColumn0(string text) {
        emitAtColumn0(text);
        newline();
    }

    #! Emit text followed by a newline
    emitLine(string text) {
        emit(text);
        newline();
    }

    #! Emit multi-line text followed by a newline, indenting each line
    emitMultiLineLine(string text) {
        emitMultiLine(text);
        newline();
    }

    #! Increase indentation level
    indent() {
        ++indentLevel;
    }

    #! Decrease indentation level
    dedent() {
        if (indentLevel > 0) {
            --indentLevel;
        }
    }

    #! Get the current output
    string getOutput() {
        # Ensure exactly one trailing newline
        string result = buf;
        # Remove trailing whitespace/newlines
        result = regex_subst(result, "\\s+$", "");
        if (!result.empty()) {
            result += "\n";
        }
        return result;
    }

    #! Get current indentation level
    int getIndentLevel() {
        return indentLevel;
    }

    #! Check if we're at the start of a line
    bool isAtLineStart() {
        return atLineStart;
    }

    #! Emit indented text (indent + emit + newline)
    emitIndentedLine(string text) {
        emit(text);
        newline();
    }

    #! Reset the emitter
    reset() {
        buf = "";
        indentLevel = 0;
        needsIndent = True;
        atLineStart = True;
        consecutiveBlankLines = 0;
    }
}

} # namespace QoreCodeFormat
