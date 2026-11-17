# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AbstractCsvWriter class definition

/*  AbstractCsvWriter.qc Copyright 2012 - 2026 Qore Technologies, s.r.o.

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
        |\c "optimal_quotes"|\c bool_type "bool"|set to @ref False "False" to disable optimal quoting; when optimal quoting is disabled, all fields are quoted regardless of type or content, when it is enabled, then fields are quoted only if they require quoting: if they contain the quote character, the separator, the end of line, a carriage return or a line feed, or start or end with white space, which a reader strips from unquoted fields; the default is @ref True "True"
        |\c "quote"|@ref string_type "string"|the field quote character (default: \c '\"')
        |\c "quote_null"|\c bool_type "bool"|set to @ref True "True" to write a field without a value (@ref nothing or \c NULL, such as a \c NULL column of a @ref Qore::SQL::SQLStatement "SQLStatement") as an empty quoted field (\c '""'), or to @ref False "False" to write it as an empty field without quotes, so that it can be told apart from an empty string written with quotes (see @ref csvwriter_quoting); if not given, a field with @ref nothing is written without quotes, unless \c "optimal_quotes" is @ref False "False", and a \c NULL value as before (@since %CsvUtil 1.12)
        |\c "quote_types"|@ref list_type "list" of @ref string_type "strings"|the types of the fields that are always quoted: \c "string", \c "int", \c "float", \c "number", \c "date", \c "bool", \c "binary", \c "hash" or \c "list"; the type of a field is the type given in its field description, if it is not \c "string" or \c "*string" (the \c "*" prefix is ignored), otherwise the type of the value; the other fields are quoted only if they require quoting; a field without a value is written as given by \c "quote_null"; ignored if \c "optimal_quotes" is @ref False "False" (see @ref csvwriter_quoting) (@since %CsvUtil 1.12)
        |\c "quote_escape"|@ref string_type "string"|the escape character(s) written before a \c "quote" character in a quoted field; if not given or an empty string (the default), the quote character itself, so a quote is escaped by doubling it, as in RFC 4180 CSV data; before %CsvUtil 1.12 the default was a backslash (\c '\\'), which a reader reads with its \c "quote_escape" option set to \c '\\'
        |\c "separator"|@ref string_type "string"|the string separating the fields in the file (default: \c ",")
        |\c "timezone"|@ref string_type "string"|the time zone region (ex: \c "Europe/Prague") whose wall-clock time dates are written in; a field's \c "timezone" attribute overrides it; if not given, dates are written in their own time zone, and dates written with a \c "mask" in the current time zone (@since %CsvUtil 1.12)
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

        @section csvwriter_quoting AbstractCsvWriter Field Quoting
        By default (\c "optimal_quotes" is @ref True "True"), a field is quoted only if it requires quoting: if it
        contains the quote character, the separator, the end of line, a carriage return or a line feed, or starts or
        ends with white space; with \c "optimal_quotes" set to @ref False "False", every field is quoted.

        Fields can also be quoted by their type, for readers or loaders that take a quoted field as text and an
        unquoted field as another type, or an empty unquoted field as \c NULL (for example PostgreSQL's \c COPY in
        CSV format):
        - the \c "quote_types" option gives the types of the fields that are always quoted (ex: \c ("string") to
          quote strings, but not numbers or dates); a field of another type is quoted only if it requires quoting
        - the \c "quote" attribute of a field description (@ref True "True" or @ref False "False") overrides
          \c "quote_types" for the field: @ref True "True" to quote it always, @ref False "False" to quote it only if
          it requires quoting
        - the \c "quote_null" option sets how a field without a value (@ref nothing or \c NULL) is written:
          @ref False "False" as an empty field without quotes, whatever its type, and @ref True "True" as an empty
          quoted field (\c '""'); a field with an empty string is quoted if its type is quoted, so that it can be
          told apart from a field without a value

        For example, with \c "quote_types" set to \c ("string") and \c "quote_null" set to @ref False "False", the
        values \c "a", \c "", @ref nothing, \c NULL, \c 1 and \c 1.5 are written as:
        @code
"a","",,,1,1.5
        @endcode

        @note the CSV readers of this module read an empty quoted field and an empty unquoted field the same way: as an
        empty string for a \c "string" field and as @ref nothing for a \c "*string" field, so that the difference is
        meaningful only for other readers
     */
    public class AbstractCsvWriter inherits private CsvHelper {
        public {
            #! white space that a reader strips from the start or end of an unquoted field, by default
            /** a field starting or ending with one of these characters is quoted

                @since %CsvUtil 1.12
            */
            const EdgeWhitespace = {
                " ": True,
                "\t": True,
                "\v": True,
                "\n": True,
                "\r": True,
            };
        }

        private {
            #! valid options for the object (a hash for quick lookups of valid keys)
            const hash<string, int> Options = {
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
                "quote_null": C_OPT1|C_OPT2,
                "quote_types": C_OPT1|C_OPT2,
                "separator": C_OPT1|C_OPT2,
                "timezone": C_OPT1|C_OPT2,
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

            #! quote escape character(s); an empty string (the default) means the quote character (RFC 4180)
            string m_quoteEscapeChar = "";

            #! a quote character in a quoted field as it is written: escaped
            string escapedQuote;

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

            #! the field types that are always quoted (the \c "quote_types" option), if any
            /** @since %CsvUtil 1.12
            */
            *hash<string, bool> quote_types;

            #! how a field without a value is written (the \c "quote_null" option), if set
            /** @since %CsvUtil 1.12
            */
            *bool quote_null;

            #! a closure/call reference for informational logging when using write(SQLStatement)
            *code<nothing(string, ...)> info_log;

            #! the time zone whose wall-clock time dates are written in, if any
            *TimeZone timezone;

            #! mapping output field by name
            hash m_out_by_name;

            #! mapping output field by index
            hash m_out_by_idx;
        }

        private {
            #! the field types that the \c "quote_types" option takes
            const QuoteTypes = {
                "string": True,
                "int": True,
                "float": True,
                "number": True,
                "date": True,
                "bool": True,
                "binary": True,
                "hash": True,
                "list": True,
            };

            #! the names of the types of values for the \c "quote_types" option, if they differ from their type names
            const QuoteValueTypes = {
                "integer": "int",
                "boolean": "bool",
            };

            #! True if fields are quoted by type or have the \c "quote" attribute, or if \c "quote_null" is set
            bool quote_by_type = False;

            #! how each field of the line being written is quoted, if fields are quoted by type (see QuoteAlways,
            #! QuoteIfNeeded and QuoteNull)
            *list<int> line_quote;

            #! a field that is always quoted
            const QuoteAlways = 1;
            #! a field that is quoted if it requires quoting
            const QuoteIfNeeded = 0;
            #! a field without a value
            const QuoteNull = -1;
        }

        #! Creates the AbstractCsvWriter in single-type mode
        /**
            @param n_errname a string to construct child class error message. For example: value 'FOO' will result in exception names 'CSVFOOWRITER-ERROR'
            @param n_opts0 @ref csvwriter_options

            @throw CSV<errname>WRITER-ERROR in the case of incorrect options
         */
        constructor(string n_errname, *hash<auto> n_opts0): CsvHelper(sprintf("CSV%sWRITER-ERROR", n_errname)) {
            # a caller's literal may be narrowed (e.g. hash<string, hash<string, string>> for {"fields": {...}}),
            # and adding the "headers" key would throw
            *hash<auto!> n_opts = n_opts0;
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
                        checkNumberFormat(i.value, "option \"number_format\"");
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
                    case "quote_null": {
                        quote_null = parse_boolean(i.value);
                        break;
                    }
                    case "quote_types": {
                        if (i.value.typeCode() != NT_LIST) {
                            throw errname, sprintf("expecting a list of type names for option %y; got %y (type %s) "
                                "instead", i.key, i.value, i.value.type());
                        }
                        hash<string, bool> h();
                        foreach auto t in (i.value) {
                            if (t.typeCode() != NT_STRING || !QuoteTypes{t}) {
                                throw errname, sprintf("unknown type %y in option %y (known types: %y)", t, i.key,
                                    keys QuoteTypes);
                            }
                            h{t} = True;
                        }
                        quote_types = h;
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
                    case "timezone": {
                        if (i.value.typeCode() != NT_STRING) {
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead",
                                i.key, i.value, i.value.type());
                        }
                        try {
                            timezone = new TimeZone(i.value);
                        } catch (hash<ExceptionInfo> ex) {
                            throw errname, sprintf("invalid time zone %y given for option %y: %s: %s", i.value, i.key,
                                ex.err, ex.desc);
                        }
                        break;
                    }
                }

            }
            # make eg. '"%s",' template
            baseTemplate = sprintf("%s%%s%s%s", quote, quote, separator);
            escapedQuote = (m_quoteEscapeChar.val() ? m_quoteEscapeChar : quote) + quote;
        }

        #! Returns True if a field of any record type has the \c "quote" attribute
        private bool hasFieldQuote() {
            foreach hash<auto> t in (m_specs.pairIterator()) {
                foreach hash<auto> f in (t.value.pairIterator()) {
                    if (exists f.value.quote) {
                        return True;
                    }
                }
            }
            return False;
        }

        #! Process specification and set internal variable for mapping
        private processSpec() {
            quote_by_type = quote_types.val() || exists quote_null || hasFieldQuote();
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
            # how each field is quoted, if fields are quoted by type
            *list<int> lq = quote_by_type ? () : NOTHING;
            for(int j = 0; j < values.size(); j++) {
                int i = j;
                #! format date to string by options. Priority: 1) column definition 2) standard format
                string dateFmt = date_format;
                *string numFmt = number_format;
                *TimeZone tz = timezone;
                *hash<auto> spec;
                *string column_name;
                if (m_out_by_idx{type} && j < m_out_by_idx{type}.size()) {
                    # get correct input value
                    i = m_out_by_idx{type}[j];
                    column_name = m_out_by_name{type}[j];
                    spec = m_specs{type}{column_name};
                    if (spec.timezone) {
                        tz = spec.timezone;
                    }
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

                if (quote_by_type) {
                    push lq, getFieldQuote(spec, values[i]);
                }

                # a value written with a ValueMask mask
                if (exists spec.mask && exists values[i]) {
                    try {
                        switch (values[i].typeCode()) {
                            case NT_DATE:
                                values[i] = tz
                                    ? ValueMask::formatDate(values[i], spec.mask, tz, spec.mask_kind)
                                    : ValueMask::formatDate(values[i], spec.mask, NOTHING, spec.mask_kind);
                                break;
                            case NT_INT:
                            case NT_FLOAT:
                            case NT_NUMBER:
                                values[i] = ValueMask::formatNumber(values[i], spec.mask);
                                break;
                        }
                    } catch (hash<ExceptionInfo> ex) {
                        if (ex.err != "VALUE-MASK-VALUE-ERROR") {
                            rethrow;
                        }
                        throw errname, sprintf("Line (%d), Column (%d) %y: %s", lineNo, j, column_name, ex.desc);
                    }
                    push rawValues, values[i];
                    continue;
                }

                # format date as requested
                if (values[i].typeCode() == NT_DATE) {
                    values[i] = format_date(dateFmt, tz ? tz.date(values[i]) : values[i]);
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

            line_quote = lq;
            on_exit remove line_quote;
            writeRawLine(rawValues);
        }

        #! Returns how a field is quoted when fields are quoted by type
        /** @param spec the description of the field, if any
            @param v the value of the field before it is formatted

            @return QuoteAlways to quote the field, QuoteIfNeeded to quote it only if it requires quoting, or
            QuoteNull for a field without a value

            @since %CsvUtil 1.12
        */
        private int getFieldQuote(*hash<auto> spec, auto v) {
            if (!exists v || v === NULL) {
                return QuoteNull;
            }
            if (exists spec.quote) {
                return spec.quote ? QuoteAlways : QuoteIfNeeded;
            }
            if (!quote_types) {
                return QuoteIfNeeded;
            }
            # the declared type of the field, unless it is a string (the default), else the type of the value
            *string t = spec.type;
            if (t && t[0] == "*") {
                t = t.substr(1);
            }
            if (!t || t == "string") {
                t = v.type();
                t = QuoteValueTypes{t} ?? t;
            }
            return quote_types{t} ? QuoteAlways : QuoteIfNeeded;
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
            if (line_quote) {
                return prepareRawLineQuotedByType(values);
            }
            if (!optimal_quotes) {
                # every field is quoted; complex types are serialized via csvValueToString()
                list<string> row = map quoteField(csvValueToString($1)), values;
                return (foldl $1 + separator + $2, row) + eol;
            }

            list<string> l = ();
            foreach auto v in (values) {
                # Convert non-string-printable values (hash, list, etc.) to a string
                # representation; the result is then quoted like any other string value;
                # NOTHING produces an empty field
                if (!exists v) {
                    l += "";
                    continue;
                }
                string sv = v.strp() ? v.toString() : serializeValue(v);
                l += needsQuotes(sv) ? quoteField(sv) : sv;
            }
            return (foldl $1 + separator + $2, l) + eol;
        }

        #! Returns a line whose fields are quoted by type (see @ref csvwriter_quoting)
        /** @since %CsvUtil 1.12
        */
        private string prepareRawLineQuotedByType(list<auto> values) {
            # how a field without a value is written: quoted if "quote_null" is set, else as without the option
            bool qnull = quote_null ?? !optimal_quotes;
            list<string> l = ();
            foreach auto v in (values) {
                if (!exists v || v === NULL || line_quote[$#] == QuoteNull) {
                    l += qnull ? quote + quote : "";
                    continue;
                }
                string sv = csvValueToString(v);
                l += (!optimal_quotes || line_quote[$#] == QuoteAlways || needsQuotes(sv)) ? quoteField(sv) : sv;
            }
            return (foldl $1 + separator + $2, l) + eol;
        }

        #! Returns True if a field must be quoted to be read back as it is written
        /** A field is quoted if it contains the quote character, the separator, the end of line, a carriage return or
            a line feed, or if it starts or ends with white space, which a reader strips from unquoted fields

            @since %CsvUtil 1.12
        */
        private bool needsQuotes(string v) {
            if (!v.val()) {
                return False;
            }
            return v.find(quote) >= 0 || v.find(separator) >= 0 || v.find("\n") >= 0 || v.find("\r") >= 0
                || (eol.val() && v.find(eol) >= 0) || EdgeWhitespace{v[0]} || EdgeWhitespace{v[v.length() - 1]};
        }

        #! Returns a quoted field: quote characters in it are escaped
        /** @since %CsvUtil 1.12
        */
        private string quoteField(string v) {
            return quote + replace(v, quote, escapedQuote) + quote;
        }

        #! Converts a value to a string suitable for CSV output
        /** Values that can be converted to a string via \c toString() are converted
            directly; complex types (hashes, lists, etc.) are serialized via
            \c serializeValue().
        */
        private static string csvValueToString(auto v) {
            if (!exists v) {
                return "";
            }
            return v.strp() ? v.toString() : serializeValue(v);
        }

        #! Serializes a complex value to a string
        /** Uses the bundled YAML serializer for hashes, lists, and other complex values.
        */
        private static string serializeValue(auto v) {
            return regex_subst(trim(make_yaml(v)), "^%YAML [0-9.]+\\n---\\s*", "");
        }

    } # AbstractCsvWriter class
} # CsvUtil namespace
