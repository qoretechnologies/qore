# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CsvByteOrderMark class definition

/*  CsvByteOrderMark.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Byte order mark handling shared by the CSV readers and the CSV profiler (module-private)
/** A byte order mark (U+FEFF) at the start of a text file marks its encoding; it is not part of the content.  The
    profiler detects it to find the encoding, and the readers skip it so that the first header or value of the file
    does not start with it.
*/
class CsvByteOrderMark {
    public {
        #! The UTF-8 byte order mark
        const Utf8 = <efbbbf>;

        #! The UTF-16 little-endian byte order mark
        const Utf16Le = <fffe>;

        #! The UTF-16 big-endian byte order mark
        const Utf16Be = <feff>;

        #! The byte order mark as a character
        const Character = 0xfeff;
    }

    #! Returns the encoding given by the byte order mark at the start of the data
    /** @param data the start of the data; at least the first three bytes, if the data has them

        @return \c "UTF-8", \c "UTF-16LE", or \c "UTF-16BE", or @ref nothing if the data does not start with a byte
        order mark
    */
    static *string detect(binary data) {
        if (data.substr(0, Utf8.size()) == Utf8) {
            return "UTF-8";
        }
        binary head = data.substr(0, Utf16Le.size());
        if (head == Utf16Le) {
            return "UTF-16LE";
        }
        if (head == Utf16Be) {
            return "UTF-16BE";
        }
    }

    #! Returns the string without a byte order mark at its start
    /** @param data the text to check

        @return \a data without a leading U+FEFF character, otherwise \a data unchanged
    */
    static string strip(string data) {
        if (data.val() && data.getUnicode(0) == Character) {
            return data.substr(1);
        }
        return data;
    }

    #! Returns a line iterator for the stream that skips a UTF-8 byte order mark at the start of the stream
    /** @param input the input stream
        @param encoding the character encoding of the stream; if not given, the default character encoding is
        assumed
        @param eol the line separator; if not given, it is detected

        @return a line iterator for the stream; when the stream is read as UTF-8, a UTF-8 byte order mark at its start
        is skipped; a UTF-16 byte order mark is removed by the line iterator itself, and in any other encoding the
        bytes of a UTF-8 byte order mark are characters of the content
    */
    static InputStreamLineIterator getLineIterator(InputStream input, *string encoding, *string eol) {
        string enc = encoding ?? get_default_encoding();
        if (isUtf8(enc)) {
            input = new CsvUtf8BomSkippingInputStream(input);
        }
        return new InputStreamLineIterator(input, enc, eol);
    }

    #! Returns a line iterator for the string that skips a byte order mark at its start
    /** @param data the text to iterate
        @param eol the line separator; if not given, it is detected

        @return a line iterator for \a data without a leading U+FEFF character
    */
    static DataLineIterator getLineIterator(string data, *string eol) {
        return new DataLineIterator(strip(data), eol);
    }

    #! Returns @ref True if the character encoding is UTF-8 under any of its names
    private static bool isUtf8(string encoding) {
        return binary_to_string(binary(), encoding).encoding() == "UTF-8";
    }
}

#! An input stream that skips a UTF-8 byte order mark at the start of another stream (module-private)
/** The start of the wrapped stream is read when the first byte is read or peeked; if it is not a byte order mark, it
    is returned as read.
*/
class CsvUtf8BomSkippingInputStream inherits InputStream {
    private {
        #! The wrapped stream
        InputStream input;

        #! The bytes read from the start of the wrapped stream that are not a byte order mark and not yet returned
        binary pending = binary();

        #! @ref True once the start of the wrapped stream has been checked
        bool checked = False;
    }

    #! Creates the stream
    /** @param input the stream to read
    */
    constructor(InputStream input) {
        self.input = input;
    }

    #! Reads bytes from the stream, without a UTF-8 byte order mark at its start
    /** @param limit the maximum number of bytes to read

        @return the bytes read, or @ref nothing at the end of the stream
    */
    *binary read(int limit) {
        checkStart();
        if (pending) {
            if (limit <= 0) {
                return;
            }
            binary rv = pending.substr(0, limit);
            pending = pending.substr(rv.size());
            return rv;
        }
        return input.read(limit);
    }

    #! Returns the next byte without reading it, or -1 at the end of the stream
    int peek() {
        checkStart();
        return pending ? pending[0] : input.peek();
    }

    #! Reads the start of the wrapped stream once and skips a UTF-8 byte order mark there
    private checkStart() {
        if (checked) {
            return;
        }
        # bytes read are kept in pending as they arrive, so a failed read loses none of them
        while (pending.size() < CsvByteOrderMark::Utf8.size()) {
            *binary data = input.read(CsvByteOrderMark::Utf8.size() - pending.size());
            if (!data) {
                break;
            }
            pending += data;
        }
        checked = True;
        if (pending == CsvByteOrderMark::Utf8) {
            pending = binary();
        }
        @assert(pending.size() <= CsvByteOrderMark::Utf8.size());
    }
}
}
