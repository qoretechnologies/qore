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
#! Provides a data provider for reading CSV files
public class CsvWriteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "CsvWriteDataProvider",
            "supports_create": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
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
                "desc": "Sets a hash describing the CSV fields to output; keys are field names, values are "
                    "[field types](https://qoretechnologies.com/manual/qorus/current/qore/modules/CsvUtil/html/"
                    "class_csv_util_1_1_abstract_csv_iterator.html#abstractcsviterator_option_field_types) "
                    "or a hash with the following keys:\n"
                    "- `type`: a [field type string](https://qoretechnologies.com/manual/qorus/current/qore/modules/"
                        "CsvUtil/html/class_csv_util_1_1_abstract_csv_iterator.html"
                        "#abstractcsviterator_option_field_types)\n"
                    "- `format`: when ysed with type `date`, a [date format](https://qoretechnologies.com/manual/"
                        "qorus/current/qore/lang/html/group__date__and__time__functions.html#date_formatting), when "
                        "used with type `int`, `float`, or `number`, it is a number format string with 1 to 3 "
                        "fields as follows: *<thousands_separator>*[*<decimal_separator>*[*<decimals>*]]; ex: "
                        "`\".,3\"` (= `.` for the thousands separator, `,` for the decimal separator, and 3 digits "
                        "after the decimal place)"
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "Header Names",
                "short_desc": "List of field / header names for the data output",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "List of field / header names for the data output",
            },
            "header_reorder": <DataProviderOptionInfo>{
                "display_name": "Enforce Header Order",
                "short_desc": "If true, then the output data will be reordered in the order of `headers`",
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
                    "require quoting (i.e. they contain a quote or separator character); the default is `true`",
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
                "short_desc": "The escape character(s) for quotes in a field",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The escape character(s) used for `quote` (default: `\\`)",
                "default_value": "\\",
            },
            "separator": <DataProviderOptionInfo>{
                "display_name": "Field Separator",
                "short_desc": "The field separator character(s)",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The string separating fields in the output data (default: `,`)",
                "default_value": ",",
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
        const CsvGenerationOptionList = keys (ConstructorOptions - ("path", "stream"));
    }

    private {
        #! the writer object
        AbstractCsvWriter writer;
    }

    #! Creates the CsvWriteDataProvider with the output path and optionally an option hash
    /** @param path the output file to write
        @param opts a hash of options; see @ref csvwriter_options for more information

        @throw CSVFILEWRITER-ERROR option error
    */
    constructor(string path, *hash<auto> opts) {
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
        return writer instanceof CsvFileWriter
            ? (writer.getFileName() ?? sprintf("stream %s", self.uniqueHash()))
            : sprintf("stream %s", self.uniqueHash());
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"path\" options; use either one or "
                    "the other");
            }
            writer = new CsvWriter(FileLocationHandler::getOutputStreamForLocation(copts.path),
                copts{CsvGenerationOptionList});
            return;
        }
        if (copts.stream) {
            writer = new CsvWriter(copts.stream, copts{CsvGenerationOptionList});
            return;
        }
        # create a dummy stream output object
        writer = new CsvWriter(new StringOutputStream(), copts{CsvGenerationOptionList});
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
        return rec;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
} # CsvUtil namespace
