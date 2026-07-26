# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore NdjsonWriter class definition

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
#! Writer for creating NDJSON (Newline Delimited JSON) output
/** This class provides methods for writing JSON values in NDJSON format
    to an output stream.

    @par Example
    @code{.py}
    StringOutputStream sos();
    NdjsonWriter writer(sos);
    writer.write({"name": "Alice", "age": 30});
    writer.write({"name": "Bob", "age": 25});
    string result = sos.getData();
    # result is: {"name":"Alice","age":30}\n{"name":"Bob","age":25}\n
    @endcode
*/
public class NdjsonWriter {
    private {
        #! The output stream
        Qore::StreamWriter writer;

        #! Line ending string
        string line_ending;

        #! Writer options
        hash<NdjsonWriterOptions> opts;

        #! Number of records written
        int records_written = 0;
    }

    #! Creates the writer with an output stream
    /** @param output_stream the output stream to write to
        @param options writer options
    */
    constructor(Qore::OutputStream output_stream, *hash<NdjsonWriterOptions> options) {
        opts = options ?? <NdjsonWriterOptions>{};
        writer = new StreamWriter(output_stream, opts.encoding);
        line_ending = opts.use_crlf ? "\r\n" : "\n";
    }

    #! Writes a JSON value as a line
    /** @param value the value to serialize as JSON and write

        @return the number of bytes written
    */
    int write(auto value) {
        string json = make_json(value);
        string line = json + line_ending;
        writer.write(line);
        records_written++;
        return line.size();
    }

    #! Writes multiple JSON values as lines
    /** @param values the list of values to write

        @return the total number of bytes written
    */
    int writeAll(list<auto> values) {
        int total = 0;
        foreach auto value in (values) {
            total += write(value);
        }
        return total;
    }

    #! Flushes the output stream
    flush() {
        # StreamWriter doesn't have flush, but we keep the method for consistency
    }

    #! Returns the number of records written
    int getRecordsWritten() {
        return records_written;
    }
}

#! Creates NDJSON output as a string
/** This is a convenience class that writes NDJSON to an internal string buffer.

    @par Example
    @code{.py}
    NdjsonStringWriter writer();
    writer.write({"id": 1});
    writer.write({"id": 2});
    string result = writer.getString();
    @endcode
*/
public class NdjsonStringWriter inherits NdjsonWriter {
    private {
        #! The string output stream
        StringOutputStream sos;
    }

    #! Creates the string writer
    /** @param options writer options
    */
    constructor(*hash<NdjsonWriterOptions> options) : NdjsonWriter(sos = new StringOutputStream(), options) {
    }

    #! Returns the NDJSON string
    string getString() {
        return sos.getData();
    }

    #! Clears the internal buffer and resets the writer
    clear() {
        sos = new StringOutputStream();
    }
}
}
