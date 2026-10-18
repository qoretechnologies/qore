# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvWriteDataProvider class definition

/*  CsvWriteDataProvider.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
#! Provides a data provider for writing CSV files and data
/** The records created with the data provider are written to the output location of the \c path option, to the
    output stream of the \c stream option, or, with neither option, to CSV data in memory that
    @ref DataProvider::AbstractDataProvider::finishCreate() "finishCreate()" returns, as the \c write-data action
    does.

    @par Example
    @code{.py}
# create a CSV attachment from order lines
CsvWriteDataProvider prov({"headers": ("sku", "qty"), "encoding": "WINDOWS-1252"});
prov.createRecord({"sku": "A-1", "qty": 5});
prov.createRecord({"sku": "B-2", "qty": 7});
hash<GeneratedDataInfo> csv = prov.finishCreate();
# csv.data: the bytes of "sku,qty\nA-1,5\nB-2,7\n" in WINDOWS-1252; csv.record_count: 2
    @endcode
*/
public class CsvWriteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "type": "CsvWriteDataProvider",
            "supports_create": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const hash<string, hash<DataProviderOptionInfo>> ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "CSV Output Path",
                "short_desc": "Where to write the CSV output file",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The location for the output CSV data file; mutually exclusive with \"stream\"; the output "
                    "location is handled by a call to [FileLocationHandler::getOutputStreamForLocation()]("
                    "https://qoretechnologies.com/manual/qorus/current/qore/modules/FileLocationHandler/html/"
                    "class_file_location_handler_1_1_file_location_handler.html#a9d1ca411ea23126280b9d2e4b4b3ede9)",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Output Stream",
                "short_desc": "An output stream object for the CSV data",
                "type": AbstractDataProviderType::get(new Type("OutputStream")),
                "desc": "An output stream for CSV data; mutually exclusive with \"path\"",
            },
            "block": <DataProviderOptionInfo>{
                "display_name": "Block Size",
                "short_desc": "The block size when generating bulk output",
                "type": AbstractDataProviderType::get(SoftIntType),
                "desc": "The block size when generating bulk output",
            },
            "datamap": <DataProviderOptionInfo>{
                "display_name": "Output Field Name Map",
                "short_desc": "Maps data key names to output field names",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Sets a hash mapping actual data key names to the output field names, for use in case the "
                    "data field names differ; does not have to include every data or output key; keys not present "
                    "will be assumed to be mapped 1:1",
            },
            "date_format": <DataProviderOptionInfo>{
                "display_name": "Date Format",
                "short_desc": "The default mask for date value output formatting",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The default mask for date value output formatting; see "
                    "[Date Formatting Codes](https://qoretechnologies.com/manual/qorus/current/qore/lang/html/"
                    "group__date__and__time__functions.html#date_formatting) for more information",
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Character Encoding",
                "short_desc": "The output character encoding",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The output character encoding for the data",
                "default_value": "UTF-8",
            },
            "eol": <DataProviderOptionInfo>{
                "display_name": "End Of Line Chars",
                "short_desc": "The end of line / record character(s)",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The end of line / record character(s); if not present, the current platform standard will be "
                    "used",
            },
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Descriptions",
                "short_desc": "A hash describing the CSV fields to output",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "A hash describing the CSV fields to output. Keys are field names, values are "
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
                    "- `mask`: for `date`, `int`, `float`, or `number` types, a mask that the value is written "
                        "with, such as `DD.MM.YYYY`, `hh:mm AM`, `#.##0,00` or `€#,##0.00`, so that a file is "
                        "written exactly as the mask describes and read back with the same mask as the same "
                        "values; cannot be given with `format`; see "
                        "[ValueMask](https://qoretechnologies.com/manual/qorus/current/qore/modules/ValueMask/html/"
                        "index.html) for the masks\n"
                    "- `timezone`: for `date` types only, the time zone region whose wall-clock time the dates of "
                        "the field are written in; overrides the `timezone` option\n"
                    "- `header`: the header text of the field, if not the field name"
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "Header Names",
                "short_desc": "List of field / header names for the data output",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "List of field / header names for the data output",
            },
            "header_reorder": <DataProviderOptionInfo>{
                "display_name": "Enforce Header Order",
                "short_desc": "If true, then the output data will be reordered in the order of headers",
                "type": AbstractDataProviderType::get(SoftBoolType),
                "desc": "If `true` (the default) then if `headers` are provided, then data fields are reordered to "
                    "follow headers",
                "default_value": True,
            },
            "number_format": <DataProviderOptionInfo>{
                "display_name": "Output Number Format",
                "short_desc": "The default output format for 'int', 'float', and 'number' fields",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The default output format for `int`, `float`, and `number` fields; it is a number format "
                    "string with 1 to 3 fields as follows: "
                    "*<thousands_separator>*[*<decimal_separator>*[*<decimals>*]]; ex: `\".,3\"` (= `.` for the "
                    "thousands separator, `,` for the decimal separator, and 3 digits after the decimal place)",
                "example_value": ".,3",
            },
            "optimal_quotes": <DataProviderOptionInfo>{
                "display_name": "Optimal Quotes",
                "short_desc": "Quote output fields only when necessary",
                "type": AbstractDataProviderType::get(SoftBoolType),
                "desc": "Set to `false` to disable optimal quoting; when optimal quoting is disabled, all fields are "
                    "quoted regardless of type or content, when it is enabled, then fields are quoted only if they "
                    "require quoting: if they contain the quote character, the separator, the end of line, a "
                    "carriage return or a line feed, or start or end with white space; the default is `true`",
                "default_value": True,
            },
            "quote": <DataProviderOptionInfo>{
                "display_name": "Quote Char(s)",
                "short_desc": "The quote character(s)",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The quote character to quote fields (default: `\"`)",
                "default_value": "\"",
            },
            "quote_escape": <DataProviderOptionInfo>{
                "display_name": "Quote Escape Char(s)",
                "short_desc": "The escape character(s) written before a quote in a field",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The escape character(s) written before a `quote` character in a quoted field.\n\n"
                    "By default (an empty value), a quote is escaped by doubling it, as in RFC 4180 CSV files, which "
                    "spreadsheet programs and the CSV reader read by default.  Set to `\\` only for a consumer that "
                    "expects quotes escaped with a backslash (`\\\"`); the CSV reader reads such files with its "
                    "`quote_escape` option set to `\\`.",
                "default_value": "",
                "example_value": "\\",
            },
            "separator": <DataProviderOptionInfo>{
                "display_name": "Field Separator",
                "short_desc": "The field separator character(s)",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The string separating fields in the output data (default: `,`)",
                "default_value": ",",
            },
            "timezone": <DataProviderOptionInfo>{
                "display_name": "Time Zone",
                "short_desc": "The time zone whose wall-clock time dates are written in",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The time zone region (such as `Europe/Prague` or `America/New_York`) whose wall-clock "
                    "time dates are written in, so that a reader that reads the file in this time zone reads the "
                    "same points in time; a field's `timezone` attribute overrides it.  If not given, dates are "
                    "written in their own time zone, and dates written with a `mask` in the current time zone",
                "example_value": "Europe/Prague",
            },
            "verify_columns": <DataProviderOptionInfo>{
                "display_name": "Verify Columns",
                "short_desc": "Verifies input data to ensure it has the expected structure",
                "type": AbstractDataProviderType::get(SoftBoolType),
                "desc": "If `true` (the default is `false`) then if a line is parsed with a different column count "
                    "than other lines, a `CSVFILEWRITER-DATA-ERROR` exception is thrown.\n\n"
                    "Note that enabling this option will incurr a performance penalty",
                "default_value": False,
            },
            "write_headers": <DataProviderOptionInfo>{
                "display_name": "Output Headers",
                "short_desc": "Include headers as the first output record",
                "type": AbstractDataProviderType::get(SoftBoolType),
                "desc": "Set to `false` to suppress the output of headers; the default is `true`, meaning to output "
                    "headers if they are present",
                "default_value": True,
            },
        };

        #! CSV generation option list
        const list<string> CsvGenerationOptionList = keys (ConstructorOptions - ("path", "stream"));
    }

    private {
        #! the writer object
        AbstractCsvWriter writer;

        #! stable object name for path-backed providers
        *string object_name;

        #! the stream of the output location given with the \c path option, closed by finishCreate()
        *OutputStream location_stream;

        #! the stream that the CSV data is generated in if there is no \c path or \c stream option
        *BinaryOutputStream data_stream;

        #! the number of records written
        int created = 0;
    }

    #! Creates the CsvWriteDataProvider with the output path and optionally an option hash
    /** @param path the output file to write
        @param opts a hash of options; see @ref csvwriter_options for more information

        @throw CSVFILEWRITER-ERROR option error
    */
    constructor(string path, *hash<auto> opts) {
        object_name = path;
        writer = new CsvFileWriter(path, opts);
    }

    #! Creates the CsvWriteDataProvider with the output data stream where data will be writeen and an option hash
    /** @param stream the output stream for writing the data
        @param opts a hash of optional options; see @ref csvwriter_options for more information

        @throw CSVSTREAMWRITER-ERROR option error
    */
    constructor(OutputStream stream, *hash<auto> opts) {
        writer = new CsvWriter(stream, opts);
    }

    #! Returns the object name
    string getName() {
        return object_name ?? sprintf("stream %s", self.uniqueHash());
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"path\" options; use either one or "
                    "the other");
            }
            object_name = copts.path;
            location_stream = FileLocationHandler::getOutputStreamForLocation(copts.path);
            writer = new CsvWriter(location_stream, copts{CsvGenerationOptionList});
            return;
        }
        if (copts.stream) {
            writer = new CsvWriter(copts.stream, copts{CsvGenerationOptionList});
            return;
        }
        # the CSV data is generated in memory and returned by finishCreate()
        data_stream = new BinaryOutputStream();
        writer = new CsvWriter(data_stream, copts{CsvGenerationOptionList});
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return writer.getRecordType();
    }

    #! Creates the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        writer.writeLine(rec);
        ++created;
        return rec;
    }

    #! Completes the create operations
    /** If the data provider was created with neither a \c path nor a \c stream option, returns the CSV data
        generated from the records created with it; the data is in the encoding of the \c encoding option, and the
        data returned is no longer kept by the data provider.

        If it was created with a \c path option, closes the output location, so an error writing the data to the
        location, such as an upload to a remote location that fails, is raised here.

        @return the generated CSV data and the number of records in it, if there is no \c path or \c stream
        option; otherwise no value is returned

        @since %CsvUtil 1.12
    */
    private *hash<GeneratedDataInfo> finishCreateImpl() {
        if (data_stream) {
            return <GeneratedDataInfo>{
                "data": data_stream.getData(),
                "record_count": created,
            };
        }
        if (location_stream) {
            OutputStream stream = remove location_stream;
            stream.close();
            # an output location with background I/O reports its errors when the I/O is complete
            if (stream instanceof FileLocationHandler::OutputStreamWrapper) {
                cast<FileLocationHandler::OutputStreamWrapper>(stream).waitForIo();
            }
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
} # CsvUtil namespace
