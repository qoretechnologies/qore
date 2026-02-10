# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthAbstractWriter.qc Qore user module for working with files with fixed lines length

/*  FixedLengthAbstractWriter Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
#! Structured abstract writer for fixed-length lines
public class FixedLengthAbstractWriter inherits FixedLengthBase {
    private {
        string m_state;
        int m_linescount = 0;
    }

    #! creates the FixedLengthAbstractWriter object
    /**
        @param specs Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for valid values
    */
    constructor(hash<auto> specs, *hash<auto> opts)
        : FixedLengthBase(specs, FixedLengthAbstractWriter::checkOptions(opts)) {
        # validate padding options
        HashIterator hit(specs);
        while (hit.next()) {
            HashIterator cit(hit.getValue());
            while (cit.next()) {
                if (cit.getValue(){"padding"}) {
                    if (cit.getValue(){"padding"} != "left" && cit.getValue(){"padding"} != "right")
                        throw "FIXED-LENGTH-UTIL-PADDING-ERROR", sprintf("record type: %s; field: %s; Key: padding; value must be 'left' or 'right'. Got: %s",
                                                                         hit.getKey(), cit.getKey(), cit.getValue(){"padding"});
                }
                if (cit.getValue(){"padding_char"} && cit.getValue(){"padding_char"}.length() != 1) {
                    throw "FIXED-LENGTH-UTIL-PADDING-ERROR", sprintf("record type: %s; field: %s; Key: padding; value must have a character length of 1; got: %d instead",
                                                                     hit.getKey(), cit.getKey(), cit.getValue(){"padding_char"}.length());
                }
            }
        }
    }

    private static *hash<auto> checkOptions(*hash<auto> opts) {
        string errname = 'RECORD-WRITER-ERROR';
        hash<auto> new_opts += opts;
        if (exists new_opts.eol) {
            if (new_opts.eol.typeCode() != NT_STRING)
                throw errname, sprintf("expecting a string value to option \"eol\"; got %y (type %s) instead", new_opts.eol, new_opts.eol.type());
            if (!EOLS{new_opts.eol})
                throw errname, sprintf("expecting a known end of line value to option \"eol\"; got %y instead", new_opts.eol);
        } else {
            new_opts.eol = "\n";
        }

        if (exists new_opts.tab2space && (new_opts.tab2space.typeCode() != NT_INT || new_opts.tab2space < 0))
            throw errname, sprintf("expecting a non-negative int value to option \"tab2space\"; got %y (type %s) instead", new_opts.tab2space, new_opts.tab2space.type());

        return new_opts;
    }

    #! get processed lines count
    int linesCount() {
        return m_linescount;
    }

    #! Formats a single line from a hash describing the record type and its contents
    /**
        @param line a hash describing the record to be formatted; the hash must have the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-LINE-DATA line argument missing either \a type or \a record keys
        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    string formatLine(hash<auto> line) {
        if (!line.type)
            throw "INVALID-LINE-DATA", sprintf("invalid line data; missing \"type\" key; got keys: %y", line.keys());
        if (!line.record)
            throw "INVALID-LINE-DATA", sprintf("invalid line data; missing \"record\" key; got keys: %y", line.keys());
        string type = line.type;
        if (!m_specs{type})
            throw "INVALID-RECORD", sprintf("unknown record type %y; valid records: %y", type, m_specs.keys());

        string result = "";
        foreach string field in (keys m_specs{type}) {
            # field description
            hash<auto> fh = m_specs{type}{field};

            auto value = line.record{field} ?? fh.default;  # default value from specification
            # format output
            switch (fh.type) {
                case "float":
                case "number": {
                    *string fmt = fh.format ?? m_opts.number_format;
                    if (fmt)
                        value = format_number(fmt, value);
                    else if (value.typeCode() != NT_STRING)
                        value = string(value);
                    break;
                }
                case "date": {
                    switch (value.typeCode()) {
                        case NT_DATE: {
                            *string fmt = fh.format ?? m_opts.date_format;
                            value = fmt ? value.format(fmt) : string(value);
                            break;
                        }
                        case NT_STRING:
                            break;
                        default:
                            value = string(value);
                        break;
                    }
                }
                default:
                    if (value.typeCode() != NT_STRING)
                        value = string(value);
                    break;
            }

            # Convert tabs to spaces
            if (exists fh.tab2space || exists m_opts.tab2space) {
                value = replace (value, "\t", strmul(' ', fh.tab2space ?? m_opts.tab2space));
            }

            int len = value.length();
            # Validate and truncate if configured to do so
            if (len > fh.length) {
                if (!(fh.truncate ?? m_opts.truncate))
                    throw "FIELD-INPUT-ERROR", sprintf("Value %y (len %d) too large to pack into field: %s (len %d)", value, len, field, fh.length);
                splice value, fh.length;
                len = value.length();
            }

            # do output justification
            int spacingsize = fh.length - len;
            if (spacingsize) {
                string spacing = strmul(fh.padding_char ?? " ", spacingsize);

                if (fh.padding == "left")
                    result += spacing + value;
                else if (!fh.padding && inlist (fh.type, ("int", "float", "number")))
                    if (fh.padding_char == '0' && inlist (value[0], ("-", "+")))
                        result += splice(value, 1, 0, spacing);
                    else
                        result += spacing + value;
                else
                    result += value + spacing;
            } else
                result += value;
        }

        if (!checkTransition(m_state, type)) {
            throw "RECORD-TRANSITION-ERROR", sprintf("record %y cannot follow record %y for line %y", (type ?? "<START>"), (m_state ?? "<START>"), result);
        }
        m_state = type;
        ++m_linescount;

        return result;
    }

    #! A verification function to be called for each line. This method can be overridden to achieve a begin-to-end validation of the whole input file.
    /**
        @param from Type of previous line being processed
        @param to Type of the current line being processed
    */
    bool checkTransition(*string from, *string to) {
        return True;
    }

    #! Renders a single line for a single input record hash to the output
    /** @param line A hash representing input data to be written to the output; the hash must have the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a
 type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    abstract write(hash<auto> line);

    #! iterates the input records and writes rendered versions of all input records to the output
    /** @param lines A list of input records; each list value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    abstract write(list<auto> lines);

    #! iterates the input records and writes rendered versions of all input records to the output
    /** @param lines An iterator to stream input records; each iterator value must be a hash with the following keys:
        - \c "type": a string giving the record type (must be defined in @ref fixedlengthspec given in the constructor)
        - \c "record": a hash giving the input record to be rendered (with keys as defined in the @ref fixedlengthspecrecordhash for the record identified by the \a type argument)

        @throw INVALID-RECORD record name (\a type key in the record hash) not recognized
        @throw FIELD-INPUT-ERROR the input value is too large to render into the output field
        @throw RECORD-TRANSITION-ERROR a record transition error occurred; an invalid record sequence was given in the input data
    */
    abstract write(Qore::AbstractIterator lines);
}
}