# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore NdjsonReadDataProvider class definition

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

#! Contains all public definitions in the NdjsonDataProvider module
public namespace NdjsonDataProvider {
#! NDJSON read data provider constructor options
public hashdecl NdjsonReadDataProviderOptions {
    #! The path to the NDJSON file to read
    *string path;

    #! The input stream to read from
    *Qore::InputStream stream;

    #! The NDJSON data as a string
    *string data;

    #! Skip empty lines (default: True)
    bool skip_empty_lines = True;

    #! Skip invalid JSON lines instead of throwing an exception (default: False)
    bool skip_invalid_json = False;

    #! Maximum line length in bytes (default: 10 MB)
    int max_line_length = DefaultMaxLineLength;

    #! Encoding for the input data (default: UTF-8)
    string encoding = DefaultEncoding;
}

#! Data provider for reading NDJSON data
/** This data provider allows reading NDJSON data from files, streams, or strings
    using the DataProvider API.

    @par Example
    @code{.py}
    NdjsonReadDataProvider provider({"path": "data.ndjson"});
    AbstractDataProviderRecordIterator i = provider.searchRecords();
    map printf("Record: %y\n", $1), i;
    @endcode
*/
public class NdjsonReadDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "ndjson",
            "desc": "NDJSON data provider for reading Newline Delimited JSON data",
            "type": "NdjsonReadDataProvider",
            "supports_read": True,
            "supports_request": False,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "File Path",
                "short_desc": "The path to the NDJSON file to read",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The filesystem path to the NDJSON file to read",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "short_desc": "The input stream to read from",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An input stream to read NDJSON data from",
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "Data",
                "short_desc": "NDJSON data as a string",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "NDJSON data provided directly as a string",
            },
            "skip_empty_lines": <DataProviderOptionInfo>{
                "display_name": "Skip Empty Lines",
                "short_desc": "Skip empty lines in the input",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` (default), empty lines are skipped during iteration",
                "default_value": True,
            },
            "skip_invalid_json": <DataProviderOptionInfo>{
                "display_name": "Skip Invalid JSON",
                "short_desc": "Skip invalid JSON lines instead of throwing",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True`, lines that fail to parse as JSON are skipped; if `False` (default), "
                    "an exception is thrown",
                "default_value": False,
            },
            "max_line_length": <DataProviderOptionInfo>{
                "display_name": "Max Line Length",
                "short_desc": "Maximum line length in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Maximum allowed length for a single line in bytes (default: 10 MB)",
                "default_value": DefaultMaxLineLength,
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Encoding",
                "short_desc": "Character encoding for the input",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The character encoding to use when reading the input (default: UTF-8)",
                "default_value": DefaultEncoding,
            },
        };
    }

    private {
        #! Provider options
        hash<NdjsonReadDataProviderOptions> opts;
    }

    #! Creates the provider
    /** @param options provider options; one of path, stream, or data must be provided

        @throw NDJSON-ERROR if no input source is provided
    */
    constructor(hash<NdjsonReadDataProviderOptions> options) {
        if (!exists options.path && !exists options.stream && !exists options.data) {
            throw "NDJSON-ERROR", "one of 'path', 'stream', or 'data' must be provided";
        }
        opts = options;
        @debug {
            if (opts.path) {
                @debug("NdjsonReadDataProvider: reading from path %y", opts.path);
            } else if (opts.stream) {
                @debug("NdjsonReadDataProvider: reading from stream");
            } else {
                @debug("NdjsonReadDataProvider: reading from data string (%d bytes)", opts.data.size());
            }
        }
    }

    #! Creates the provider from a hash
    /** @param options provider options as a generic hash
    */
    constructor(*hash<auto> options) {
        hash<NdjsonReadDataProviderOptions> typed_opts();
        if (exists options.path) {
            typed_opts.path = options.path;
        }
        if (exists options.stream) {
            typed_opts.stream = options.stream;
        }
        if (exists options.data) {
            typed_opts.data = options.data;
        }
        if (exists options.skip_empty_lines) {
            typed_opts.skip_empty_lines = options.skip_empty_lines;
        }
        if (exists options.skip_invalid_json) {
            typed_opts.skip_invalid_json = options.skip_invalid_json;
        }
        if (exists options.max_line_length) {
            @assert(options.max_line_length > 0, "max_line_length must be positive");
            typed_opts.max_line_length = options.max_line_length;
        }
        if (exists options.encoding) {
            typed_opts.encoding = options.encoding;
        }
        if (!exists typed_opts.path && !exists typed_opts.stream && !exists typed_opts.data) {
            throw "NDJSON-ERROR", "one of 'path', 'stream', or 'data' must be provided";
        }
        opts = typed_opts;
        @debug {
            if (opts.path) {
                @debug("NdjsonReadDataProvider: reading from path %y", opts.path);
            } else if (opts.stream) {
                @debug("NdjsonReadDataProvider: reading from stream");
            } else {
                @debug("NdjsonReadDataProvider: reading from data string (%d bytes)", opts.data.size());
            }
        }
    }

    #! Returns the data provider name
    string getName() {
        return opts.path ?? "ndjson";
    }

    #! Returns the data provider description
    *string getDesc() {
        if (opts.path) {
            return sprintf("NDJSON data provider for file: %s", opts.path);
        }
        return "NDJSON data provider";
    }

    #! Returns an iterator for the records
    /** @param where_cond not used
        @param search_options not used

        @return an iterator for the NDJSON records
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new NdjsonRecordIterator(getIterator());
    }

    #! Returns the iterator options
    private hash<NdjsonIteratorOptions> getIteratorOptions() {
        return <NdjsonIteratorOptions>{
            "skip_empty_lines": opts.skip_empty_lines,
            "skip_invalid_json": opts.skip_invalid_json,
            "max_line_length": opts.max_line_length,
            "encoding": opts.encoding,
        };
    }

    #! Creates and returns the appropriate iterator
    private AbstractIterator getIterator() {
        if (exists opts.data) {
            return new NdjsonIterator(opts.data, getIteratorOptions());
        }
        Qore::InputStream stream;
        if (exists opts.stream) {
            stream = opts.stream;
        } else {
            stream = new FileInputStream(opts.path);
        }
        return new InputStreamNdjsonIterator(stream, getIteratorOptions());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Record iterator wrapper for NDJSON data
public class NdjsonRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The underlying iterator
        AbstractIterator iter;

        #! Record type - auto type since records can be any JSON value
        AbstractDataProviderType record_type;
    }

    #! Creates the record iterator
    /** @param iterator the underlying NDJSON iterator
    */
    constructor(AbstractIterator iterator) {
        iter = iterator;
        record_type = AbstractDataProviderTypeMap."auto";
    }

    #! Advances to the next record
    private bool nextImpl() {
        return iter.next();
    }

    #! Returns the current record value
    hash<auto> getValue() {
        auto val = iter.getValue();
        # Wrap non-hash values in a hash with a "value" key
        if (val.typeCode() != NT_HASH) {
            return {"value": val};
        }
        return val;
    }

    #! Returns True if the iterator is valid
    bool valid() {
        return iter.valid();
    }

    #! Returns the record type
    *AbstractDataProviderType getRecordType() {
        return record_type;
    }
}
}
