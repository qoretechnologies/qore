# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvReadDataProvider class definition

/*  CsvReadDataProvider.qc Copyright 2012 - 2026 Qore Technologies, s.r.o.

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

#! The primary CsvUtil namespace; all classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
#! Provides a data provider for reading CSV files
/** Supports generic record search operators and options
*/
public class CsvReadDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "type": "CsvReadDataProvider",
            "supports_read": True,
            "has_record": True,
            "supports_search_expressions": True,
            "constructor_options": ConstructorOptions,
            "search_options": GenericRecordSearchOptions,
            "expressions": AbstractDataProvider::GenericExpressions,
        };

        #! Common character encodings of CSV input; any encoding supported by Qore can be used
        /** @since CsvUtil 1.12
        */
        const CsvEncodingValues = (
            <AllowedValueInfo>{
                "value": "UTF-8",
                "display_name": "UTF-8",
                "desc": "Unicode UTF-8; the default, and what most current systems export",
            },
            <AllowedValueInfo>{
                "value": "UTF-16",
                "display_name": "UTF-16",
                "desc": "Unicode UTF-16 with a byte order mark, as written by *Unicode Text* exports",
            },
            <AllowedValueInfo>{
                "value": "UTF-16LE",
                "display_name": "UTF-16 Little Endian",
                "desc": "Unicode UTF-16, little-endian, without a byte order mark",
            },
            <AllowedValueInfo>{
                "value": "UTF-16BE",
                "display_name": "UTF-16 Big Endian",
                "desc": "Unicode UTF-16, big-endian, without a byte order mark",
            },
            <AllowedValueInfo>{
                "value": "WINDOWS-1252",
                "display_name": "Windows Western European (1252)",
                "desc": "Windows code page 1252, as written by Excel *CSV* exports on Western European Windows "
                    "systems; has `€` and typographic quotes, unlike `ISO-8859-1`",
            },
            <AllowedValueInfo>{
                "value": "WINDOWS-1250",
                "display_name": "Windows Central European (1250)",
                "desc": "Windows code page 1250, for Czech, Polish, Slovak, Hungarian and other Central European "
                    "languages on Windows systems",
            },
            <AllowedValueInfo>{
                "value": "WINDOWS-1251",
                "display_name": "Windows Cyrillic (1251)",
                "desc": "Windows code page 1251, for Cyrillic languages on Windows systems",
            },
            <AllowedValueInfo>{
                "value": "ISO-8859-1",
                "display_name": "Latin-1 (ISO-8859-1)",
                "desc": "ISO Latin-1 for Western European languages; has no `€`",
            },
            <AllowedValueInfo>{
                "value": "ISO-8859-2",
                "display_name": "Latin-2 (ISO-8859-2)",
                "desc": "ISO Latin-2 for Central European languages",
            },
            <AllowedValueInfo>{
                "value": "ISO-8859-15",
                "display_name": "Latin-9 (ISO-8859-15)",
                "desc": "ISO Latin-9, Latin-1 with `€`",
            },
            <AllowedValueInfo>{
                "value": "US-ASCII",
                "display_name": "ASCII",
                "desc": "7-bit ASCII",
            },
        );

        #! Common CSV field separators; any string can be used
        /** @since CsvUtil 1.12
        */
        const CsvSeparatorValues = (
            <AllowedValueInfo>{
                "value": ",",
                "display_name": "Comma",
                "desc": "Comma (`,`); the default",
            },
            <AllowedValueInfo>{
                "value": ";",
                "display_name": "Semicolon",
                "desc": "Semicolon (`;`), as written by spreadsheet exports in locales that use a decimal comma",
            },
            <AllowedValueInfo>{
                "value": "\t",
                "display_name": "Tab",
                "desc": "Tab character, for tab-separated values (TSV)",
            },
            <AllowedValueInfo>{
                "value": "|",
                "display_name": "Pipe",
                "desc": "Vertical bar (`|`)",
            },
        );

        #! Common CSV quote characters; any character can be used
        /** @since CsvUtil 1.12
        */
        const CsvQuoteValues = (
            <AllowedValueInfo>{
                "value": "\"",
                "display_name": "Double Quote",
                "desc": "Double quote (`\"`); the default",
            },
            <AllowedValueInfo>{
                "value": "'",
                "display_name": "Single Quote",
                "desc": "Single quote / apostrophe (`'`)",
            },
        );

        #! Common CSV end-of-line sequences; the end of line is detected if none is given
        /** @since CsvUtil 1.12
        */
        const CsvEolValues = (
            <AllowedValueInfo>{
                "value": "\n",
                "display_name": "LF",
                "desc": "Line feed, as on Linux and macOS",
            },
            <AllowedValueInfo>{
                "value": "\r\n",
                "display_name": "CR LF",
                "desc": "Carriage return and line feed, as on Windows",
            },
            <AllowedValueInfo>{
                "value": "\r",
                "display_name": "CR",
                "desc": "Carriage return, as on classic Mac OS",
            },
        );

        #! Constructor options
        const hash<string, hash<DataProviderOptionInfo>> ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "CSV Input Location",
                "short_desc": "The location of the input file",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_FromLocation: True,
                }),
                "desc": "The location of the CSV data; conflicts with `stream` and `data`; resolved with a call to "
                    "[FileLocationHandler::getBinaryStreamFromLocation()](https://qoretechnologies.com/manual/qorus/"
                    "current/qore/modules/FileLocationHandler/html/"
                    "class_file_location_handler_1_1_file_location_handler.html#a29c2338efc901a6b8f24542505e2a094)",
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "Input Data",
                "short_desc": "CSV data to process",
                "type": AbstractDataProviderTypeMap."data",
                "desc": "CSV data to process, as text or bytes; bytes are decoded with the `encoding` option; "
                    "conflicts with `stream` and `path`",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "short_desc": "An input stream object for CSV data",
                "type": AbstractDataProviderType::get(new Type("InputStream")),
                "desc": "An input stream for CSV data; conflicts with `path` and `data`",
            },
            "date_format": <DataProviderOptionInfo>{
                "display_name": "Date Format",
                "short_desc": "The default mask for date value input parsing",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The default date format for parsing `date` fields; see "
                    "[Date Mask Format](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/"
                    "group__date__and__time__functions.html#date_mask) for more information on this option "
                    "(ex: `DD/MM/YYYY HH:mm:SS`)",
                "example_value": "DD/MM/YYYY HH:mm:SS",
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Requires,
                        "option": "fields",
                        "desc": "applies only to fields that `fields` gives the `date` type; fields are strings by default",
                    },),
                },
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Input Character Encoding",
                "short_desc": "The Character encoding for the input file",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Character encoding of the input file, stream, or binary `data`; text `data` is read in "
                    "its own encoding, which is also the encoding of the output data.  Any character encoding "
                    "supported by Qore can be given; the allowed values list common ones",
                "default_value": "UTF-8",
                "allowed_values": CsvEncodingValues,
                "allowed_values_creatable": True,
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Ignored,
                        "option": "data",
                        "value_type": "string",
                        "desc": "text given in `data` is read in its own encoding; `encoding` applies only to a "
                            "file, a stream, or bytes in `data`",
                    },),
                },
            },
            "eol": <DataProviderOptionInfo>{
                "display_name": "End Of Line Chars",
                "short_desc": "The end of line / record character(s)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The end of line character(s); if not specified, then the end-of-line characters will be "
                    "auto detected",
                "allowed_values": CsvEolValues,
                "allowed_values_creatable": True,
            },
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Descriptions",
                "short_desc": "A hash describing the CSV fields on input",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "A hash describing the CSV fields on input. Keys are field names as given by the "
                    "`header_names` or `headers` options (if neither is used, field names are numbers starting "
                    "with `0`). Values are "
                    "[field types](https://qoretechnologies.com/manual/qorus/current/qore/modules/CsvUtil/html/"
                    "class_csv_util_1_1_abstract_csv_iterator.html#abstractcsviterator_option_field_types) "
                    "or a hash with the following keys:\n"
                    "- `type`: a [field type string](https://qoretechnologies.com/manual/qorus/current/qore/modules/"
                        "CsvUtil/html/class_csv_util_1_1_abstract_csv_iterator.html"
                        "#abstractcsviterator_option_field_types)\n"
                    "- `format`: for `date` types, a "
                        "[date format](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/"
                        "group__date__and__time__functions.html#date_formatting); for `int`, `float`, or `number` "
                        "types, a number format string: "
                        "*<thousands_separator>*[*<decimal_separator>*[*<decimals>*]] "
                        "(e.g. `\".,3\"` = `.` thousands, `,` decimal, 3 decimal places)\n"
                    "- `timezone`: for `date` types only; passed to "
                        "[TimeZone::constructor()](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/"
                        "class_qore_1_1_time_zone.html#ab77972801cee2c9d116a9631dfd631d9) "
                        "to parse dates (overrides any default time zone; use when different columns have "
                        "different time zones)\n\n"
                    "**Note**: setting this value also sets `headers` if not set automatically with "
                    "`header_names`. If no field type is given, the default is `*string`. Invalid field names "
                    "in this option are ignored.",
            },
            "header_lines": <DataProviderOptionInfo>{
                "display_name": "Number of Header Lines",
                "short_desc": "The number of headers lines in the file",
                "type": AbstractDataProviderTypeMap."softint",
                "desc": "The number of header lines skipped at the start of the input; if `header_names` is `true` "
                    "and this option is not set, one header line is assumed",
                "default_value": 0,
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Default,
                        "option": "header_names",
                        "value": True,
                        "default_value": 1,
                        "desc": "when `header_names` is `true`, one header line is assumed",
                    },),
                },
            },
            "header_names": <DataProviderOptionInfo>{
                "display_name": "Automatically Detect Headers",
                "short_desc": "If true then headers will be automatically detected from the first header row",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "if `true` then the object will parse the header names from the first header row, in this "
                    "case if `header_lines` is not set explicitly, it will be assumed to be `1`",
                "default_value": False,
                "has_dependents": True,
            },
            "header_reorder": <DataProviderOptionInfo>{
                "display_name": "Enforce Header Order",
                "short_desc": "If true, then the input data will be reordered in the order of headers",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "If `true` (the default) then if `headers` are provided, then data fields are reordered to "
                    "follow headers",
                "default_value": True,
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Requires,
                        "option": "headers",
                        "desc": "fields are reordered only to follow header names, given in `headers` or read from "
                            "the input with `header_names`",
                    }, <DataProviderOptionInteraction>{
                        "kind": OI_Requires,
                        "option": "header_names",
                        "value": True,
                        "desc": "fields are reordered only to follow header names, given in `headers` or read from "
                            "the input with `header_names`",
                    }),
                },
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "Header Names",
                "short_desc": "List of header / column names for the data iterated",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "List of header / column names for the data iterated; if this is present, then "
                    "`header_names` must be `false`",
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Conflicts,
                        "option": "header_names",
                        "value": True,
                        "desc": "`headers` cannot be given when `header_names` is `true`",
                    },),
                },
            },
            "ignore_empty": <DataProviderOptionInfo>{
                "display_name": "Ignore Empty Lines",
                "short_desc": "If true, empty lines will be ignored",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "If `true` (the default), empty lines are skipped; if `false`, an empty line is returned as a "
                    "record without values",
                "default_value": True,
            },
            "ignore_whitespace": <DataProviderOptionInfo>{
                "display_name": "Ignore Leading and Trailing Whitespace",
                "short_desc": "If true, leading and trailing whitespace will be stripped from non-quoted fields",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "If `true` (the default), leading and trailing whitespace is stripped from non-quoted "
                    "fields; quoted fields are never changed",
                "default_value": True,
            },
            "number_format": <DataProviderOptionInfo>{
                "display_name": "Number Format",
                "short_desc": "The default number format for parsing int, float, and number fields",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The default format for `int`, `float`, and `number` fields as a string giving the thousands "
                    "separator character followed by the decimal separator character (ex: `.,` for "
                    "continental-European-style numbers)",
                "example_value": ".,",
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Requires,
                        "option": "fields",
                        "desc": "applies only to fields that `fields` gives the `int`, `float` or `number` type; fields are strings by default",
                    },),
                },
            },
            "quote": <DataProviderOptionInfo>{
                "display_name": "Quote Char(s)",
                "short_desc": "The quote character(s)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The field quote character (default: `\"`); a quote character in a quoted field is "
                    "escaped by doubling it",
                "default_value": "\"",
                "allowed_values": CsvQuoteValues,
                "allowed_values_creatable": True,
            },
            "separator": <DataProviderOptionInfo>{
                "display_name": "Field Separator",
                "short_desc": "The field separator character(s)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "the string separating the fields in the file (default: `,`)",
                "default_value": ",",
                "allowed_values": CsvSeparatorValues,
                "allowed_values_creatable": True,
            },
            "timezone": <DataProviderOptionInfo>{
                "display_name": "Timezone Region",
                "short_desc": "The timezone region to use when parsing dates",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The timezone region to use when parsing dates (ex: `Europe/Prague`); see "
                    "[TZ indentifier](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones) for valid values",
                "example_value": "Europe/Prague",
                "metadata": {
                    OptionInteractionsKey: (<DataProviderOptionInteraction>{
                        "kind": OI_Requires,
                        "option": "fields",
                        "desc": "applies only to fields that `fields` gives the `date` type; the default is the current time zone",
                    },),
                },
            },
            "tolwr": <DataProviderOptionInfo>{
                "display_name": "Convert Headers to Lower Case",
                "short_desc": "Convert all header names to lower case",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "If `true` (default `false`), all header names will be converted to lower case letters",
                "default_value": False,
            },
            "verify_columns": <DataProviderOptionInfo>{
                "display_name": "Verify Columns",
                "short_desc": "Verifies input data to ensure it has the expected structure",
                "type": AbstractDataProviderTypeMap."softbool",
                "desc": "if `true` (default `false`), if a line is parsed with a different column or field count "
                    "than other lines, a `CSVFILEITERATOR-DATA-ERROR` exception is raised\n\n"
                    "Note that enabling this option will incurr a performance penalty",
                "default_value": False,
            },
        };

        #! CSV iteration option list
        const list<string> CsvIterationOptionList = keys (ConstructorOptions - ("path", "data", "stream"));
    }

    private {
        #! the iterator object
        AbstractCsvIterator i;

        #! stable object name for path-backed providers
        *string object_name;

        #! the record type, kept once read: a search releases the iterator when it is exhausted
        *hash<string, AbstractDataField> record_type;
    }

    #! Creates the CsvReadDataProvider with the input data path and optionally an option hash
    /** @param path the input file to iterate
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names"
        is @ref True "True" and \c "headers" is also present; unknown field type
    */
    constructor(string path, *hash<auto> opts) {
        object_name = path;
        i = new CsvFileIterator(path, opts);
    }

    #! Creates the CsvReadDataProvider with the input data stream and optionally an option hash
    /** @param stream the input stream to iterate
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names"
        is @ref True "True" and \c "headers" is also present; unknown field type
    */
    constructor(InputStream stream, *hash<auto> opts) {
        i = new CsvIterator(stream, opts.encoding, opts);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"path\" options; use either one or "
                    "the other");
            }
            if (copts."data") {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"path\" and \"data\" options; use either one or "
                    "the other");
            }
            object_name = copts.path;
            i = new CsvIterator(FileLocationHandler::getBinaryStreamFromLocation(copts.path),
                copts.encoding, copts{CsvIterationOptionList} - "encoding");
            return;
        }
        if (copts."data") {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"data\" options; use either one or "
                    "the other");
            }
            if (copts."data".typeCode() == NT_BINARY) {
                # bytes are decoded with the encoding option, as a file or stream is
                i = new CsvIterator(new BinaryInputStream(copts."data"), copts.encoding,
                    copts{CsvIterationOptionList} - "encoding");
            } else {
                i = new CsvIterator(new StringInputStream(copts."data"), copts."data".encoding(),
                    copts{CsvIterationOptionList});
            }
            return;
        }
        if (copts.stream) {
            i = new CsvIterator(copts.stream, copts.encoding, copts{CsvIterationOptionList});
            return;
        }
        # create a dummy interator with no input
        i = new CsvIterator(new StringInputStream(source: ""), copts.encoding, copts{CsvIterationOptionList});
    }

    #! Returns the object name
    string getName() {
        return object_name ?? sprintf("stream %s", self.uniqueHash());
    }

    #! Returns additional data-provider-specific info
    *hash<auto> getAdditionalInfo() {
        return i.getInfo();
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        return new DefaultRecordIterator<hash<auto>>(i, where_cond, search_options, getRecordType());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<DataProviderExpression> where_cond,
            *hash<auto> search_options) {
        return new DefaultRecordIterator<hash<auto>>(i, where_cond, search_options, getRecordType());
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        # the type is read before any search, which deletes the iterator once it has no more records; asking for
        # the type again afterwards (as the example data fallback does when there are no records) must not use it
        if (record_type) {
            return record_type;
        }
        if (!i.valid()) {
            i.peek();
        }
        return record_type = i.getRecordType();
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
} # CsvUtil namespace
