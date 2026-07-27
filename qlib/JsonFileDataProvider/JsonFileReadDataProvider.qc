# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JsonFileReadDataProvider class definition

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
#! Data provider for reading standard JSON file data
/** This data provider allows reading standard JSON data from files, streams, or strings
    using the DataProvider API.

    The JSON data may be:
    - An array of objects: each element is iterated as a separate record
    - A single object: returned as one record
    - An array of non-object values: each element is wrapped in \c {"value": element}

    @par Example
    @code{.py}
    JsonFileReadDataProvider provider({"path": "data.json"});
    AbstractDataProviderRecordIterator i = provider.searchRecords();
    map printf("Record: %y\n", $1), i;
    @endcode
*/
public class JsonFileReadDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "jsonread",
            "desc": "JSON file data provider for reading standard JSON files",
            "type": "JsonFileReadDataProvider",
            "supports_read": True,
            "supports_request": False,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "File Path",
                "short_desc": "The path to the JSON file to read",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The filesystem path or location URL of the JSON file to read; mutually exclusive "
                    "with `stream` and `data`; resolved with a call to "
                    "[FileLocationHandler::getBinaryStreamFromLocation()]("
                    "https://qoretechnologies.com/manual/qorus/current/qore/modules/FileLocationHandler/html/"
                    "class_file_location_handler_1_1_file_location_handler.html"
                    "#a29c2338efc901a6b8f24542505e2a094)",
                "example_value": "/tmp/data.json",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "short_desc": "The input stream to read from",
                "type": AbstractDataProviderType::get(new Type("InputStream")),
                "desc": "An input stream to read JSON data from; mutually exclusive with `path` and `data`",
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "Data",
                "short_desc": "JSON data as a string",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "JSON data provided directly as a string; mutually exclusive with `path` and `stream`",
                "example_value": "[{\"name\": \"Alice\", \"age\": 30}]",
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Encoding",
                "short_desc": "Character encoding for the input",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The character encoding to use when reading the input (default: `UTF-8`)",
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
        };
    }

    private {
        #! The file path, if any
        *string path;

        #! The input stream, if any
        *Qore::InputStream stream;

        #! The inline data, if any
        *string data;

        #! The encoding
        string encoding;
    }

    #! Creates the provider from constructor options
    /** @param options provider options as a generic hash; one of `path`, `stream`, or `data` must be provided

        @throw JSON-FILE-READ-ERROR if no input source is provided or conflicting options are given
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("JSON-FILE-READ-ERROR", ConstructorOptions, options);
        if (copts.path) {
            if (copts.stream || copts.data) {
                error("JSON-FILE-READ-ERROR", "option `path` is mutually exclusive with `stream` and `data`");
            }
            path = copts.path;
        } else if (copts.stream) {
            if (copts.data) {
                error("JSON-FILE-READ-ERROR", "option `stream` is mutually exclusive with `data`");
            }
            stream = copts.stream;
        } else if (copts.data) {
            data = copts.data;
        } else {
            throw "JSON-FILE-READ-ERROR", "one of `path`, `stream`, or `data` must be provided";
        }
        encoding = copts.encoding ?? DefaultEncoding;
    }

    #! Returns the data provider name
    string getName() {
        return path ?? "jsonread";
    }

    #! Returns the data provider description
    *string getDesc() {
        if (path) {
            return sprintf("JSON file data provider for file: %s", path);
        }
        return "JSON file data provider";
    }

    #! Returns an iterator for the records
    /** @param where_cond the search criteria
        @param search_options the search options

        @return an iterator for the JSON records
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        string json_str = readContent();
        auto parsed = parse_json(json_str);
        list<hash<auto>> records;
        if (parsed.typeCode() == NT_LIST) {
            records = map ($1.typeCode() == NT_HASH ? $1 : {"value": $1}), parsed;
        } else if (parsed.typeCode() == NT_HASH) {
            records = (parsed,);
        } else {
            records = ({"value": parsed},);
        }
        return new DefaultRecordIterator(new ListIterator(records), where_cond, search_options);
    }

    #! Reads all content from the configured input source
    /** @return the JSON string content

        @throw JSON-FILE-READ-ERROR if the input source cannot be read
    */
    private string readContent() {
        if (exists data) {
            return data;
        }
        Qore::InputStream is;
        if (exists stream) {
            is = stream;
        } else {
            is = FileLocationHandler::getBinaryStreamFromLocation(path);
        }
        StreamReader reader(is, encoding);
        *string content = reader.readString(-1);
        if (!exists content) {
            throw "JSON-FILE-READ-ERROR", "empty input from stream";
        }
        return content;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
