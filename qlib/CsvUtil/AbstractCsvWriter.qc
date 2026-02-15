# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AbstractCsvWriter class definition

/*  AbstractCsvWriter.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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

# assume local var scope, do not use "$" for vars, members, and method calls
%modern

#! the CsvUtil namespace. All classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
    #! The AbstractCsvWriter class provides a parent for all CSV writers
    /**
        Any inherited class must provide concrete implementations for the following abstract method, where the real physical write action must be implemented.

        @code{.py} private writeRawLine(list values) @endcode

        @section csvwriter_options AbstractCsvWriter Constructor Option Hash Overview
        The AbstractCsvWriter class constructor takes an optional hash with possible keys given in the following table.
        Note that key names are case-sensitive, and data types are soft (conversions are made when possible).

        <b>AbstractCsvWriter Options</b>
        |!Option|!Data Type|!Description
        |\c "block"|@ref int_type "int"|the block size when generating output based on a @ref Qore::SQL::SQLStatement "SQLStatement" source (default: \c 1000)
        |\c "date_format"|@ref string_type "string"|the default mask for date values formatting
        |\c "encoding"|@ref string_type "string"|the @ref character_encoding "character encoding" for the file (and for tagging string data read); if the value of this key is not a string then it will be ignored
        |\c "eol"|@ref string_type "string"|the end of line character(s) (default: auto-detect); if the value of this key is not a string then it will be ignored
        |\c "info_log"|@ref code_type "code"|a call reference / closure for informational logging when performing bulk generation with write(Qore::SQL::SQLStatement); must take a single string argument
        |\c "optimal_quotes"|\c bool_type "bool"|set to @ref False "False" to disable optimal quoting; when optimal quoting is disabled, all fields are quoted regardless of type or content, when it is enabled, then fields are quoted only if they require quoting (i.e. they contain a quote or separator character); the default is @ref True "True"
        |\c "quote"|@ref string_type "string"|the field quote character (default: \c '\"')
        |\c "quote_escape"|@ref string_type "string"|the escape character(s) used for \c "quote" (default: \c '\\')
        |\c "separator"|@ref string_type "string"|the string separating the fields in the file (default: \c ",")
        |\c "verify_columns"|\c bool_type "bool"|if @ref True "True" (the default is @ref False "False") then if a line is parsed with a different column count than other lines, a \c CSVFILEWRITER-DATA-ERROR exception is thrown
        |\c "write_headers"|\c bool_type "bool"|set to @ref False "False" to suppress the output of headers; the default is @ref True "True", meaning to output headers if they are present. The value is @ref False "False" for multi-type lines.

        <b>AbstractCsvIterator Single-type-only Options </b>
        |!Option|!Data Type|!Description
        |\c "datamap"|@ref hash_type "hash"|a hash mapping actual data key names to the output field names, for use in case the data field names differ; does not have to include every data or output key; keys not present will be assumed to be mapped 1:1
        |\c "fields"|@ref hash_type "hash"|describes the data to be output; see @ref abstractcsviterator_option_field_hash "option field hash" and @ref abstractcsviterator_option_field_types "option field types" for more information on the possible values of this option; if no field type is given, the default is \c "*string"
        |\c "headers"|@ref list_type "list" of @ref string_type "strings"|list of header / column names for the data iterated.
        |\c "header_reorder"|\c bool_type "bool"|if @ref True "True" (default value) then if \"headers\" are provided by options then fields are reordered to follow headers. It has effect on expected field order as passed to write function via list. If @ref False "False" then fields not yet specified are pushed at the end of field definition.

        @note the following options with dashes in their names are supported for backwards-compatibility:
        - \c "date-format"
        - \c "optimal-quotes"
        - \c "verify-columns"
        - \c "write-headers"
     */
    public class AbstractCsvWriter inherits private CsvHelper {
        private {
            #! valid options for the object (a hash for quick lookups of valid keys)
            const Options = {
                "block": C_OPT1|C_OPT2,
                "datamap": C_OPT1,
                "date_format": C_OPT1|C_OPT2,
                "date-format": C_OPT1|C_OPT2,
                "encoding": C_OPT1|C_OPT2,
                "eol": C_OPT1|C_OPT2,
                "fields": C_OPT1,
                "headers": C_OPT1,
                "header_reorder": C_OPT1,
                "info_log": C_OPT1|C_OPT2,
                "number_format": C_OPT1|C_OPT2,
                "optimal_quotes": C_OPT1|C_OPT2,
                "optimal-quotes": C_OPT1|C_OPT2,
                "quote": C_OPT1|C_OPT2,
                "quote_escape": C_OPT1|C_OPT2,
                "separator": C_OPT1|C_OPT2,
                "verify_columns": C_OPT1|C_OPT2,
                "verify-columns": C_OPT1|C_OPT2,
                "write_headers": C_OPT1|C_OPT2,
                "write-headers": C_OPT1|C_OPT2,
            };

            #! output file character encoding
            string encoding;

            #! field separator
            string separator = ",";

            #! field content delimiter
            string quote = "\"";

            #! quote escape character
            string m_quoteEscapeChar = "\\";

            #! end of line sequence
            string eol = EOL_UNIX;

            #! verify the column count for every row; if a row does not match, then throw a \c CSVFILEITERATOR-DATA-ERROR exception
            bool checkElementCounts = False;

            #! the latest line number
            int lineNo = 0;

            #! block size for bulk DML
            int block = 1000;

            #! base template for value format
            string baseTemplate;

            #! this flag determines if any stored headers are output
            bool write_headers = True;

            #! stores the optimal quotes option
            bool optimal_quotes = True;

            #! a closure/call reference for informational logging when using write(SQLStatement)
            *code<nothing(string, ...)> info_log;

            #! mapping output field by name
            hash m_out_by_name;

            #! mapping output field by index
            hash m_out_by_idx;
        }

        #! Creates the AbstractCsvWriter in single-type mode
        /**
            @param n_errname a string to construct child class error message. For example: value 'FOO' will result in exception names 'CSVFOOWRITER-ERROR'
            @param n_opts @ref csvwriter_options

            @throw CSV<errname>WRITER-ERROR in the case of incorrect options
         */
        constructor(string n_errname, *hash<auto> n_opts): CsvHelper(sprintf("CSV%sWRITER-ERROR", n_errname)) {
            processCommonOptions(n_opts, C_OPT1);

            # set headers automatically from field names if not set
            if (n_opts.fields && !n_opts.headers)
                n_opts.headers = n_opts.fields.keys();

            m_specs = getSpec1(n_opts.fields);

            if (n_opts.datamap) {
                if (n_opts.datamap.typeCode() != NT_HASH)
                    throw errname, sprintf("expecting a hash value to option %y; got %y (type %s) instead", n_opts.datamap.key, n_opts.datamap, n_opts.datamap.type());
                # ensure that all the values are strings
                foreach hash<auto> h in (n_opts.datamap.pairIterator()) {
                    if (h.value.typeCode() != NT_STRING)
                        throw errname, sprintf("\"datamap\" key %y has a non-string value; got type %y", h.key, h.value.type());
                }

                string k = m_specs.firstKey();
                foreach hash<auto> h in (n_opts.datamap.pairIterator()) {
                    if (!exists m_specs{k}{h.key}) {
                        m_specs{k}{h.key} = ("type": "*string", "idx": m_specs{k}.size());
                    }
                    m_specs{k}{h.key}.header = tolwr ? h.value.lwr() : h.value;
                }
            }
            if (n_opts.headers) {
                adjustFieldsFromHeaders(m_specs.firstKey(), n_opts.headers);
            }
            processSpec();
        }

        #! Creates the AbstractCsvWriter in single-type mode
        /**

            @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more information
            @param n_errname a string to construct child class error message. For example: value 'FOO' will result in exception names 'CSVFOOWRITER-ERROR'
            @param n_opts @ref csvwriter_options

            @throw CSV<errname>WRITER-ERROR in the case of incorrect options
         */
        constructor(string n_errname, hash<auto> spec, hash<auto> n_opts): CsvHelper(sprintf("CSV%sWRITER-ERROR", n_errname)) {
            m_specs = getSpec2(spec);
            processCommonOptions(n_opts, C_OPT2);
            processSpec();
        }

        #! Returns the description of the record type, if any
        *hash<string, AbstractDataField> getRecordType() {
            return CsvHelper::getRecordType();
        }

        #! Process options and set internal variables
        private processCommonOptions(*hash<auto> n_opts, int C_OPTx) {
            date_format = 'DD/MM/YYYY hh:mm:SS';  # different default value in writer
            if (n_opts.encoding.typeCode() == NT_STRING) {
                encoding = n_opts.encoding;
                # ensure that the default value for the encoding is used (to handle case + encoding aliases); also validates that
                # the encoding is known and supported
                string str = convert_encoding(encoding, encoding);
                encoding = str.encoding();
            }
            else
                encoding = get_default_encoding();

            foreach hash<auto> i in (n_opts.pairIterator()) {
                if (!exists Options.(i.key) && (Options.(i.key) & C_OPTx) == 0)
                    throw errname, sprintf("unknown option %y passed to %s::constructor() (valid options: %y)", i.key, self.className(), (select Options.keys(), (Options.$1 & C_OPTx) != 0));

                # if the option is not set, then ignore it
                if (!exists i.value || i.value === NULL)
                    continue;

                switch (i.key) {
                    case "block": {
                        if (!i.value.intp())
                            throw errname, sprintf("expecting an integer value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        block = i.value.toInt();
                        if (block < 1)
                            throw errname, sprintf("expecting a positive integer value to option %y; got %y instead", i.key, i.value);
                        break;
                    }
                    case "separator": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        separator = i.value;
                        break;
                    }
                    case "quote": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        quote = i.value;
                        break;
                    }
                    case "quote_escape": {
                        if (i.value.typeCode() != NT_STRING) {
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        }
                        m_quoteEscapeChar = i.value;
                        break;
                    }
                    case "eol": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        if (!inlist(i.value, EOLS))
                            throw errname, sprintf("expecting a known end of line value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        eol = i.value;
                        break;
                    }
                    case "date_format":
                    case "date-format": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        date_format = i.value;
                        break;
                    }
                    case "number_format": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        number_format = i.value;
                        break;
                    }
                    case "verify_columns":
                    case "verify-columns": {
                        checkElementCounts = parse_boolean(i.value);
                        break;
                    }
                    case "write_headers":
                    case "write-headers": {
                        write_headers = parse_boolean(i.value);
                        break;
                    }
                    case "optimal_quotes":
                    case "optimal-quotes": {
                        optimal_quotes = parse_boolean(i.value);
                        break;
                    }
                    case "info_log": {
                        if (!i.value.callp())
                            throw errname, sprintf("expecting a callable value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        info_log = i.value;
                        break;
                    }
                    case "header_reorder": {
                        headerReorder = parse_boolean(i.value);
                        break;
                    }
                }

            }
            # make eg. '"%s",' template
            baseTemplate = sprintf("%s%%s%s%s", quote, quote, separator);
        }

        #! Process specification and set internal variable for mapping
        private processSpec() {
            if (write_headers && isMultiType()) {
                throw errname, sprintf("\"write_headers\" is True but \"headers\" defined as multi-type (%y)", m_specs);
            }
            foreach string k in (keys m_specs) {
                list ll;
                if (m_specs{k}) {
                    int i = 0;
                    foreach hash<auto> f in (m_specs{k}.pairIterator()) {
                        ll += ("out_idx": f.value.idx ?? 0, "field": f.key, "idx": i);
                        i++;
                    }
                    code sort_func = int sub (hash<auto> l, hash<auto> r) { return l.out_idx <=> r.out_idx; };
                    ll = sort(ll, sort_func);
                    m_out_by_name{k} = map ($1.field), ll;
                    m_out_by_idx{k} = map ($1.idx), ll;
                }
            }
        }

        #! Write csv headers
        private writeHeaders() {
            if (write_headers && m_specs.size() == 1 && m_specs.firstValue().size() > 0) {
                string type = m_specs.firstKey();
                list<auto> values = map (m_specs{type}{$1}.header ?? $1), m_out_by_name{type};
                writeRawLine(values);
            }
        }

        #! Write a line with a list of values; data are checked against column rules
        /**
            @param values a list with values.

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints
         */
        writeLine(list<auto> values) {
            writeLine(CSV_TYPE_SINGLE, values);
        }

        #! Write a line with headers-values hash
        /**
            @param values a hash with keys as column headers in single-type format or with hash with \"type\" and \"record\" members for multi-type. The format is automatically detected.

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints
         */
        writeLine(hash<auto> values) {
            if (exists values.type && exists values.record) {
                writeLine(values.type, values.record);
            } else {
                writeLine(CSV_TYPE_SINGLE, values);
            }
        }

        #! Write a line with headers-values list
        /**
            @param type record type
            @param values as list of values

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints
         */
        writeLine(string type, list<auto> values) {
            lineNo++;
            if (checkElementCounts && m_specs{type} && values.size() != m_specs{type}.size())
                throw errname, sprintf("Line (%d), Header size (%d) and line size (%d) are different (enforced by "
                    "the \"verify_columns\" option)", lineNo, m_specs{type}.size(), values.size());

            list<auto> rawValues = ();
            for(int j = 0; j < values.size(); j++) {
                int i = j;
                #! format date to string by options. Priority: 1) column definition 2) standard format
                string dateFmt = date_format;
                *string numFmt = number_format;
                if (m_out_by_idx{type} && j < m_out_by_idx{type}.size()) {
                    # get correct input value
                    i = m_out_by_idx{type}[j];
                    string column_name = m_out_by_name{type}[j];
                    hash<auto> spec = m_specs{type}{column_name};
                    switch (spec.type) {
                        case "bool": {
                            if (values[i].typeCode() != NT_BOOLEAN)
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a boolean "
                                    "value: %y", lineNo, j, column_name, values[i]);
                            values[i] = values[i] ? "true": "false";
                            break;
                        }
                        case "*bool": {
                            if (!values[i].empty() && values[i].typeCode() != NT_BOOLEAN)
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a boolean "
                                    "value: %y", lineNo, j, column_name, values[i]);
                            values[i] = values[i] ? "true": "false";
                            break;
                        }
                        case "int": {
                            if (int(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain an integer "
                                    "value %y vs %y", lineNo, j, column_name, int(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "*int": {
                            if (!values[i].empty() && int(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain an integer "
                                    "value %y vs %y", lineNo, j, column_name, int(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "float": {
                            if (float(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a float "
                                    "value %y vs %y", lineNo, j, column_name, float(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "*float": {
                            if (!values[i].empty() && float(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a float "
                                    "value %y vs %y", lineNo, j, column_name, float(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "number": {
                            if (number(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a number "
                                    "value %y vs %y", lineNo, j, column_name, number(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "*number": {
                            if (!values[i].empty() && number(values[i]) != values[i])
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a number "
                                    "value %y vs %y", lineNo, j, number(values[i]), values[i]);
                            numFmt = spec.format ?? number_format;
                            break;
                        }
                        case "date": {
                            if (values[i].typeCode() != NT_DATE)
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a date "
                                    "value (%s), %y", lineNo, j, column_name, values[i].type(), values[i]);
                            dateFmt = spec.format ?? date_format;
                            break;
                        }
                        case "*date": {
                            if (!values[i].empty() && values[i].typeCode() != NT_DATE)
                                throw errname, sprintf("Line (%d), Column (%d) %y: does not contain a date "
                                    "value (%s)", lineNo, j, column_name, values[i].type());
                            dateFmt = spec.format ?? date_format;
                            break;
                        }
                    }
                }

                # format date as requested
                if (values[i].typeCode() == NT_DATE) {
                    values[i] = format_date(dateFmt, values[i]);
                }
                if (numFmt) {
                    switch (values[i].typeCode()) {
                        case NT_INT: values[i] = values[i].format(numFmt); break;
                        case NT_FLOAT: values[i] = values[i].format(numFmt); break;
                        case NT_NUMBER: values[i] = values[i].format(numFmt); break;
                    }
                }
                push rawValues, values[i];
            }

            writeRawLine(rawValues);
        }

        #! Write a line for a specific record from a hash to the output
        /**
            @param type record type
            @param values a hash of values

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints
         */
        writeLine(string type, hash<auto> values) {
            if (!m_specs{type}) {
                # issue #4838: automatically set headers from the input data
                m_specs{type} = map {$1: NOTHING}, keys values;
                m_out_by_name{type} = keys values;
                # write headers if enabled
                writeHeaders();
            }
            list<auto> line;
            foreach string i in (keys m_specs{type}) { # (m_out_by_name{type}) {
                if (!values.hasKey(i)) {
                    if (exists m_specs{type}{i}."default") {
                        push line, m_specs{type}{i}."default";
                    } else {
                        push line, NOTHING;
                    }
                } else {
                    push line, values{i};
                }
            }
            writeLine(type, line);
        }

        #! Stream an iterator into the output
        /**
            @param iterator an iterator to stream data into file

            The iterator has to return @ref list or @ref hash from the
            @ref Qore::AbstractIterator::getValue() "getValue()" method

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints or when
            @ref Qore::AbstractIterator::getValue() "getValue()" does not return list or hash
         */
        write(Qore::AbstractIterator iterator) {
            while (iterator.next()) {
                auto row = iterator.getValue();
                switch (row.typeCode()) {
                    case NT_HASH:
                        writeLine(row);
                        break;
                    case NT_LIST:
                        writeLine(row);
                        break;
                    default:
                        throw errname, sprintf("Iterator's getValue() method returned %y, expected \"list\" or "
                            "\"hash\"", row.type());
                }
            }
        }

        #! Stream an iterator into the output
        /**
            @param iterator an @ref Qore::SQL::SQLStatement "SQLStatement" iterator to stream data into file; @ref Qore::SQL::SQLStatement::fetchColumns() "SQLStatement::fetchColumns()" is used to leverage bulk DML for more efficient SQL I/O

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints

            @note if any \c "info_log" option is set in the constructor; it is used here to log each block of SQL data used to generate the corresponding number of lines; log messages look like: \c "query input generated bulk output lines: 1000"
         */
        write(Qore::SQL::SQLStatement iterator) {
            while (*hash<auto> h = iterator.fetchColumns(block)) {
                int n = h.firstValue().lsize();
                if (!n)
                    break;
                map writeLine($1), h.contextIterator();
                if (info_log)
                    info_log(sprintf("query input generated bulk output lines: %d", n));
            }
        }

        #! Stream the contents of the list into the output
        /**
            @param l a list of input data to format as CSV output

            The list has to contain @ref list or @ref hash elements that can be formatted according to the CSV definition

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints or when the list elements have the wrong type or values
         */
        write(list<auto> l) {
            write(l.iterator());
        }

        #! This method must be overridden in child classes to provide the output implementation
        abstract private writeRawLine(list<auto> values);

        #! Prepare a string (line with EOF) with formatting and escaping.
        /**
            @param values a list with values

            @return string a formatted and escaped line with a trailing EOL string; if an \c encoding option was given to the constructor; the return value always has the given encoding
        */
        private string prepareRawLine(list<auto> values) {
            string rv = prepareRawLineIntern(values);
            return encoding && rv.encoding() != encoding ? convert_encoding(rv, encoding) : rv;
        }

        private string prepareRawLineIntern(list<auto> values) {
            if (!optimal_quotes) {
                # make full line template
                string template = strmul(baseTemplate, elements values, 1) + eol;
                # escape separators in strings
                list<auto> row = map replace(string($1), quote, m_quoteEscapeChar + quote), values;

                return vsprintf(template, row);
            }

            list<auto> l = ();
            foreach auto v in (values) {
                if (v.typeCode() == NT_STRING) {
                    if (v.find(quote) >= 0) {
                        v = replace(v, quote, m_quoteEscapeChar + quote);
                        v = sprintf("\"%s\"", v);
                    }
                    else if (v.find(separator) >= 0)
                        v = sprintf("\"%s\"", v);
                    l += v;
                    continue;
                }
                l += string(v);
            }
            return (foldl $1 + separator + $2, l) + eol;
        }

    } # AbstractCsvWriter class
} # CsvUtil namespace
