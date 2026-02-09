# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthIterator.qc Qore user module for working with files with fixed lines length

/*  FixedLengthIterator Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
#! The FixedLengthIterator class allows fixed-length data sources to be iterated on a record basis.  The source of the input data is an @ref Qore::AbstractLineIterator "AbstractLineIterator" object or an @ref Qore::InputStream "InputStream"
public class FixedLengthIterator inherits FixedLengthUtil::FixedLengthAbstractIterator {
    #! Creates the FixedLengthIterator in single-type mode with general line iterator to read and an option hash
    /** @param li line iterator of fixed-length file to read
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for more information
     */
    constructor(Qore::AbstractLineIterator li, hash<auto> spec, *hash<auto> opts)
        : FixedLengthAbstractIterator(li, spec, opts) {
    }

    #! Instantiates the FixedLengthIterator object from a @ref Qore::StringInputStream "StringInputStream"
    /** @par Example:
        @code{.py}
const Specs = {
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

const GlobalOptions = {
    "encoding"    : "UTF-8",
    "eol"         : "\n",
    "ignore_empty": True,
    "timezone"    : "Europe/Prague", # used if not overridden in a date field specification
};

StringInputStream input("00001AV\n00002BN\00003CZ\n");

FixedLengthIterator i(input, Specs, GlobalOptions);
while (i.next()) {
    operation_with_hash(i.getValue())
}
        @endcode

        @param input The @ref Qore::InputStream "InputStream" providing input data to process
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for valid values
    */
    constructor(Qore::StringInputStream input, hash<auto> spec, *hash<auto> opts)
        : FixedLengthAbstractIterator(new InputStreamLineIterator(input, input.getEncoding(), opts.eol, opts.trim), spec, opts) {
    }

    #! Instantiates the FixedLengthIterator object from an @ref Qore::InputStream "InputStream"
    /** @par Example:
        @code{.py}
const Specs = {
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

const GlobalOptions = {
    "encoding"    : "UTF-8",
    "eol"         : "\n",
    "ignore_empty": True,
    "timezone"    : "Europe/Prague", # used if not overridden in a date field specification
};

BinaryInputStream input(binary("00001AV\n00002BN\00003CZ\n"));

FixedLengthIterator i(input, "UTF-8", specs, global_options);
while (i.next()) {
    operation_with_hash(i.getValue())
}
        @endcode

        @param input The @ref Qore::InputStream "InputStream" providing input data to process
        @param encoding the encoding of the input stream
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for valid values
    */
    constructor(Qore::InputStream input, string encoding = "UTF-8", hash<auto> spec, *hash<auto> opts)
        : FixedLengthAbstractIterator(new InputStreamLineIterator(input, encoding, opts.eol, opts.trim), spec, opts) {
    }
}
}
