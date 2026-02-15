# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthAbstractIterator.qc Qore user module for working with files with fixed lines length

/*  FixedLengthAbstractIterator Copyright 2015 - 2024 Qore Technologies, s.r.o.

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
#! Structured line iterator for abstract data allowing efficient "pipelined" processing
public class FixedLengthAbstractIterator inherits FixedLengthBase, Qore::AbstractIterator {
    private {
        # rules keyed by field length then by record name
        hash<string, hash<string, list<hash<auto>>>> m_resolve_by_rule;
        # all rules; keyed by record name
        hash<string, list<hash<auto>>> m_resolve_by_rule_all;
        string m_state;
        #! hash of type without rule, i.e.potentially conflicting records; key = record length, value = list of no-rule type names
        hash m_resolve_by_length;
        AbstractLineIterator lineIterator;
    }

    #! Instantiates the FixedLengthAbstractIterator object
    /**
        @param li source line iterator
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for more information

        @throw FIXED-LENGTH-UTIL-INVALID-SPEC invalid record specification; invalid type or missing field length
    */
    constructor(AbstractLineIterator li, hash<auto> spec, *hash<auto> opts)
        : FixedLengthBase(spec, FixedLengthAbstractIterator::checkOptions(opts)) {
        lineIterator = li;

        # Determine line lengths of each record, for use by the default identifyType() method. We do this by building a reverse length -> type table.
        m_resolve_by_rule = {};
        foreach string k in (keys m_specs) {
            if (m_specs{k}.typeCode() != NT_HASH)
                throw "FIXED-LENGTH-UTIL-INVALID-SPEC", sprintf("expecting a record description hash assigned to record key %y; got type %y instead (value: %y)", k, m_specs{k}.type(), m_specs{k});
            int len = 0;
            list<hash<auto>> rec_rule = ();
            foreach string c in (keys m_specs{k}) {
                if (!m_specs{k}{c}.length)
                    throw "FIXED-LENGTH-UTIL-INVALID-SPEC", sprintf("Length missing for field %y", c);
                hash<auto> fld_rule;
                if (exists m_specs{k}{c}.regex) {
                    if (exists m_specs{k}{c}.value) {
                        throw "FIXED-LENGTH-UTIL-INVALID-SPEC", sprintf("Both value and regex used in field rule for field %y, record: %y", c, k);
                    }
                    fld_rule.regex = m_specs{k}{c}.regex;
                } else if (exists m_specs{k}{c}.value) {
                    fld_rule.value = m_specs{k}{c}.value;
                }
                if (fld_rule) {
                    fld_rule.pos = len;
                    fld_rule.len = m_specs{k}{c}.length;
                    rec_rule += fld_rule;
                }
                len += m_specs{k}{c}.length;
            }
            if (rec_rule) {
                # filter specified
                m_resolve_by_rule{len}{k} = rec_rule;
                m_resolve_by_rule_all{k} = rec_rule;
            } else {
                # no spec filter, check potential conflict later
                if (!m_resolve_by_length{len}) {
                    m_resolve_by_length{len} = ();
                }
                m_resolve_by_length{len} += (k);
            }
        }
    }

    bool valid() {
        return lineIterator.valid();
    }

    #! Moves the current line / record position to the next line / record; returns @ref False if there are no more lines to iterate
    /** This method will return @ref True again after it returns @ref False once if the file being iterated has data that can be iterated, otherwise it will always return @ref False. The iterator object should not be used to retrieve a value after this method returns @ref False.
        @return @ref False if there are no lines / records to iterate (in which case the iterator object is invalid and should not be used); @ref True if successful (meaning that the iterator object is valid)

        @note that empty lines are ignored if "ignore_empty" option is in effect
     */
    bool next() {
        auto status = lineIterator.next();
        if (m_opts.ignore_empty) {
            while (status && lineIterator.getValue() == "") { # skip empty lines
                status = lineIterator.next();
            }
        }
        return status;
    }

    string getLine() {
        return lineIterator.getValue();
    }

    #! Returns the current record as a hash
    /** @par Example:
        @code{.py}
my hash $h = $i.getValue();
        @endcode

        @return The current record as a hash with the following keys:
        - \c "type": a string giving the record type name
        - \c "record": a hash giving the parsed record data
    */
    *hash<auto> getValue() {
        *string line = lineIterator.getValue();

        if (!line)
            return;

        hash<auto> result = {};

        string type = identifyType(line);
        result.type = type;
        if (!checkTransition(m_state, type)) {
            throw "FIXED-LENGTH-UTIL-INVALID-TRANSITION", sprintf("record %y cannot follow record %y for line %y", (type ?? "<START>"), (type ?? "<START>"), line);
        }
        m_state = type;
        int pos = 0;
        foreach string col in (keys m_specs{type}) {
            hash field = m_specs{type}{col};
            if (!field.timezone) {
                field.timezone = m_opts.timezone;
            }
            result.record{col} = transform(line.substr(pos, field.length), field + {'name': col});
            pos += field.length;
        }
        return result;
    }

    int index() {
        return lineIterator.index();
    }

    #! parses the input value based on global configuration and the current field definition
    auto transform(auto value, hash<auto> type) {
        switch (type.type) {
            case "int": {
                return int(value);
            }

            case NOTHING:
            case "string": {
                if (value.typeCode() == NT_STRING) {
                    trim value;
                    # Convert tabs to spaces
                    if (exists type.tab2space || exists m_opts.tab2space) {
                        value = replace (value, "\t", strmul(' ', type.tab2space ?? m_opts.tab2space));
                    }
                    # Validate and truncate if configured to do so
                    if ((int len = value.length()) > type.length) {
                        if (!(type.truncate ?? m_opts.truncate))
                            throw "FIELD-VALUE-ERROR", sprintf("Value %y (len %d) too large to pack into field: %s (len %d)", value, len, type.name, type.length);
                        splice value, type.length;
                    }
                    return m_opts.encoding && value.encoding() != m_opts.encoding ? convert_encoding(value, m_opts.encoding) : value;
                }
                return string(value, m_opts.encoding);
            }

            case "float": {
                if (value.typeCode() == NT_STRING) {
                    *string nf = type.format ? type.format: m_opts.number_format;
                    return nf ? parse_float(value, nf) : float(value);
                }
                return float(value);
            }
            case "number": {
                if (value.typeCode() == NT_STRING) {
                    *string nf = type.format ? type.format: m_opts.number_format;
                    return nf ? parse_number(value, nf) : number(value);
                }
                return number(value);
            }
            case "date": {
                if (value.empty()) {
                    return 1970-01-01Z;
                }
                TimeZone tz;
                if (type.timezone)
                    tz = type.timezone.type() == "object" ? type.timezone: new TimeZone(type.timezone);
                else if (m_opts.timezone)
                    tz = m_opts.timezone;

                int t = value.typeCode();
                if (t == NT_STRING) {
                    *string fmt = type.format;
                    if (!fmt)
                        fmt = m_opts.date_format;

                    try {
                        return tz ? tz.date(value, fmt) : date(value, fmt);
                    } catch (hash<ExceptionInfo> ex) {
                        throw ex.err, ex.desc, ("value": value, "fmt": fmt);
                    }
                }
                try {
                    if (t != NT_DATE && tz)
                        value = date(value);
                    return tz ? tz.date(value) : date(value);
                } catch (hash<ExceptionInfo> ex) {
                    throw ex.err, ex.desc, ("value": value);
                }
            }
            default: {
                throw "FIELD-TYPE-ERROR", sprintf("output type %y not supported", type.type);
            }
        }
    }

    #! Validate and prepare global fixed-length options
    /**
        @param opts Options passed during class instantiation.
    */
    private static *hash<auto> checkOptions(*hash<auto> opts) {
        string errname = 'RECORD-ITERATOR-ERROR';

        *hash<auto> new_opts += opts;
        foreach hash<auto> i in (opts.pairIterator()) {
            switch (i.key) {
                case "eol": {
                    if (i.value.typeCode() != NT_STRING) {
                        throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                    }
                    if (!EOLS{i.value}) {
                        throw errname, sprintf("expecting a known end of line value to option %y; got %y instead", i.key, i.value);
                    }
                    new_opts.eol = i.value;
                    break;
                }
                case "timezone": {
                    new_opts.timezone = new TimeZone(new_opts.timezone);
                    break;
                }
                case "ignore_empty": {
                    new_opts.ignore_empty = boolean(new_opts.ignore_empty);
                    break;
                }
                case "encoding": {
                    if (i.value.typeCode() != NT_STRING)
                        throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                    new_opts.encoding = i.value;
                    # ensure that the default value for the encoding is used (to handle case + encoding aliases); also validates that
                    # the encoding is known and supported
                    string str = convert_encoding(new_opts.encoding, new_opts.encoding);
                    new_opts.encoding = str.encoding();

                    break;
                }
                case "number_format": {
                    if (i.value.typeCode() != NT_STRING)
                        throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                    # TODO: check here
                    break;
                }
                case "date_format": {
                    if (i.value.typeCode() != NT_STRING)
                        throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                    # TODO: check here
                    break;
                }
                case "tab2space": {
                    if (i.value.typeCode() != NT_INT || i.value < 0)
                        throw errname, sprintf("expecting a non-negative int value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                    break;
                }
            }
        }

        return new_opts;
    }

    #! Identify a fixed-length line type using identifyTypeImpl(), that may be overridden if necessary.
    /**
        @param input_line Input line to be identified

        @return the name of the record corresponding to the input line

        @throw FIXED-LENGTH-UTIL-NON-MATCHING-TYPE input line cannot be matched to a known record
    */
    string identifyType(string input_line) {
        *string type = identifyTypeImpl(input_line);
        if (!type)
            throw "FIXED-LENGTH-UTIL-NON-MATCHING-TYPE", sprintf("The input line could not be identified: %y", input_line);
        if (!m_specs.hasKey(type))
            throw "FIXED-LENGTH-UTIL-NON-MATCHING-TYPE", sprintf("Line identified to be of type %y that is not present in the spec: %y", type, input_line);
        return type;
    }

    #! Identify a fixed-length line type, given the raw line string. This method performs a lookup to a precalculated table based on line lengths (see constructor()). In case different criteria are needed, eg. when two line types in a spec have the same length and no unique resolving rule are specified, this method needs to be overridden, and will throw an exception, because the precalculated mapping will be empty.
    /**
        @param input_line Input line to be identified

        @return the record name or @ref nothing if the input cannot be matched

        @throw FIXED-LENGTH-UTIL-NON-MATCHING-TYPE input line cannot be matched to a known record or the input line's
        length does not match the expected length
    */
    *string identifyTypeImpl(string input_line) {
        *string rv;
        int len = input_line.size();
        if (!m_resolve_by_rule{len} && !m_resolve_by_length{len} && !m_resolve_by_rule_all) {
            throw "FIXED-LENGTH-UTIL-NON-MATCHING-TYPE", sprintf("Line of unexpected length %d found; known "
                "lengths: %y (record: %s)", len, (map $1.toInt(),
                sort(keys (m_resolve_by_rule + m_resolve_by_length))), input_line);
        }

        if (m_resolve_by_rule{len}) {
            # try match type by filter spec
            foreach string k in (keys m_resolve_by_rule{len}) {
                if (identifyRecord(m_resolve_by_rule{len}{k}, input_line)) {
                    rv = k;
                    break;
                }
            }
        }

        if (!exists rv && m_resolve_by_rule_all) {
            foreach hash<auto> i in (m_resolve_by_rule_all.pairIterator()) {
                if (identifyRecord(i.value, input_line)) {
                    rv = i.key;
                    break;
                }
            }
        }

        if (!rv && m_resolve_by_length{len}) {
            if (m_resolve_by_length{len}.size() == 1) {
                rv = m_resolve_by_length{len}[0];
            }
            if (m_resolve_by_length{len}.size() > 1)
                throw "FIXED-LENGTH-UTIL-NON-MATCHING-TYPE", sprintf("Line with byte length %d was not automatically matched since the following records have this length: %y; you need to provide your own identifyTypeImpl() method or specify rules (record: %s)", len, m_resolve_by_length{len}, input_line);
        }
        return rv;
    }

    #! Attempts to identify a single record
    bool identifyRecord(list<auto> rec_rule, string input_line) {
        foreach hash fld_rule in (rec_rule) {
            if (exists fld_rule.regex) {
                # we do not limit regex to field length to support multi field regex
                if (!input_line.substr(fld_rule.pos).regex(fld_rule.regex)) {
                    return False;
                }
            } else {
                string val = input_line.substr(fld_rule.pos, fld_rule.len);
                if (fld_rule.value.typeCode() == NT_INT) {
                    # special care about int type,  0 == "000" should evaluate as True
                    if (!val.intp() || val.toInt() != fld_rule.value) {
                        return False;
                    }
                } else {
                    if (val != fld_rule.value) {
                        return False;
                    }
                }
            }
        }
        return True;
    }

    #! A verification function to be called for each line. This method can be overridden to achieve a begin-to-end validation of the whole input file.
    /**
        @param from Type of previous line being processed
        @param to Type of the current line being processed
    */
    bool checkTransition(*string from, *string to) {
        return True;
    }
}
}