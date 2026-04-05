# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TerminalTableRenderer class definition

/*  TerminalTableRenderer.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Renders tabular data with Unicode box-drawing characters and column alignment
/** Formats a list of rows with headers into a clean terminal table using
    Unicode box-drawing characters for borders.

    @par Example
    @code{.py}
    TerminalTableRenderer tbl();
    list<string> headers = ("Name", "Status", "ID");
    list<list<string>> rows = (
        ("my-service", "loaded", "42"),
        ("other-svc", "unloaded", "43"),
    );
    printf("%s\n", tbl.render(headers, rows));
    @endcode

    Output:
    @verbatim
    Name         Status     ID
    ──────────── ────────── ──
    my-service   loaded     42
    other-svc    unloaded   43
    @endverbatim

    @since TerminalFormat 1.0
*/
public class TerminalTableRenderer {
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

    #! Renders a table from headers and row data
    /** @param headers list of column header strings
        @param rows list of row data (each row is a list of cell strings)
        @param right_align optional set of 0-based column indices to right-align

        @return the formatted table as a single string
    */
    string render(list<string> headers, list<auto> rows, *hash<string, bool> right_align) {
        return renderLines(headers, rows, right_align).join("\n");
    }

    #! Renders a table to a list of lines
    /** @param headers list of column header strings
        @param rows list of row data (each row is a list of cell strings)
        @param right_align optional set of 0-based column indices to right-align

        @return list of formatted lines
    */
    list<string> renderLines(list<string> headers, list<auto> rows,
            *hash<string, bool> right_align) {
        int num_cols = headers.size();

        # Calculate column widths
        list<int> widths = map $1.length(), headers;
        foreach auto row in (rows) {
            list<auto> cells = row.typeCode() == NT_LIST ? row : (row,);
            for (int c = 0; c < min(cells.size(), num_cols); ++c) {
                int len = cells[c].toString().length();
                if (len > widths[c]) {
                    widths[c] = len;
                }
            }
        }

        list<string> output = ();

        # Header row
        string hdr_line = "";
        for (int c = 0; c < num_cols; ++c) {
            string cell = padCell(headers[c], widths[c], right_align{c.toString()});
            hdr_line += color.bold(cell);
            if (c < num_cols - 1) {
                hdr_line += "  ";
            }
        }
        output += hdr_line;

        # Separator
        string sep = "";
        for (int c = 0; c < num_cols; ++c) {
            sep += strmul("─", widths[c]);
            if (c < num_cols - 1) {
                sep += "──";
            }
        }
        output += color.dim(sep);

        # Data rows
        foreach auto row in (rows) {
            list<auto> cells = row.typeCode() == NT_LIST ? row : (row,);
            string data_line = "";
            for (int c = 0; c < num_cols; ++c) {
                string val = c < cells.size() ? cells[c].toString() : "";
                data_line += padCell(val, widths[c], right_align{c.toString()});
                if (c < num_cols - 1) {
                    data_line += "  ";
                }
            }
            output += data_line;
        }

        return output;
    }

    #! Renders a table from a list of hashes with automatic header detection
    /** @param data list of row hashes (keys become headers)
        @param columns optional list of column keys to include (default: all keys from first row)

        @return the formatted table as a single string
    */
    string renderFromHashes(list<hash<auto>> data, *list<string> columns) {
        if (!data) {
            return "";
        }
        if (!columns) {
            columns = keys data[0];
        }
        list<string> headers = columns;
        list<list<string>> rows = ();
        foreach hash<auto> row in (data) {
            rows += (map row{$1}.toString(), columns,);
        }
        return render(headers, rows);
    }

    #! Pads a cell value to the given width
    private string padCell(string val, int width, bool right = False) {
        int padding = width - val.length();
        if (padding <= 0) {
            return val;
        }
        if (right) {
            return strmul(" ", padding) + val;
        }
        return val + strmul(" ", padding);
    }
}

} # namespace TerminalFormat
