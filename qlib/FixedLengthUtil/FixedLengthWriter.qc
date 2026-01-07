# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthWriter.qc Qore user module for working with files with fixed lines length

/*  FixedLengthWriter Copyright 2015 - 2024 Qore Technologies, s.r.o.

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

%modern

#! The FixedLengthUtil namespace contains all the definitions in the FixedLengthUtil module
public namespace FixedLengthUtil {
#! Writer for fixed-length line output data
/** @par Example:
    @code{.py}
#!/usr/bin/env qore

%modern

%requires FixedLengthUtil

list<auto> data = (
    {"type": "type1", "record": {"col1": 11111, "col2": "bb"}},
    {"type": "type2", "record": {"col3": "c", "col4": "ddd", "col5": "31122014"}},
    {"type": "type1", "record": {"col1": 22222, "col2": "gg"}},
);

hash<auto> specs = {
    "type1": {
        "col1": {"length": 5, "type": "int"},
        "col2": {"length": 2, "type": "string"},
    },
    "type2": {
        "col3": {"length": 1, "type": "string"},
        "col4": {"length": 3, "type": "string"},
        "col5": {"length": 8, "type": "date", "format": "DDMMYYYY"},
    },
};

hash<auto> global_options = {
    "eol": "\n",
};

FileOutputStream output("output.txt");
FixedLengthWriter w(output, specs, global_options);
w.write(data);
    @endcode
*/
public class FixedLengthWriter inherits FixedLengthUtil::FixedLengthAbstractWriter {
    private {
        #! the output stream for the fixed-length data
        StreamWriter output;
    }

    #! Instantiates the FixedLengthWriter object
    /**
        @param output the @ref Qore::OutputStream "OutputStream" for the data generated
        @param specs Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for valid values
    */
    constructor(Qore::OutputStream output, hash<auto> specs, *hash<auto> opts) : FixedLengthUtil::FixedLengthAbstractWriter(specs, opts)  {
        self.output = new StreamWriter(output, opts.encoding);
    }

    #! Renders a single line for a single input record hash to the output stream
    /** @par Example:
        @code{.py}
const Spec = (
    "header": (
        "flow_type": ("length": 3, "type": "string"),
        "record_type": ("length": 3, "type": "int", "padding_char": "0"),
        "number_of_records": ("length": 8, "type": "int", "padding_char": "0"),
    ),
);
hash hh = (
    "type": "header",
    "record": (
        "flow_type": "WTS",
        "record_type": "950",
        "number_of_records": 1,
     ),
);
FixedLengthWriter w(output, Spec);
w.write(hh);
        @endcode

        @param line A hash representing input data to be writen to the output stream; the hash must have the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    write(hash<auto> line) {
        output.print(formatLine(line) + m_opts.eol);
    }

    #! iterates the input records and writes rendered versions of all input records to the output stream
    /** @par Example:
        @code{.py}
FixedLengthWriter w(output, Spec);
w.write(i);
        @endcode

        @param lines A list of input records; each list value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    write(list<auto> lines) {
        map output.print(formatLine($1) + m_opts.eol), lines;
    }

    #! iterates the input records and writes rendered versions of all input records to the output stream
    /** @par Example:
        @code{.py}
FixedLengthWriter w(output, Spec);
w.write(i);
        @endcode

        @param lines An iterator to stream input records; each iterator value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    write(Qore::AbstractIterator lines) {
        map output.print(formatLine($1) + m_opts.eol), lines;
    }
}
}