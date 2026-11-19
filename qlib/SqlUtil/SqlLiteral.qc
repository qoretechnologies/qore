# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SqlLiteral.qc Checks of the strings given to SQL string literals

/** SqlLiteral.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace SqlUtil {
#! Checks that a string given to an SQL string literal is valid in its encoding
/** A string literal is made by doubling each quote character in the string; a string that is not valid in its
    encoding cannot be processed reliably, so it is refused instead of being written into the SQL, whatever the
    driver and however it quotes the string

    @param value the string for the literal

    @return \a value

    @throw VALUE-ERROR \a value is not valid in its encoding

    @since SqlUtil 3.11
*/
public string sub sql_literal_check_encoding(string value) {
    if (!value.isEncodingValid()) {
        throw "VALUE-ERROR", sprintf("a string of %d bytes given to an SQL string literal is not valid in its encoding "
            "(%s)", value.size(), value.encoding());
    }
    return value;
}
}
