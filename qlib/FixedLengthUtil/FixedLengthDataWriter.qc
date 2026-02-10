# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthDataWriter.qc Qore user module for working with files with fixed lines length

/*  FixedLengthDataWriter Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
#! Writer class for fixed-length line string data
/** @par Example:
    @code{.py}
const Spec = {
    "header": {
        "flow_type": {"length": 3, "type": "string", "value": "001"},
        "record_type": {"length": 3, "type": "int", "padding_char": "0"},
        "number_of_records": {"length": 8, "type": "int", "padding_char": "0"},
    },
    "line": {
        "flow_type": {"length": 3, "type": "string"},
        "record_type": {"length": 3, "type": "int", "padding_char": "0"},
        "processing_id": {"length": 10, "type": "int", "padding_char": "0"},
        "processing_name": {"length": 10, "type": "string"},
        "po_number": {"length": 10, "type": "int", "padding_char": "0"},
    },
    "trailer": {
        "flow_type": {"length": 3, "type": "string", "value": "003"},
        "record_type": {"length": 3, "type": "int", "padding_char": "0"},
        "number_of_records": {"length": 8, "type": "int", "padding_char": "0"},
    },
};
FixedLengthDataWriter w(Spec);
AbstractIterator i = get_record_iterator();
string data = w.write(i);
    @endcode

    @see
    - @ref FixedLengthUtil::FixedLengthWriter "FixedLengthWriter" for a stream-based class providing the same functionality as this class in a more generic way
*/
public class FixedLengthDataWriter inherits FixedLengthUtil::FixedLengthAbstractWriter {
    #! Instantiates the FixedLengthDataWriter object.
    /**
        @param specs Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global formatting options; see @ref fixedlengthglobals for valid values
    */
    constructor(hash<auto> specs, *hash<auto> opts) : FixedLengthUtil::FixedLengthAbstractWriter(specs, opts) {
    }

    #! iterates the input records and returns a string corresponding to the rendered versions of all input records
    /** @par Example:
        @code{.py}
FixedLengthDataWriter w(Spec);
string file = w.write(input_list);
        @endcode

        @param lines An iterator to stream input records; each iterator value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @return the rendered string for all input records

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    string write(list<auto> lines) {
        string result = "";
        string eol = m_opts.eol;
        map result += formatLine($1) + eol, lines;
        return result;
    }

    #! Return a single line for a single input record hash
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
FixedLengthDataWriter w(Spec);
string line = w.write(hh);
        @endcode

        @param line A hash representing input data to be rendered as a string; the hash must have the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @return a single line for a single input record hash

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    string write(hash<auto> line) {
        return formatLine(line) + m_opts.eol;
    }

    #! iterates the input records and returns a string corresponding to the rendered versions of all input records
    /** @par Example:
        @code{.py}
FixedLengthDataWriter w(Spec);
string file = w.write(i);
        @endcode

        @param lines An iterator to stream input records; each iterator value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @return the rendered string for all input records

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    string write(Qore::AbstractIterator lines) {
        string result = "";
        string eol = m_opts.eol;
        map result += formatLine($1) + eol, lines;
        return result;
    }
}
}
