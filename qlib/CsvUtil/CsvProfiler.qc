# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CsvProfiler class definition

/*  CsvProfiler.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The primary CsvUtil namespace; all classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
#! Detects the format of delimited text files and profiles their content
/** A delimited text file says nothing about its own format: the separator, character encoding, byte order mark,
    and line ending have to be detected before the file can be read correctly.  This class detects them, reads the
    file as a raw grid of cells, and profiles it with @ref DataProvider::TabularProfiler "TabularProfiler".

    @par Example
    @code{.py}
hash<TabularWorkbookProfile> profile = CsvProfiler::profile(ReadOnlyFile::readBinaryFile("confirmations.csv"));
printf("separator %y, header row %d\n", profile.text_format.separator, profile.sheets[0].header_row);
    @endcode

    @since CsvUtil 1.12
*/
public class CsvProfiler {
    public {
        #! The separators detected, in order of preference
        const Separators = (",", ";", "\t", "|");

        #! The default maximum number of rows read
        const DefaultMaxRows = 10000;

        #! The number of lines used to detect the separator
        const SniffLines = 50;
    }

    #! Detects the text format of delimited text data
    /** @param data the file content
        @param quote the field quote character; separators inside quoted fields are not counted

        @return the separator, the encoding (\c "UTF-8", \c "UTF-16LE", \c "UTF-16BE", or \c "ISO-8859-1" when the
        content is not valid UTF-8), whether the content starts with a byte order mark, the line ending, and the
        quote character

        @since CsvUtil 1.12 the \a quote parameter
    */
    static hash<TabularTextFormat> sniffTextFormat(binary data, string quote = "\"") {
        hash<auto> enc = detectEncoding(data);
        string text = decode(data, enc);
        return <TabularTextFormat>{
            "separator": detectSeparator(text, quote),
            "encoding": enc.encoding,
            "bom": enc.bom,
            "eol": detectEol(text),
            "quote": quote,
        };
    }

    #! Reads delimited text data as a raw grid of cells
    /** @param data the file content
        @param opts options:
        - \c max_rows (int): the maximum number of rows read (default @ref DefaultMaxRows); the total row count is
          still reported
        - \c separator (string): the separator, instead of detecting it
        - \c encoding (string): the character encoding, instead of detecting it
        - \c quote (string): the field quote character (default \c "\""); fields are quoted with it and
          separators inside quoted fields are not counted when the separator is detected

        @return a grid with one sheet named \c "csv", in which every cell is a string and empty lines are empty rows

        @throw CSV-PROFILE-ERROR the \c max_rows option is not positive, or the \c quote option is not one character
    */
    static hash<TabularWorkbookGrid> readGrid(binary data, *hash<auto> opts) {
        int max_rows = opts.max_rows ?? DefaultMaxRows;
        if (max_rows <= 0) {
            throw "CSV-PROFILE-ERROR", sprintf("max_rows must be positive; got %d", max_rows);
        }
        if (exists opts.quote && opts.quote.length() != 1) {
            throw "CSV-PROFILE-ERROR", sprintf("quote must be one character; got %y", opts.quote);
        }
        hash<TabularTextFormat> format = sniffTextFormat(data, opts.quote);
        hash<auto> enc = {"encoding": format.encoding, "bom": format.bom};
        if (opts.encoding) {
            enc.encoding = opts.encoding;
            format.encoding = opts.encoding;
        }
        if (opts.separator) {
            format.separator = opts.separator;
        }
        string text = decode(data, enc);

        hash<TabularSheetGrid> sheet({"name": "csv"});
        int rows = 0;
        if (text.val()) {
            CsvDataIterator i(text, {
                "separator": format.separator,
                "quote": format.quote,
                "eol": format.eol,
                "ignore_empty": False,
                "header_lines": 0,
            });
            while (i.next()) {
                if (++rows <= max_rows) {
                    sheet.rows += (i.getRecordList(),);
                }
            }
        }
        if (rows > max_rows) {
            sheet.row_count = rows;
        }
        return <TabularWorkbookGrid>{
            "format": "csv",
            "sheets": (sheet,),
            "text_format": format,
        };
    }

    #! Profiles delimited text data
    /** @param data the file content
        @param opts grid options, see @ref readGrid()
        @param profile_opts profile options

        @return the profile, including the detected text format

        @throw CSV-PROFILE-ERROR the \c max_rows option is not positive
    */
    static hash<TabularWorkbookProfile> profile(binary data, *hash<auto> opts,
            *hash<TabularProfileOptions> profile_opts) {
        return TabularProfiler::profile(readGrid(data, opts), profile_opts);
    }

    #! Detects the character encoding and byte order mark
    private static hash<auto> detectEncoding(binary data) {
        string head = data.substr(0, 3).toHex();
        if (head == "efbbbf") {
            return {"encoding": "UTF-8", "bom": True, "skip": 3};
        }
        if (head.substr(0, 4) == "fffe") {
            return {"encoding": "UTF-16LE", "bom": True, "skip": 2};
        }
        if (head.substr(0, 4) == "feff") {
            return {"encoding": "UTF-16BE", "bom": True, "skip": 2};
        }
        try {
            convert_encoding(binary_to_string(data, "UTF-8"), "UTF-16LE");
            return {"encoding": "UTF-8", "bom": False};
        } catch (hash<ExceptionInfo> ex) {
            # not valid UTF-8: an 8-bit encoding
            return {"encoding": "ISO-8859-1", "bom": False};
        }
    }

    #! Decodes the content to a UTF-8 string without the byte order mark
    private static string decode(binary data, hash<auto> enc) {
        int skip = enc.skip ?? 0;
        if (!skip && enc.bom) {
            skip = enc.encoding =~ /^UTF-16/ ? 2 : 3;
        }
        binary body = skip ? data.substr(skip) : data;
        return convert_encoding(binary_to_string(body, enc.encoding), "UTF-8");
    }

    #! Detects the line ending
    private static string detectEol(string text) {
        if (text.find("\r\n") >= 0) {
            return "\r\n";
        }
        if (text.find("\n") >= 0) {
            return "\n";
        }
        if (text.find("\r") >= 0) {
            return "\r";
        }
        return "\n";
    }

    #! Returns the number of occurrences of a separator in a line
    private static int countOccurrences(string line, string sep) {
        int rv = 0;
        int pos = 0;
        while ((pos = line.find(sep, pos)) >= 0) {
            ++rv;
            pos += sep.length();
        }
        return rv;
    }

    #! Detects the separator
    /** For each candidate separator, counts its occurrences outside quoted fields in each of the first lines; the
        separator is the candidate whose count is the same in most lines, and on a tie the one with more fields.  A
        file with one column has no separator in its lines; the default comma is returned.
    */
    private static string detectSeparator(string text, string quote) {
        string q = regex_escape(quote);
        list<string> lines = ();
        foreach string line in (text.split("\n")) {
            line =~ s/\r$//;
            if (!trim(line).val()) {
                continue;
            }
            # remove quoted fields so separators inside them are not counted
            line = regex_subst(line, sprintf("%s([^%s]|%s%s)*%s", q, quote, q, q, q), "", RE_Global);
            lines += line;
            if (lines.size() == SniffLines) {
                break;
            }
        }
        string best = Separators[0];
        int best_lines = 0;
        int best_count = 0;
        foreach string sep in (Separators) {
            hash<string, int> freq = {};
            foreach string line in (lines) {
                int count = countOccurrences(line, sep);
                if (count) {
                    ++freq{count.toString()};
                }
            }
            # the most common non-zero count and the number of lines with it
            foreach hash<auto> i in (freq.pairIterator()) {
                int count = i.key.toInt();
                if (i.value > best_lines || (i.value == best_lines && count > best_count)) {
                    best = sep;
                    best_lines = i.value;
                    best_count = count;
                }
            }
        }
        return best;
    }
}
}
