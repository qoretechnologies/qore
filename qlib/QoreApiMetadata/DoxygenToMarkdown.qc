# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DoxygenToMarkdown.qc Doxygen inline formatting to Markdown converter

/*  DoxygenToMarkdown.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @cond nodoc
public namespace QoreApiMetadata {
#! @endcond

#! Converts doxygen inline formatting to Markdown
public class DoxygenToMarkdown {
    #! Convert doxygen-formatted text to Markdown
    /** @param text the text containing doxygen formatting
        @return the text with doxygen constructs converted to Markdown
    */
    static string convert(*string text) {
        if (!exists text || !text.length()) {
            return text ?? "";
        }

        string result = text;

        # 1. Strip @htmlonly...@endhtmlonly blocks (multiline)
        result = regex_subst(result, "@htmlonly[\\s\\S]*?@endhtmlonly", "", RE_Global);

        # 2. Convert @code{.py}...@endcode to fenced code blocks
        result = DoxygenToMarkdown::convertCodeBlocks(result);

        # 3. Protect existing code blocks and inline backticks from further conversion
        list<string> protected_blocks = ();
        result = DoxygenToMarkdown::protectCodeBlocks(result, \protected_blocks);

        # 4. Convert @par Section: to bold section header
        result = regex_subst(result, "@par\\s+([^\n:]+):", "**$1:**", RE_Global);
        # @par with no colon — just bold it
        result = regex_subst(result, "@par\\s+([^\n]+)", "**$1**", RE_Global);

        # 5. Convert @ref target "display text" → display text (plain)
        result = regex_subst(result, "@ref\\s+\\S+\\s+\"([^\"]+)\"", "$1", RE_Global);

        # 6. Convert @ref target → `target`
        result = regex_subst(result, "@ref\\s+(\\S+)", "`$1`", RE_Global);

        # 7. Convert %ClassName → ClassName (strip % before uppercase letter)
        result = regex_subst(result, "%([A-Z])", "$1", RE_Global);

        # 8. Convert \c word → `word`
        result = regex_subst(result, "\\\\c\\s+(\\S+)", "`$1`", RE_Global);

        # 9. Convert \a word / \e word → *word*
        result = regex_subst(result, "\\\\[ae]\\s+(\\S+)", "*$1*", RE_Global);

        # 10. Convert \b word → **word**
        result = regex_subst(result, "\\\\b\\s+(\\S+)", "**$1**", RE_Global);

        # 11. Convert <tt>text</tt> → `text`
        result = regex_subst(result, "<tt>(.*?)</tt>", "`$1`", RE_Global | RE_Caseless | RE_DotAll);

        # 12. Convert <b>text</b> → **text**
        result = regex_subst(result, "<b>(.*?)</b>", "**$1**", RE_Global | RE_Caseless | RE_DotAll);

        # 13. Convert <i>text</i> → *text*
        result = regex_subst(result, "<i>(.*?)</i>", "*$1*", RE_Global | RE_Caseless | RE_DotAll);

        # 14. Convert HTML tables to Markdown tables
        result = DoxygenToMarkdown::convertHtmlTables(result);

        # 15. Strip remaining HTML tags
        result = regex_subst(result, "</?(?:td|tr|th|div|span|hr|br|p|ul|ol|li|dl|dt|dd|em|strong|pre|blockquote|table|thead|tbody|a)\\b[^>]*>", "", RE_Global | RE_Caseless);

        # 16. HTML entities
        result = regex_subst(result, "&lt;", "<", RE_Global);
        result = regex_subst(result, "&gt;", ">", RE_Global);
        result = regex_subst(result, "&amp;", "&", RE_Global);
        result = regex_subst(result, "&quot;", "\"", RE_Global);
        result = regex_subst(result, "&nbsp;", " ", RE_Global);
        result = regex_subst(result, "&#39;", "'", RE_Global);

        # 17. Strip remaining doxygen-only tags (structural tags not meaningful in output)
        result = regex_subst(result, "@(?:anchor|page|section|subsection|subsubsection|cond|endcond|internal|defgroup|addtogroup|ingroup|file|mainpage|{|})\\b[^\n]*", "", RE_Global);

        # 18. Clean up \\n\\n literal newline markers (common in qpp descriptions)
        result = regex_subst(result, "\\\\n\\\\n", "\n\n", RE_Global);
        result = regex_subst(result, "\\\\n", "\n", RE_Global);

        # Restore protected code blocks
        result = DoxygenToMarkdown::restoreCodeBlocks(result, protected_blocks);

        return result;
    }

    #! Convert @code{.py}...@endcode blocks to fenced code blocks
    private static string convertCodeBlocks(string text) {
        string result = text;
        # match @code or @code{.py} or @code{.lang} followed by content and @endcode
        while (True) {
            *list<string> m = (result =~ x/@code(?:\{[^}]*\})?\s*\n?([\s\S]*?)@endcode/);
            if (!exists m) {
                break;
            }
            string code_content = m[0];
            # trim leading/trailing blank lines from code content
            code_content = regex_subst(code_content, "^\\s*\\n", "");
            code_content = regex_subst(code_content, "\\n\\s*$", "");
            result = regex_subst(result, "@code(?:\\{[^}]*\\})?\\s*\\n?[\\s\\S]*?@endcode",
                "```qore\n" + code_content + "\n```");
        }
        return result;
    }

    #! Protect existing fenced code blocks and inline backticks from conversion
    /** @param text the text to protect
        @param protected_blocks reference to list to store protected blocks
        @return the text with protected blocks replaced by placeholders
    */
    private static string protectCodeBlocks(string text, reference<list<string>> protected_blocks) {
        string result = text;

        # protect fenced code blocks (one at a time since replacement depends on index)
        while (True) {
            *list<string> m = (result =~ x/(```[^`]*```)/s);
            if (!exists m) {
                break;
            }
            int idx = protected_blocks.size();
            protected_blocks += m[0];
            result = regex_subst(result, "```[^`]*```", "\x00PROTECTED_" + string(idx) + "\x00", RE_DotAll);
        }

        # protect inline backticks — collect all spans in one pass, then replace
        *list<string> spans = (result =~ x/(`[^`\n]+`)/g);
        foreach string span in (spans ?? ()) {
            int idx = protected_blocks.size();
            protected_blocks += span;
            # replace first occurrence of this exact span
            int pos = result.find(span);
            if (pos >= 0) {
                result = result.substr(0, pos)
                    + "\x00PROTECTED_" + string(idx) + "\x00"
                    + result.substr(pos + span.length());
            }
        }

        return result;
    }

    #! Restore protected code blocks from placeholders
    private static string restoreCodeBlocks(string result, list<string> protected_blocks) {
        for (int i = protected_blocks.size() - 1; i >= 0; --i) {
            result = regex_subst(result, "\x00PROTECTED_" + string(i) + "\x00",
                protected_blocks[i], RE_Global);
        }
        return result;
    }

    #! Convert HTML <table> elements to Markdown tables
    private static string convertHtmlTables(string text) {
        string result = text;
        while (True) {
            *list<string> m = (result =~ x/<table[^>]*>([\s\S]*?)<\/table>/i);
            if (!exists m) {
                break;
            }
            string table_content = m[0];
            string md_table = DoxygenToMarkdown::htmlTableToMarkdown(table_content);
            result = regex_subst(result, "<table[^>]*>[\\s\\S]*?<\\/table>", md_table, RE_Caseless);
        }
        return result;
    }

    #! Convert HTML table content to Markdown table format
    private static string htmlTableToMarkdown(string table_html) {
        # extract rows
        list<string> rows = ();
        string remaining = table_html;
        while (True) {
            *list<string> rm = (remaining =~ x/<tr[^>]*>([\s\S]*?)<\/tr>/i);
            if (!exists rm) {
                break;
            }
            rows += rm[0];
            remaining = regex_subst(remaining, "<tr[^>]*>[\\s\\S]*?<\\/tr>", "", RE_Caseless);
        }

        if (!rows.size()) {
            return "";
        }

        list<string> md_rows = ();
        bool has_header = False;
        foreach string row in (rows) {
            # check if row has th cells
            list<string> cells = ();
            string row_remaining = row;
            bool is_header = False;

            # try th first
            while (True) {
                *list<string> cm = (row_remaining =~ x/<th[^>]*>([\s\S]*?)<\/th>/i);
                if (!exists cm) {
                    break;
                }
                cells += trim(regex_subst(cm[0], "<[^>]*>", "", RE_Global));
                row_remaining = regex_subst(row_remaining, "<th[^>]*>[\\s\\S]*?<\\/th>",
                    "", RE_Caseless);
                is_header = True;
            }

            # try td cells
            if (!cells.size()) {
                while (True) {
                    *list<string> cm = (row_remaining =~ x/<td[^>]*>([\s\S]*?)<\/td>/i);
                    if (!exists cm) {
                        break;
                    }
                    cells += trim(regex_subst(cm[0], "<[^>]*>", "", RE_Global));
                    row_remaining = regex_subst(row_remaining, "<td[^>]*>[\\s\\S]*?<\\/td>",
                        "", RE_Caseless);
                }
            }

            if (cells.size()) {
                md_rows += "| " + (foldl $1 + " | " + $2, cells) + " |";
                if (is_header && !has_header) {
                    # add separator row
                    list<string> seps = map "---", cells;
                    md_rows += "| " + (foldl $1 + " | " + $2, seps) + " |";
                    has_header = True;
                }
            }
        }

        if (!has_header && md_rows.size() > 0) {
            # no header row detected — add separator after first row
            # count columns from first row
            *list<string> first_cells = (md_rows[0] =~ x/\|/g);
            if (exists first_cells) {
                int col_count = first_cells.size() - 1;
                if (col_count > 0) {
                    list<string> seps = ();
                    for (int i = 0; i < col_count; ++i) {
                        seps += "---";
                    }
                    string sep_row = "| " + (foldl $1 + " | " + $2, seps) + " |";
                    splice md_rows, 1, 0, sep_row;
                }
            }
        }

        return md_rows.size() ? (foldl $1 + "\n" + $2, md_rows) : "";
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
