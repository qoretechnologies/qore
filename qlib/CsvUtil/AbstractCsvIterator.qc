# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AbstractCsvIterator class definition

/*  AbstractCsvIterator.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
    #! the AbstractCsvIterator class is an abstract base class that allows abstract CSV data to be iterated
    /**
        @section abstractcsviterator_options AbstractCsvIterator Constructor Option Hash Overview
        The AbstractCsvIterator class constructor takes an optional hash with possible keys given in the following table.  Note that
        key names are case-sensitive, and data types are soft (conversions are made when possible).

        <b>AbstractCsvIterator Options</b>
        |!Option|!Data Type|!Description
        |\c "date_format"|@ref string_type "string"|the default date format for \c "date" fields (see @ref date_formatting "date formatting" for the value in this case)
        |\c "encoding"|@ref string_type "string"|the @ref character_encoding "character encoding" for the file (and for tagging string data read); if the value of this key is not a string then it will be ignored
        |\c "eol"|@ref string_type "string"|the end of line character(s) (default: auto-detect); if the value of this key is not a string then it will be ignored
        |\c "header_lines"|@ref int_type "int"|the number of headers lines in the file (if not present and \c "header_names" is @ref True "True", assumed to be \c 1)
        |\c "header_names"|\c bool|if @ref True "True" then the object will parse the header names from \
            the first header row, in this case if \c "header_lines" is not set explicitly, it will be assumed to be \
            \c 1. In case of multi-type lines \c "header_names" must be @ref False "False".
        |\c "header_reorder"|\c bool|if @ref True "True" (default value) then if \"headers\" are provided by options or read from file then data fields are reordered to follow headers. It has a major effect on the return value of @ref AbstractCsvIterator::getRecordList() and also a minor effect on the hash result of AbstractCsvIterator::getRecord() when a program  depends on the order of keys. If this value is @ref False "False" then fields not yet specified are pushed at the end of the field definition.
        |\c "ignore_empty"|\c bool|if @ref True "True" (the default) then empty lines will be ignored; this option is processed with @ref Qore::parse_boolean() "parse_boolean()"
        |\c "ignore_whitespace"|\c bool|if @ref True "True" (the default) then leading and trailing whitespace will be stripped from non-quoted fields; this option is processed with @ref Qore::parse_boolean() "parse_boolean()"
        |\c "number_format"|@ref string_type "string"|the default format for \c "int", \c "float", and \c "number" fields as a string giving the thousands separator character followed by the decimal separator character (ex: \c ".," for continental-European-style numbers)
        |\c "quote"|@ref string_type "string"|the field quote character (default: \c '\"')
        |\c "separator"|@ref string_type "string"|the string separating the fields in the file (default: \c ",")
        |\c "timezone"|@ref string_type "string"|the timezone to use when parsing dates (will be passed to @ref Qore::TimeZone::constructor())
        |\c "tolwr"|\c bool|if @ref True "True" then all header names will be converted to lower case letters
        |\c "verify_columns"|\c bool|if @ref True "True" (the default is @ref False "False") then if a line is parsed with a different column or field count than other lines, a \c CSVFILEITERATOR-DATA-ERROR exception is thrown

        <b>AbstractCsvIterator Single-type-only Options </b>
        |!Option|!Data Type|!Description
        |\c "headers"|@ref list_type "list" of @ref string_type "strings"|list of header / column names for the data iterated; if this is present, then \c "header_names" must be @ref False "False".
        |\c "fields"|@ref hash|the keys are field names as given by the \a header_names or \a headers option (in case neither of these options are used, then field names are numbers starting with \c "0") and the values are either strings (one of @ref abstractcsviterator_option_field_types giving the data type for the field) or a @ref abstractcsviterator_option_field_hash describing the field; also sets \a headers if not set automatically with \c "header_names"; if no field type is given, the default is \c "*string"; note that invalid field names given in this option are ignored

        <b>AbstractCsvIterator Multi-type-only Options </b>
        |!Option|!Data Type|!Description
        |\c "extended_record"|\c boolean|if @ref True "True" then get functions will use extended hash with \"type\" and \"record\" members to provide type to calling party, Default: @ref False "False"

        @note the following options separated by dashes are still supported for backwards-compatibility:
        - \c "date-format"
        - \c "ignore-empty"
        - \c "ignore-whitespace"
        - \c "header-names"
        - \c "header-lines"
        - \c "verify-columns"

        @subsection abstractcsviterator_option_field_types Option Field Types
        Fields are defined in order how the data are expected by user program. In this order are
        data returned by get functions. There are two exception, the former \c "headers" options sorts definition
        that data correspond to \c "headers" field order and the later when header names are read from Csv file header.

        <b>AbstractCsvIterator Option Field Types</b>
        |!Name|!Description
        |\c "bool"|the value will be unconditionally converted to a \c bool using the @ref Qore::parse_boolean() function
        |\c "*bool"|the value will be converted to @ref nothing if empty, otherwise it will be converted to a \c bool using the @ref Qore::parse_boolean() function
        |\c "int"|the value will be unconditionally converted to an @ref integer "integer" using the @ref Qore::int() function
        |\c "*int"|the value will be converted to @ref nothing if empty, otherwise it will be converted to an @ref integer "integer" using the @ref Qore::int() function
        |\c "float"|the value will be unconditionally converted to a @ref float "floating-point value" using the @ref Qore::float() function
        |\c "*float"|the value will be converted to @ref nothing if empty, otherwise it will be converted to a @ref float "floating-point value" using the @ref Qore::float() function
        |\c "number"|the value will be unconditionally converted to an @ref number "arbitrary-precision number value" using the @ref Qore::number() function
        |\c "*number"|the value will be converted to @ref nothing if empty, otherwise it will be converted to an @ref number "arbitrary-precision number value" using the @ref Qore::number() function
        |\c "string"|(the default) the value remains a string; no transformation is done on the input data
        |\c "*string"|the value will be converted to @ref nothing if empty, otherwise, it remains a string
        |\c "date"|in this case dates are parsed directly with the @ref Qore::date() function (and therefore are tagged automatically with the current @ref time_zones "time zone"); to specify another date format, use the @ref abstractcsviterator_option_field_hash "hash format" documented below
        |\c "*date"|the value will be converted to @ref nothing if empty, otherwise dates are parsed directly with the @ref Qore::date() function (and therefore are tagged automatically with the current @ref time_zones "time zone"); to specify another date format, use the @ref abstractcsviterator_option_field_hash "hash format" documented below

        @subsection abstractcsviterator_option_field_hash Option Field Hash
        See @ref complex_example "here" for an example of using the hash field description in the @ref CsvUtil::CsvFileIterator::constructor() "constructor()".

        <b>AbstractCsvIterator Option Field Hash and Spec Hash</b>
        Field specification is provided via options \"fields\" for old-style constructor or as separate parameter in new-style constructor supporting multi-type.

        |!Key|!Value Description
        |\c "type"|one of the @ref abstractcsviterator_option_field_types "option type values" giving the field type
        |\c "format"|when used with \c "date", this is a @ref date_mask "date/time format mask" for parsing dates, when used with \c "int", \c "float", or \c "number" types, this is a number format as in @ref Qore::format_number() "format_number()"
        |\c "timezone"|used only with the \c "date" type; this value is passed to @ref Qore::TimeZone::constructor() and the resulting timezone is used to parse the date (this value overrides any default time zone for the object; use only in the rare case that date/time values from different time zones are present in different columns of the same file)
        |\c "code"|this is a @ref closure "closure" or @ref call_reference "call reference" that takes a single argument of the value (after formatting with any optional \c "type" formats) and returns the value that will be output for the field

        <b>Extra AbstractCsvIterator Spec Hash Options</b>
        |!Key|!Data Type|!Value Description
        |\c value|@ref string_type "string"|the value to use to compare to input data when determining the record type; if \c "value" is defined for a field, then \c "regex" cannot be defined (for iterator only)
        |\c regex|@ref string_type "string"|the regular expression to use to apply to input data lines when determining the record type (for iterator only)
        |\c header|@ref string_type "string"|field name as defined in Csv header line. It enables remapping from Csv to own name
        |\c index|@ref int_type "int"|index of field in Csv file. It enables mapping when Csv has not header
        |\c default|@ref any_type "any"|Default output value (for writers only)

     */
    public class AbstractCsvIterator inherits Qore::AbstractIterator, private CsvHelper {
        private {
            #! valid options for the object (a hash for quick lookups of valid keys)
            const Options = {
                "compat_force_empty_string": C_OPT1|C_OPT2,
                "date_format": C_OPT1|C_OPT2,
                "date-format": C_OPT1|C_OPT2,
                "encoding": C_OPT1|C_OPT2,
                "eol": C_OPT1|C_OPT2,
                "extended_record": C_OPT2,
                "fields": C_OPT1,
                "header-lines": C_OPT1|C_OPT2,
                "header_lines": C_OPT1|C_OPT2,
                "header-names": C_OPT1|C_OPT2,
                "header_names": C_OPT1|C_OPT2,
                "header_reorder": C_OPT1|C_OPT2,
                "headers": C_OPT1,
                "ignore-empty": C_OPT1|C_OPT2,
                "ignore_empty": C_OPT1|C_OPT2,
                "ignore-whitespace": C_OPT1|C_OPT2,
                "ignore_whitespace": C_OPT1|C_OPT2,
                "number_format": C_OPT1|C_OPT2,
                "quote": C_OPT1|C_OPT2,
                "separator": C_OPT1|C_OPT2,
                "timezone": C_OPT1|C_OPT2,
                "tolwr": C_OPT1|C_OPT2,
                "verify-columns": C_OPT1|C_OPT2,
                "verify_columns": C_OPT1|C_OPT2,
            };

            # field separator
            string separator = ",";

            # field content delimiter
            string quote = "\"";

            # number of header lines
            softint headerLines = 0;

            # flag to use string names from the first header row if possible
            bool headerNames = False;

            # True if empty lines should be ignored
            bool ignoreEmptyLines = True;

            # Flag to trim the field content (trim leading and trailing whitespace) from unquoted fields
            bool ignoreWhitespace = True;

            # the @ref Qore::TimeZone to use when parsing dates (default: current time zone)
            *TimeZone timezone;

            # verify the column count for every row; if a row does not match, then throw a \c CSVFILEITERATOR-DATA-ERROR exception
            bool checkElementCounts = False;

            # getRecord/getValue returns extended hash
            bool extendedRecord = False;

            # force "*string" fields with no value to return an empty string rather than @ref nothing for backwards compatibility with very early versions of CsvUtil
            bool compat_force_empty_string = False;

            # read ahead flag
            bool read_ahead;

            # column count for verifying column counts
            int cc;

            # current record count for the index() method
            int rc = 0;

            # to resolve record type by rules
            hash<string, hash<string, list<hash<auto>>>> m_resolve_by_rule;

            # to resolve record type by number of fields
            hash<string, list<string>> m_resolve_by_count;

            # list of idx to field transformarions, in order of spec
            hash<string, list<string>> m_resolve_by_idx;

            # fake specs based on the first non-header row
            bool fakeHeaderNames;

            #! the eol marker, if any
            *string eol;

            # data source iterator
            AbstractLineIterator lineIterator;
        }

        #! creates the AbstractCsvIterator with an option hash in single-type mode
        /**
            @param li source line iterator
            @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

            @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option;
            \c "header-names" is @ref True "True" \c "headers" is also present; unknown field type
         */
        constructor(AbstractLineIterator li, *hash<auto> opts): CsvHelper("ABSTRACTCSVITERATOR-ERROR") {
            lineIterator = li;
            processCommonOptions(opts, C_OPT1);

            if (headerNames && opts.headers)
                throw errname, sprintf("\"header_names\" is True but \"headers\" has a value (%y)", opts.headers);

            processSpec(getSpec1(opts.fields));

            if (opts.headers) {
                # set headers automatically from field names if not set
                prepareFieldsFromHeaders(opts.headers);
            }
        }

        #! creates the AbstractCsvIterator with an option hash in multi-type mode
        /**
            @param li source line iterator
            @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more information
            @param opts a hash of optional options; see @ref abstractcsviterator_options for more information
         */
        # NOTE: when declared as *hash then always calls this constructor
        constructor(AbstractLineIterator li, hash<auto> spec, hash<auto> opts): CsvHelper("ABSTRACTCSVITERATOR-ERROR") {
            lineIterator = li;
            processCommonOptions(opts, C_OPT2);
            foreach hash<auto> i in (opts.pairIterator()) {
                switch (i.key) {
                    case "extended_record": {
                        extendedRecord = parse_boolean(i.value);
                        break;
                    }
                }
            }
            if (headerNames && isMultiType())
                throw errname, sprintf("\"header_names\" is True but multi-type is specified (%y)", m_specs);
            processSpec(getSpec2(spec));
            # potential prepareFieldsFromHeaders() call does not support rule adjusting
            if (headerNames && m_resolve_by_rule)
                throw errname, sprintf("\"header_names\" is True but a resolve rule is specified (%y)", m_specs);
        }

        #! Returns configuration information about the object
        hash<auto> getInfo() {
            return self{
                "separator", "quote", "headerLines", "headerNames", "ignoreEmptyLines", "ignoreWhitespace",
                "checkElementCounts", "extendedRecord", "compat_force_empty_string", "read_ahead",
            } + (timezone
                ? {
                    "timezone": timezone.region(),
                } : NOTHING);
        }

        #! process common options and and assing internal fields
        private processCommonOptions(*hash<auto> opts, int C_OPTx) {
            foreach hash<auto> i in (opts.pairIterator()) {
                if (!exists Options.(i.key) || ((Options.(i.key) & C_OPTx) == 0))
                    throw errname, sprintf("unknown option %y passed to AbstractCsvIterator::constructor() (valid options: %y)", i.key, (select Options.keys(), (Options.$1 & C_OPTx) != 0));
                string key = i.key;
                if (key == "date-format") {
                    key = "date_format";
                }
                switch (key) {
                    case "compat_force_empty_string": {
                        compat_force_empty_string = parse_boolean(i.value);
                        break;
                    }
                    case "date_format":
                    case "quote":
                    case "separator":
                    case "eol":
                    case "number_format": {
                        if (i.value.typeCode() != NT_STRING)
                            throw errname, sprintf("expecting a string value to option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        self{key} = i.value;
                        break;
                    }
                    case "ignore-empty":
                    case "ignore_empty": {
                        ignoreEmptyLines = parse_boolean(i.value);
                        break;
                    }
                    case "ignore-whitespace":
                    case "ignore_whitespace": {
                        ignoreWhitespace = parse_boolean(i.value);
                        break;
                    }
                    case "verify-columns":
                    case "verify_columns": {
                        checkElementCounts = parse_boolean(i.value);
                        break;
                    }
                    case "header-lines":
                    case "header_lines": {
                        try {
                            headerLines = i.value;
                        } catch (hash<ExceptionInfo> ex) {
                            throw errname, sprintf("expecting integer value or a value that can be converted to an integer for option %y; got %y (type %s) instead", i.key, i.value, i.value.type());
                        }
                        break;
                    }
                    case "header-names":
                    case "header_names": {
                        headerNames = parse_boolean(i.value);
                        break;
                    }
                    case "header_reorder":
                        headerReorder = parse_boolean(i.value);
                        break;
                    case "timezone": {
                        timezone = new TimeZone(i.value);
                        break;
                    }
                    case "tolwr": {
                        tolwr = parse_boolean(i.value);
                        break;
                    }
                }
            }
            # assume one header line if header-names is set
            if (headerNames && !headerLines) {
                headerLines = 1;
            }
        }

        #! process specification and assing internal data for resolving
        private processSpec(hash<auto> spec) {
            m_specs           = spec;

            m_resolve_by_rule = {};
            m_resolve_by_count = {};
            m_resolve_by_idx = {};
            fakeHeaderNames = False;

            # setup type resolving from spec
            foreach string k in (keys m_specs) {
                if (m_specs{k}.typeCode() != NT_HASH)
                    throw errname, sprintf("expecting a record description hash assigned to record key %y; got type %y instead (value: %y)", k, m_specs{k}.type(), m_specs{k});

                list<hash<auto>> rec_rule = ();
                m_resolve_by_idx{k} = ();
                foreach string c in (keys m_specs{k}) {
                    hash<auto> fld_rule;
                    if (exists m_specs{k}{c}.regex) {
                        if (exists m_specs{k}{c}.value) {
                            throw errname, sprintf("Both value and regex used in field rule for field %y, record: %y", c, k);
                        }
                        fld_rule.regex = m_specs{k}{c}.regex;
                    } else if (exists m_specs{k}{c}.value) {
                        fld_rule.value = m_specs{k}{c}.value;
                    }
                    if (fld_rule) {
                        fld_rule.idx = m_specs{k}{c}.idx;
                        rec_rule += fld_rule;
                    }
                    push m_resolve_by_idx{k}, c;
                }
                int cnt = m_specs{k}.size();
                if (rec_rule) {
                    # filter specified
                    m_resolve_by_rule{cnt}{k} = rec_rule;
                } else {
                    # no spec filter, check potential conflict later
                    if (!m_resolve_by_count{cnt}) {
                        m_resolve_by_count{cnt} = ();
                    }
                    m_resolve_by_count{cnt} += k;
                }
            }
        }

        #! match headers provided at csv header or in options, never called for multi-type because header_names is False
        private prepareFieldsFromHeaders(*list<auto> headers) {
            # add missing m_specs from headers
            string k = m_specs.firstKey();
            if (m_specs{k}) {
                m_resolve_by_count{m_specs{k}.size()} = select m_resolve_by_count{m_specs{k}.size()}, $1 != k;
            }
            m_resolve_by_idx{k} = adjustFieldsFromHeaders(k, headers, True);

            if (!m_resolve_by_count{headers.size()}) {
                m_resolve_by_count{headers.size()} = ();
            }
            push m_resolve_by_count{headers.size()}, k;
        }

        bool valid() {
            return lineIterator.valid();
        }

        #! Moves the current line / record position to the next line / record; returns @ref False if there are no more lines to iterate
        /** This method will return @ref True again after it returns @ref False once if the file being iterated has data that can be iterated, otherwise it will always return @ref False. The iterator object should not be used to retrieve a value after this method returns @ref False.
            @return @ref False if there are no lines / records to iterate (in which case the iterator object is invalid and should not be used); @ref True if successful (meaning that the iterator object is valid)

            @note that if headers are not given as an option to the constructor, then they are detected and set the first time AbstractCsvIterator::next() is run on a file (see @ref getHeaders())
         */
        bool next() {
            # try to parse any header and skip to data if we haven't started iterating yet
            if (!valid()) {
                if (headerLines) {
                    if (headerNames) {
                        # return False if there is no data to iterate
                        if (!lineIterator.next())
                            return False;

                        list<*string> l = getLineAndSplit();
                        if (!l)
                            return False;
                        prepareFieldsFromHeaders(l);
                    }
                    # skip the rest of the header rows
                    while (lineNumber() < headerLines) {
                        if (!lineIterator.next())
                            return False;
                    }
                }
            }
            bool b;
            if (read_ahead) {
                read_ahead = False;
                return True;
            } else {
                b = lineIterator.next();
            }
            if (b) {
                # skip empty lines
                if (ignoreEmptyLines) {
                    while (b && (lineIterator.getValue().empty())) {
                        b = lineIterator.next();
                    }
                }

                # generate fake header names with index positions if no header data is already available, note: multi-type has headers
                if (b && (!m_specs || (m_specs.size() == 1 && exists m_specs{CSV_TYPE_SINGLE} && m_specs{CSV_TYPE_SINGLE}.size() == 0))) {
                    list<*string> l = getLineAndSplit();
                    list<string> h = ();
                    map h += string($#), l;
                    prepareFieldsFromHeaders(h);
                    fakeHeaderNames = True;
                }
            }

            if (b)
                ++rc;
            else
                rc = 0;

            return b;
        }

        #! Reads a single row without moving the index position
        /** this method can be used to read headers for example without reading any data
        */
        peek() {
            if (!read_ahead) {
                read_ahead = next();
            }
        }

        #! Returns the given column value for the current row
        /** @param name the name of the field (header name) in record

            @return the value of the given header for the current record

            @throw INVALID-ITERATOR this error is thrown if the iterator is invalid; make sure that the next() method returns True before calling this method
            @throw ABSTRACTCSVITERATOR-FIELD-ERROR invalid or unknown field name given
         */
        auto memberGate(string name) {
            hash h = getRecord(True);
            if (!h.record.hasKey(name))
                throw "ABSTRACTCSVITERATOR-FIELD-ERROR", sprintf("the requested header %y does not exist (known headers: %y)", name, keys m_specs{h.type} );

            return h.record{name};
        }

        #! Returns the current record as a hash
        /** @par Example:
            @code{.py}
hash h = i.getValue();
            @endcode

            @return the current record as a hash; when the \c "extended_record" option is set, then the return value is a hash with the following keys:
            - \c "type": the record type
            - \c "record": a hash of the current record

            @throw INVALID-ITERATOR this error is thrown if the iterator is invalid; make sure that the next() method returns True before calling this method
         */
        hash<auto> getValue() {
            return getRecord();
        }

        #! Returns the current record as a hash
        /** @par Example:
            @code{.py}
hash h = i.getRecord();
            @endcode

            @param extended specifies if result is an extended hash including \c "type" and \c "record".

            @return the current record as a hash; if \a extended is @ref True, then the return value is a hash with the following keys:
            - \c "type": the record type
            - \c "record": a hash of the current record

            @throw INVALID-ITERATOR this error is thrown if the iterator is invalid; make sure that the next() method returns @ref True before calling this method
         */
        hash<auto> getRecord(bool extended) {
            hash<auto> h = parseLine();
            if (extended) {
                return ("type": h.type, "record": h.spec_fields);
            } else {
                return h.spec_fields;
            }
        }

        #! Returns the current record as a hash
        /** @par Example:
            @code{.py}
hash h = i.getRecord();
            @endcode

            @return the current record as a hash; when the \c "extended_record" option is set, then the return value is a hash with the following keys:
            - \c "type": the record type
            - \c "record": a hash of the current record

            @throw INVALID-ITERATOR this error is thrown if the iterator is invalid; make sure that the next() method returns @ref True before calling this method
         */
        hash<auto> getRecord() {
            return getRecord(extendedRecord);
        }

        #! Returns the current record as a list
        /** @par Example:
            @code{.py}
list l = i.getRecordList();
            @endcode

            When \"extended_record\" option is set then result is extended hash including \"type\" and \"record\".
            @return the current record as a list of field values; when the \c "extended_record" option is set, then the return value is a hash with the following keys:
            - \c "type": the record type
            - \c "record": a list of field values for the current record

            @throw INVALID-ITERATOR this error is thrown if the iterator is invalid; make sure that the next() method returns True before calling this method
         */
        auto getRecordList() {
            hash<auto> h = parseLine();
            if (extendedRecord) {
                return ("type": h.type, "record": h.all_fields);
            } else {
                return h.all_fields;
            }
        }

        #! Returns the current separator string
        /** @par Example:
            @code{.py}
string sep = i.getSeparator();
            @endcode

            @return the current separator string
         */
        string getSeparator() {
            return separator;
        }

        #! Returns the current quote string
        /** @par Example:
            @code{.py}
string quote = i.getQuote();
            @endcode

            @return the current quote string
         */
        string getQuote() {
            return quote;
        }

        #! Returns the description of the record type, if any
        *hash<string, AbstractDataField> getRecordType() {
            return CsvHelper::getRecordType();
        }

        #! Returns the current record headers or @ref nothing if no headers have been detected or saved yet
        /** @par Example:
            @code{.py}
*list l = i.getHeaders();
            @endcode

            @note if headers are not saved against the object in the \c constructor(), then they are written to the object after the first call to \c next()
         */
        *list<string> getHeaders() {
            return getHeaders(m_specs.firstKey());  # single-type
        }

        #! Returns a list of headers for the given record or @ref nothing if the record is not recognized
        /** @par Example:
            @code{.py}
*list l = i.getHeaders(my_type);
            @endcode
         */
        *list<string> getHeaders(string type) {
            return map ($1.value.header), m_specs{type}.pairIterator();
        }

        #! Returns the row index being iterated, which does not necessarily correspond to the line number when there are header rows and blank lines are skipped
        /** @par Example:
            @code{.py}
int index = i.index();
            @endcode

            @return the row index being iterated, which does not necessarily correspond to the line number when there are header rows and blank lines are skipped

            @see lineNumber()

            @since %CsvUtil 1.1
        */
        int index() {
            return rc;
        }

        #! Returns the current iterator line number in the file (the first line is line 1) or 0 if not pointing at a valid element
        /** @par Example:
            @code{.py}
while (i.next()) {
    printf("+ line %d: %y\n", i.lineNumber(), i.getValue());
}
            @endcode

            @return returns the current iterator line number in the data (the first line is line 1) or 0 if not pointing at a valid element

            @see index()

            @since %CsvUtil 1.1
        */
        int lineNumber() {
            return lineIterator.index();
        }

        #! Returns the current line 'as it is', i.e. the original string
        /** @par Example:
            @code{.py}
                string s = i.getRawLine();
            @endcode

            @return the current raw line, i.e. the original string before parsing

            @since %CsvUtil 1.6.3
        */
        string getRawLine() {
            return lineIterator.getValue();
        }

        #! Returns the list of raw string values of the current line
        /** @par Example:
            @code{.py}
                list<*string> l = i.getRawLineValues();
            @endcode

            @return the list of raw string values of the current line. Parsing is done only
                to split the fields but not to intrepret their contents according to their types.

            @since %CsvUtil 1.6.3
        */
        list<*string> getRawLineValues() {
            return getRawLine().split(separator, quote, ignoreWhitespace);
        }

        private auto handleType(hash<auto> fh, *string val) {
            auto rv;
            string type = fh.type;
            # if it's an "or nothing" type and there is no value, then return nothing
            if (type =~ /^\*/) {
                if (val.empty()) {
                    # see if the "compat force empty string" option is set (either local or global)
                    if ((global_compat_force_empty_string || compat_force_empty_string) && type == "*string") {
                        return "";
                    }
                    return NOTHING;
                }
                splice type, 0, 1;
            }

            switch (type) {
                case "int": {
                    *string fmt = fh.format ?? number_format;
                    if (fmt) {
                        # check for invalid chars
                        string tmp = replace(val, fmt[0], "");
                        tmp = replace(tmp, fmt[1], "");
                        if (!is_int(tmp)) {
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        }
                        rv = parse_int(val, fmt);
                    }
                    else {
                        if (!is_int(val))
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        rv = int(val);
                    }
                    break;
                }
                case "float": {
                    *string fmt = fh.format ?? number_format;
                    if (fmt) {
                        # check for invalid chars
                        string tmp = replace(val, fmt[0], "");
                        tmp = replace(tmp, fmt[1], "");
                        if (!is_float(tmp)) {
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        }
                        rv = parse_float(val, fmt);
                    }
                    else {
                        if (!is_float(val))
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        rv = float(val);
                    }
                    break;
                }
                case "number": {
                    *string fmt = fh.format ?? number_format;
                    if (fmt) {
                        # check for invalid chars
                        string tmp = replace(val, fmt[0], "");
                        tmp = replace(tmp, fmt[1], "");
                        if (!is_number(tmp)) {
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        }
                        rv = parse_number(val, fmt);
                    }
                    else {
                        if (!is_number(val))
                            throw "FIELD-VALUE-ERROR", sprintf("invalid %s value: %y", type, val);
                        rv = number(val);
                    }
                    break;
                }
                case "date": {
                    if (val.empty())
                        rv = 1970-01-01Z;
                    else {
                        TimeZone tz;
                        if (fh.timezone)
                            tz = fh.timezone;
                        else if (timezone)
                            tz = timezone;
                        *string fmt = fh.format ?? date_format;
                        if (fmt)
                            rv = tz ? tz.date(val, fmt) : date(val, fmt);
                        else
                            rv = tz ? tz.date(val) : date(val);
                    }
                    break;
                }
                case "bool": {
                    rv = parse_boolean(val);
                    break;
                }
                default:
                    rv = val;
                    break;
            }

            return rv;
        }

        #! Read line split by separator/quote into list
        private list<*string> getLineAndSplit() {
            return lineIterator.getSplitLine(separator, quote, eol, ignoreWhitespace);
        }

        #! Identify a fixed-length line type using identifyTypeImpl(); may be overridden if necessary.
        /**
            @param rec Input line record to be identified

            @return the name of the record corresponding to the input line

            @throw ABSTRACTCSVITERATOR-ERROR input line cannot be matched to a known record
        */
        string identifyType(list<auto> rec) {
            *string type = identifyTypeImpl(rec);
            if (type != CSV_TYPE_UNKNOWN) {
                if (!type)
                    throw errname, sprintf("The input line could not be identified: %y", rec);
                if (!m_specs.hasKey(type))
                    throw errname, sprintf("Line identified to be of type %y that is not present in the spec: %y", type, rec);
            }
            return type;
        }

        #! Identify a input record, given the raw line string. This method performs a lookup to a precalculated table based on number of records (see constructor()). In case different criteria are needed, eg. when two line types in a spec have the same record number and no unique resolving rule are specified, this method needs to be overridden, otherwise it will throw an exception because the precalculated mapping will be empty.
        /**
            @param rec Input line record to be identified

            @return the record name or @ref nothing if the input cannot be matched

            @throw ABSTRACTCSVITERATOR-ERROR input line cannot be matched to a known record
        */
        private *string identifyTypeImpl(list<auto> rec) {
            *string rv;
            int cnt = rec.size();
            if (!m_resolve_by_rule{cnt} && !m_resolve_by_count{cnt} && checkElementCounts)
                throw errname, sprintf("Line of unexpected field count %d found; known counts: %y (record: %y)", cnt,
                    sort(map ($1.toInt()), keys (m_resolve_by_rule + m_resolve_by_count)), rec);

            if (m_resolve_by_rule{cnt}) {
                # try match type by filter spec
                foreach string k in (keys m_resolve_by_rule{cnt}) {
                    list rec_rule = m_resolve_by_rule{cnt}{k};
                    bool found = True;  # flt has always at least one item
                    foreach hash fld_rule in (rec_rule) {
                        if (exists fld_rule.regex) {
                            # we do not limit regex to field length to support multi field regex
                            if (!rec[fld_rule.idx].regex(fld_rule.regex)) {
                                found = False;
                                break;
                            }
                        } else {
                            if (fld_rule.value.typeCode() == NT_INT) {
                                # special care about int type,  0 == "000" should evaluate as True
                                if (!rec[fld_rule.idx].intp() || rec[fld_rule.idx].toInt() != fld_rule.value) {
                                    found = False;
                                    break;
                                }
                            } else {
                                if (rec[fld_rule.idx] != fld_rule.value) {
                                    found = False;
                                    break;
                                }
                            }
                        }
                    }
                    if (found) {
                        rv = k;
                        break;
                    }
                }
            }
            if (!rv && m_resolve_by_count{cnt}) {
                if (m_resolve_by_count{cnt}.size() == 1) {
                    rv = m_resolve_by_count{cnt}[0];
                }
                if (m_resolve_by_count{cnt}.size() > 1)
                    throw errname, sprintf("Line with recourd count %d was not automatically matched since the "
                        "following records have this length: %y; you need to provide your own identifyTypeImpl() "
                        "method or specify rules (record: %y)", cnt, m_resolve_by_count{cnt}, rec);
            }
            if (!rv && !fakeHeaderNames && m_specs.size() == 1 && exists m_specs{CSV_TYPE_SINGLE}) {
                # where there is only one type then process it, e.g. when optional fields are not mentioned at the end of line
                rv = CSV_TYPE_SINGLE;
            }
            if (!rv && !checkElementCounts) {
                rv = CSV_TYPE_UNKNOWN;
            }
            return rv;
        }

        #! Parses a line in the file and returns a processed list of the fields
        private hash<auto> parseLine() {
            list<*string> values = getLineAndSplit();
            string type = identifyType(values);
            hash<auto> r;
            list<auto> l;
            if (type == CSV_TYPE_UNKNOWN) {
                r += map {$#: $1}, values;
                l = values;
            } else {
                foreach string f in (m_resolve_by_idx{type}) {
                    # cherry-pick well-known fields
                    hash<auto> fh = m_specs{type}{f};
                    auto val = values[fh.idx];

                    # first apply type transformations
                    val = handleType(fh, val);

                    # execute any callable value on the processed value
                    if (fh.code) {
                        val = fh.code(val);
                    }
                    r{f} = val;
                    push l, val;
                }
            }
            return ("type": type, "spec_fields": r, "all_fields": l);
        }
    } # AbstractCsvIterator class
} # CsvUtil namespace
