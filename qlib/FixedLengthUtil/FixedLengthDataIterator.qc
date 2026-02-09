# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthDataIterator.qc Qore user module for working with files with fixed lines length

/*  FixedLengthDataIterator Copyright 2015 - 2024 Qore Technologies, s.r.o.

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

#! The FixedLengthUtil namespace contains all the definitions in the FixedLengthUtil module
public namespace FixedLengthUtil {
#! Structured line iterator for fixed-length line strings allowing efficient "pipelined" processing.
/**
    @par Example:
    @code{.py}
string data = "00001AV\n00002BN\00003CZ\n";

hash<auto> specs = {
    "type1": {
        "col1": {"length": 5, "type": "int"},
        "col2": {"length": 2, "type": "string"},
    },
    "type2": {
        "col3": {"length": 1, "type": "string"},
        "col4": {"length": 3, "type": "string"},
        "col5": {"length": 8, "type": "date", "format": "DDMMYYYY", "timezone": "Europe/Prague"},
    },
};

hash<auto> global_options = {
    "encoding"    : "UTF-8",
    "eol"         : "\n",
    "ignore_empty": True,
    "timezone"    : "Europe/Prague", # used if not overridden in a date field specification
};

FixedLengthDataIterator i(data, specs, global_options);
while (i.next()) {
    operation_with_hash(i.getValue())
}
    @endcode

    @see
    - @ref FixedLengthUtil::FixedLengthIterator "FixedLengthIterator" for a stream-based class providing the same functionality as this class in a more generic way
*/
public class FixedLengthDataIterator inherits FixedLengthUtil::FixedLengthAbstractIterator {
    #! Instantiates the FixedLengthDataIterator object
    /** @par Example:
        @code{.py}
hash<auto> specs = {
    "type1": {
        "col1": {"length": 5, "type": "int"},
        "col2": {"length": 2, "type": "string"},
    },
    "type2": {
        "col3": {"length": 1, "type": "string"},
        "col4": {"length": 3, "type": "string"},
        "col5": {"length": 8, "type": "date", "format": "DDMMYYYY", "timezone": "Europe/Prague"},
    },
};

hash<auto> global_options = {
    "encoding"    : "UTF-8",
    "eol"         : "\n",
    "ignore_empty": True,
    "timezone"    : "Europe/Prague", # used if not overridden in a date field specification
};

FixedLengthDataIterator i(string, specs, global_options);
while (i.next()) {
    operation_with_hash(i.getValue())
}
        @endcode

        @param data The input string to process
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for valid values
    */
    constructor(string data, hash<auto> spec, *hash<auto> opts)
        : FixedLengthAbstractIterator(new InputStreamLineIterator(new StringInputStream(data), data.encoding(),
            opts.eol, opts.trim), spec, opts) {
    }
}
}
