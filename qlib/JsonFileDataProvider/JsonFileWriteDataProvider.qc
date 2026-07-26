# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JsonFileWriteDataProvider class definition

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JsonFileDataProvider module
public namespace JsonFileDataProvider {
#! Data provider for writing standard JSON file data
/** This data provider accumulates records and writes them as a standard JSON array
    to a file or stream when commit() is called or the provider is destroyed.

    @par Example
    @code{.py}
    JsonFileWriteDataProvider provider({"path": "output.json", "pretty_print": True});
    provider.createRecord({"name": "Alice", "age": 30});
    provider.createRecord({"name": "Bob", "age": 25});
    provider.commit();
    @endcode
*/
public class JsonFileWriteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "jsonwrite",
            "desc": "JSON file data provider for writing standard JSON files",
            "type": "JsonFileWriteDataProvider",
            "supports_create": True,
            "supports_request": False,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "Output File Path",
                "short_desc": "The path to the JSON output file",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The filesystem path or location URL for the output JSON data file; mutually "
                    "exclusive with `stream`; resolved with a call to "
                    "[FileLocationHandler::getOutputStreamForLocation()]("
                    "https://qoretechnologies.com/manual/qorus/current/qore/modules/FileLocationHandler/html/"
                    "class_file_location_handler_1_1_file_location_handler.html"
                    "#a9d1ca411ea23126280b9d2e4b4b3ede9)",
                "example_value": "/tmp/output.json",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Output Stream",
                "short_desc": "An output stream object for the JSON data",
                "type": AbstractDataProviderType::get(new Type("OutputStream")),
                "desc": "An output stream to write JSON data to; mutually exclusive with `path`",
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Encoding",
                "short_desc": "Character encoding for the output",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The character encoding to use when writing the output (default: `UTF-8`)",
                "default_value": DefaultEncoding,
                "example_value": "UTF-8",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "UTF-8",
                        "display_name": "UTF-8",
                        "desc": "UTF-8 encoding (recommended)",
                    },
                    <AllowedValueInfo>{
                        "value": "UTF-16",
                        "display_name": "UTF-16",
                        "desc": "UTF-16 encoding",
                    },
                    <AllowedValueInfo>{
                        "value": "ISO-8859-1",
                        "display_name": "ISO-8859-1",
                        "desc": "Latin-1 encoding",
                    },
                    <AllowedValueInfo>{
                        "value": "US-ASCII",
                        "display_name": "US-ASCII",
                        "desc": "US-ASCII encoding",
                    },
                ),
            },
            "pretty_print": <DataProviderOptionInfo>{
                "display_name": "Pretty Print",
                "short_desc": "Format JSON with indentation",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` (default), the JSON output is formatted with indentation for readability; "
                    "if `False`, the output is compact with no extra whitespace",
                "default_value": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Yes",
                        "short_desc": "Pretty-print with indentation",
                        "desc": "Format the JSON output with indentation for readability",
                        "value": True,
                    },
                    <AllowedValueInfo>{
                        "display_name": "No",
                        "short_desc": "Compact output",
                        "desc": "Write compact JSON output with no extra whitespace",
                        "value": False,
                    },
                ),
            },
        };
    }

    private {
        #! The output file path, if any
        *string path;

        #! The output stream, if any
        *Qore::OutputStream output_stream;

        #! The encoding
        string encoding;

        #! Pretty-print flag
        bool pretty_print;

        #! Accumulated records
        list<hash<auto>> records = ();

        #! Whether the data has been committed
        bool committed = False;
    }

    #! Creates the provider from constructor options
    /** @param options provider options as a generic hash

        @throw JSON-FILE-WRITE-ERROR if conflicting options are given
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("JSON-FILE-WRITE-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream) {
                error("JSON-FILE-WRITE-ERROR", "cannot provide both `stream` and `path` options; use "
                    "either one or the other");
            }
            path = copts.path;
        } else if (copts.stream) {
            output_stream = copts.stream;
        }
        encoding = copts.encoding ?? DefaultEncoding;
        pretty_print = copts.pretty_print ?? True;
    }

    #! Destroys the provider and commits any uncommitted data
    destructor() {
        if (!committed && records.size() > 0) {
            try {
                commit();
            } catch (hash<ExceptionInfo> ex) {
                # ignore errors in destructor
            }
        }
    }

    #! Returns the data provider name
    string getName() {
        return path ?? "jsonwrite";
    }

    #! Returns the data provider description
    *string getDesc() {
        if (path) {
            return sprintf("JSON file data provider for output file: %s", path);
        }
        return "JSON file data provider for writing";
    }

    #! Creates the given record in the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        records += rec;
        return rec;
    }

    #! Commits all accumulated records to the output
    /** Serializes all accumulated records as a JSON array and writes to the configured output.

        @throw JSON-FILE-WRITE-ERROR if no output destination is configured
    */
    commit() {
        binary json_data = getJsonData();
        if (exists path) {
            Qore::OutputStream os = FileLocationHandler::getOutputStreamForLocation(path);
            os.write(json_data);
            os.close();
        } else if (exists output_stream) {
            output_stream.write(json_data);
            output_stream.close();
        }
        committed = True;
    }

    #! Returns the accumulated data as binary JSON
    /** @return the JSON array of all accumulated records as binary data
    */
    binary getData() {
        return getJsonData();
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Generates JSON binary data from accumulated records
    /** @return the JSON data as binary
    */
    private binary getJsonData() {
        string json_str;
        if (pretty_print) {
            json_str = make_json(records, JGF_ADD_FORMATTING);
        } else {
            json_str = make_json(records);
        }
        return binary(json_str);
    }
}
}
