# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TerminalMarkdownRenderer class definition

/*  TerminalMarkdownRenderer.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace for the TerminalFormat module
public namespace TerminalFormat {

#! Renders markdown text to ANSI-escaped terminal output
/** Supports a practical subset of CommonMark:
    - Headings (\c # through \c ###)
    - Bold (\c **text**), italic (\c *text*), bold-italic (\c ***text***)
    - Inline code (\c `code`)
    - Fenced code blocks (\c ```)
    - Unordered lists (\c - and \c *)
    - Ordered lists (\c 1.)
    - Blockquotes (\c >)
    - Horizontal rules (\c ---)
    - Pipe tables (\c | col1 | col2 |)
    - Links (\c [text](url)) — rendered as \c text (url)

    @par Example
    @code{.py}
    TerminalMarkdownRenderer md();
    printf("%s\n", md.render("# Title\n\nSome **bold** text with `code`."));
    @endcode

    @since TerminalFormat 1.0
*/
public class TerminalMarkdownRenderer {
    private {
        #! Color output helper
        Util::TerminalColor color;
    }

    #! Creates the renderer
    /** @param no_color if True, disables ANSI color output
    */
    constructor(bool no_color = False) {
        color = new Util::TerminalColor(!no_color);
    }

    #! Renders markdown text to a single ANSI-formatted string
    /** @param text the markdown source text
        @return the rendered terminal string
    */
    string render(string text) {
        return renderLines(text).join("\n");
    }

    #! Renders markdown text to a list of ANSI-formatted lines
    /** @param text the markdown source text
        @return list of rendered lines
    */
    list<string> renderLines(string text) {
        list<string> output = ();
        list<string> lines = text.split("\n");
        int i = 0;
        while (i < lines.size()) {
            string line = lines[i];

            # Fenced code block
            if (line =~ /^```/) {
                *string lang = (line =~ x/^```(\w*)/)[0];
                ++i;
                list<string> code_lines = ();
                while (i < lines.size() && lines[i] !~ /^```/) {
                    code_lines += lines[i];
                    ++i;
                }
                if (i < lines.size()) {
                    ++i;  # skip closing ```
                }
                output += renderCodeBlock(code_lines);
                continue;
            }

            # Heading
            if (line =~ /^#{1,6}\s/) {
                output += renderHeading(line);
                ++i;
                continue;
            }

            # Horizontal rule
            if (line =~ /^(---+|\*\*\*+|___+)\s*$/) {
                output += color.dim(strmul("─", 40));
                ++i;
                continue;
            }

            # Pipe table — collect all consecutive table lines
            if (line =~ /^\|/) {
                list<string> table_lines = ();
                while (i < lines.size() && lines[i] =~ /^\|/) {
                    table_lines += lines[i];
                    ++i;
                }
                output += renderTable(table_lines);
                continue;
            }

            # Blockquote
            if (line =~ /^>\s?/) {
                output += renderBlockquote(line);
                ++i;
                continue;
            }

            # Unordered list item
            if (line =~ /^\s*[-*]\s/) {
                output += renderUnorderedListItem(line);
                ++i;
                continue;
            }

            # Ordered list item
            if (line =~ /^\s*\d+\.\s/) {
                output += renderOrderedListItem(line);
                ++i;
                continue;
            }

            # Empty line
            if (!line.val() || line =~ /^\s*$/) {
                output += "";
                ++i;
                continue;
            }

            # Regular paragraph line — apply inline formatting
            output += renderInline(line);
            ++i;
        }
        return output;
    }

    #! Renders a heading line
    private string renderHeading(string line) {
        *list<string> parts = (line =~ x/^(#{1,6})\s+(.*)/);
        if (!parts) {
            return renderInline(line);
        }
        int level = parts[0].length();
        string text = parts[1];
        switch (level) {
            case 1:
                return color.bold(text.upr());
            case 2:
                return color.bold(text);
            default:
                return color.bold(text);
        }
    }

    #! Renders a fenced code block
    private list<string> renderCodeBlock(list<string> lines) {
        list<string> output = ();
        foreach string line in (lines) {
            output += "  " + color.dim(line);
        }
        return output;
    }

    #! Renders a blockquote line
    private string renderBlockquote(string line) {
        string content = regex_subst(line, "^>\\s?", "");
        return color.dim("│ ") + renderInline(content);
    }

    #! Renders an unordered list item
    private string renderUnorderedListItem(string line) {
        *list<string> parts = (line =~ x/^(\s*)[-*]\s+(.*)/);
        if (!parts) {
            return renderInline(line);
        }
        string indent = parts[0];
        string content = parts[1];
        return indent + color.bold("•") + " " + renderInline(content);
    }

    #! Renders an ordered list item
    private string renderOrderedListItem(string line) {
        *list<string> parts = (line =~ x/^(\s*)(\d+)\.\s+(.*)/);
        if (!parts) {
            return renderInline(line);
        }
        string indent = parts[0];
        string num = parts[1];
        string content = parts[2];
        return indent + color.bold(num + ".") + " " + renderInline(content);
    }

    #! Renders a pipe table
    private list<string> renderTable(list<string> lines) {
        # Parse table rows
        list<list<string>> rows = ();
        bool has_separator = False;
        int separator_idx = -1;
        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            # Check for separator row (| --- | --- |)
            if (line =~ /^\|[\s\-:|]+\|$/) {
                has_separator = True;
                separator_idx = i;
                continue;
            }
            # Parse cells
            string trimmed = line;
            trimmed =~ s/^\|//;
            trimmed =~ s/\|$//;
            list<string> cells = map trim($1), trimmed.split("|");
            rows += (cells,);
        }

        if (!rows) {
            return ();
        }

        # Calculate column widths
        int num_cols = rows[0].size();
        list<int> widths = map 0, range(num_cols);
        foreach list<string> row in (rows) {
            for (int c = 0; c < min(row.size(), num_cols); ++c) {
                if (row[c].length() > widths[c]) {
                    widths[c] = row[c].length();
                }
            }
        }

        # Render
        list<string> output = ();
        for (int r = 0; r < rows.size(); ++r) {
            list<string> cells = rows[r];
            string formatted = "";
            for (int c = 0; c < num_cols; ++c) {
                string cell = c < cells.size() ? cells[c] : "";
                string padded = cell + strmul(" ", widths[c] - cell.length());
                if (r == 0 && has_separator) {
                    padded = color.bold(padded);
                }
                formatted += padded;
                if (c < num_cols - 1) {
                    formatted += color.dim("  ");
                }
            }
            output += formatted;
            # Add separator after header
            if (r == 0 && has_separator) {
                string sep = "";
                for (int c = 0; c < num_cols; ++c) {
                    sep += strmul("─", widths[c]);
                    if (c < num_cols - 1) {
                        sep += "──";
                    }
                }
                output += color.dim(sep);
            }
        }
        return output;
    }

    #! Applies inline markdown formatting to a line
    /** Handles bold, italic, bold-italic, inline code, and links.
    */
    string renderInline(string line) {
        # Process inline code first (to prevent other formatting inside code spans)
        line = regex_subst(line, "`([^`]+)`", color.key("$1"), RE_Global);

        # Bold-italic (***text***)
        line = regex_subst(line, "\\*\\*\\*([^*]+)\\*\\*\\*", color.bold("$1"), RE_Global);

        # Bold (**text**)
        line = regex_subst(line, "\\*\\*([^*]+)\\*\\*", color.bold("$1"), RE_Global);

        # Italic (*text*)
        line = regex_subst(line, "\\*([^*]+)\\*", color.dim("$1"), RE_Global);

        # Links [text](url) → text (url)
        line = regex_subst(line, "\\[([^\\]]+)\\]\\(([^)]+)\\)", "$1 " + color.dim("($2)"),
            RE_Global);

        return line;
    }
}

} # namespace TerminalFormat
