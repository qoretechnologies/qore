# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore NdjsonIterator class definition

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the NdjsonDataProvider module
public namespace NdjsonDataProvider {
#! Iterator for reading NDJSON (Newline Delimited JSON) data from a string
/** This class provides an iterator interface for reading NDJSON formatted data.
    Each call to next() advances to the next JSON line, and getValue() returns
    the parsed JSON value.

    @par Example
    @code{.py}
    string ndjson = '{"id": 1}\n{"id": 2}\n{"id": 3}';
    NdjsonIterator iter(ndjson);
    while (iter.next()) {
        printf("Value: %y\n", iter.getValue());
    }
    @endcode
*/
public class NdjsonIterator inherits AbstractIterator {
    private {
        #! The list of lines to iterate over
        list<string> lines;

        #! Current line index
        int index = -1;

        #! Current parsed value
        auto current_value;

        #! Current line number (1-based)
        int line_number = 0;

        #! Options for the iterator
        hash<NdjsonIteratorOptions> opts;

        #! Valid flag
        bool valid = False;
    }

    #! Creates the iterator from a string
    /** @param ndjson_string the NDJSON formatted string
        @param options iterator options

        @throw NDJSON-LINE-TOO-LONG if a line exceeds max_line_length
    */
    constructor(string ndjson_string, *hash<NdjsonIteratorOptions> options) {
        opts = options ?? <NdjsonIteratorOptions>{};
        # Split by newlines, handling both LF and CRLF
        lines = ndjson_string.split("\n");
        # Check line lengths
        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];
            # Remove trailing CR if present (CRLF case)
            if (line.size() > 0 && line[line.size() - 1] == "\r") {
                lines[i] = line.substr(0, line.size() - 1);
                line = lines[i];
            }
            if (line.size() > opts.max_line_length) {
                throw "NDJSON-LINE-TOO-LONG", sprintf("line %d exceeds maximum length of %d bytes (actual: %d)",
                    i + 1, opts.max_line_length, line.size());
            }
        }
    }

    #! Advances to the next JSON value
    /** @return True if there is a next value, False if iteration is complete
    */
    bool next() {
        while (True) {
            index++;
            line_number++;
            if (index >= lines.size()) {
                valid = False;
                current_value = NOTHING;
                return False;
            }

            string line = lines[index];

            # Handle empty lines
            if (line.empty() || line =~ /^\s*$/) {
                if (opts.skip_empty_lines) {
                    continue;
                }
                # Treat empty line as null
                current_value = NOTHING;
                valid = True;
                return True;
            }

            # Parse JSON
            try {
                current_value = parse_json(line);
                valid = True;
                return True;
            } catch (hash<ExceptionInfo> ex) {
                if (opts.skip_invalid_json) {
                    continue;
                }
                throw "NDJSON-PARSE-ERROR", sprintf("failed to parse JSON at line %d: %s", line_number, ex.desc), ex;
            }
        }
    }

    #! Returns the current JSON value
    /** @return the parsed JSON value

        @throw ITERATOR-ERROR if the iterator is not valid
    */
    auto getValue() {
        if (!valid) {
            throw "ITERATOR-ERROR", "iterator is not valid; call next() first";
        }
        return current_value;
    }

    #! Returns True if the iterator is valid
    bool valid() {
        return valid;
    }

    #! Returns the current line number (1-based)
    int getLineNumber() {
        return line_number;
    }

    #! Resets the iterator to the beginning
    reset() {
        index = -1;
        line_number = 0;
        current_value = NOTHING;
        valid = False;
    }
}

#! Iterator for reading NDJSON data from an input stream
/** This class provides an iterator interface for reading NDJSON formatted data
    from an input stream, enabling memory-efficient processing of large files.

    @par Example
    @code{.py}
    FileInputStream fis("data.ndjson");
    InputStreamNdjsonIterator iter(fis);
    while (iter.next()) {
        printf("Value: %y\n", iter.getValue());
    }
    @endcode
*/
public class InputStreamNdjsonIterator inherits AbstractIterator {
    private {
        #! The input stream
        Qore::StreamReader reader;

        #! Current parsed value
        auto current_value;

        #! Current line number (1-based)
        int line_number = 0;

        #! Options for the iterator
        hash<NdjsonIteratorOptions> opts;

        #! Valid flag
        bool valid = False;

        #! End of stream flag
        bool eos = False;
    }

    #! Creates the iterator from an input stream
    /** @param input_stream the input stream to read from
        @param options iterator options
    */
    constructor(Qore::InputStream input_stream, *hash<NdjsonIteratorOptions> options) {
        opts = options ?? <NdjsonIteratorOptions>{};
        reader = new StreamReader(input_stream, opts.encoding);
    }

    #! Advances to the next JSON value
    /** @return True if there is a next value, False if iteration is complete

        @throw NDJSON-LINE-TOO-LONG if a line exceeds max_line_length
    */
    bool next() {
        if (eos) {
            return False;
        }

        while (True) {
            line_number++;

            *string line = reader.readLine();

            if (!exists line) {
                eos = True;
                valid = False;
                current_value = NOTHING;
                return False;
            }

            # Check line length
            if (line.size() > opts.max_line_length) {
                throw "NDJSON-LINE-TOO-LONG", sprintf("line %d exceeds maximum length of %d bytes (actual: %d)",
                    line_number, opts.max_line_length, line.size());
            }

            # Remove trailing CR if present (CRLF case)
            if (line.size() > 0 && line[line.size() - 1] == "\r") {
                line = line.substr(0, line.size() - 1);
            }

            # Handle empty lines
            if (line.empty() || line =~ /^\s*$/) {
                if (opts.skip_empty_lines) {
                    continue;
                }
                # Treat empty line as null
                current_value = NOTHING;
                valid = True;
                return True;
            }

            # Parse JSON
            try {
                current_value = parse_json(line);
                valid = True;
                return True;
            } catch (hash<ExceptionInfo> ex) {
                if (opts.skip_invalid_json) {
                    continue;
                }
                throw "NDJSON-PARSE-ERROR", sprintf("failed to parse JSON at line %d: %s", line_number, ex.desc), ex;
            }
        }
    }

    #! Returns the current JSON value
    /** @return the parsed JSON value

        @throw ITERATOR-ERROR if the iterator is not valid
    */
    auto getValue() {
        if (!valid) {
            throw "ITERATOR-ERROR", "iterator is not valid; call next() first";
        }
        return current_value;
    }

    #! Returns True if the iterator is valid
    bool valid() {
        return valid;
    }

    #! Returns the current line number (1-based)
    int getLineNumber() {
        return line_number;
    }
}
}
