# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Qore Programming Language
#
# Copyright 2003 - 2026 Qore Technologies, s.r.o.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#! Contains all public definitions in the YamlDocumentDataProvider module
public namespace YamlDocumentDataProvider {

#! Options for the YAML document write data provider
public hashdecl YamlDocumentWriteDataProviderOptions {
    #! Path to YAML file to write
    *string path;

    #! Output stream to write to
    *Qore::OutputStream stream;

    #! Character encoding
    string encoding = DefaultEncoding;

    #! Use flow style (compact inline) YAML output
    bool flow_style = False;
}

#! YAML document stream write data provider
/** Provides DataProvider interface for writing YAML document streams.
    This enables integration with Qore's data provider framework for writing
    multi-document YAML files in a standardized way.

    Each call to createRecord() appends a YAML document separator ("---")
    followed by the YAML representation of the record to an internal buffer.
    The commit() method writes the buffer to the configured output path or stream.
    The getData() method returns the buffer content as binary data.

    @par Example: Writing to a file
    @code{.py}
YamlDocumentWriteDataProvider provider({"path": "/path/to/output.yaml"});
provider.createRecord({"name": "Alice", "age": 30});
provider.createRecord({"name": "Bob", "age": 25});
provider.commit();
    @endcode

    @par Example: Using the factory
    @code{.py}
AbstractDataProvider provider = DataProvider::getFactoryObject("yamlwrite", {"path": "/path/to/output.yaml"});
    @endcode

    @par Example: Getting data as binary
    @code{.py}
YamlDocumentWriteDataProvider provider({});
provider.createRecord({"key": "value"});
binary data = provider.getData();
    @endcode

    @see YamlDocumentWriteDataProviderFactory for factory-based creation

    @since yaml 1.1
*/
public class YamlDocumentWriteDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider information
        const ProviderInfo = <DataProviderInfo>{
            "name": "yamlwrite",
            "desc": "YAML document stream data provider for writing multi-document YAML",
            "type": "YamlDocumentWriteDataProvider",
            "supports_read": False,
            "supports_create": True,
            "supports_update": False,
            "supports_delete": False,
            "supports_request": False,
            "has_record": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor option metadata with full documentation
        const ConstructorOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "File Path",
                "short_desc": "Path to YAML output file",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The filesystem path to write the YAML document stream to. "
                        "Documents are separated by `---` markers.",
                "example_value": "/data/output.yaml",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Output Stream",
                "short_desc": "Output stream to write to",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An `OutputStream` object to write YAML documents to. Use this for "
                        "writing to network connections, compressed files, or other destinations.",
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Character Encoding",
                "short_desc": "Output encoding",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Character encoding for writing the YAML data. Default: `UTF-8`.",
                "default_value": DefaultEncoding,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "UTF-8",
                        "display_name": "UTF-8",
                        "short_desc": "UTF-8 encoding (recommended)",
                        "desc": "UTF-8 encoding (recommended)",
                    },
                    <AllowedValueInfo>{
                        "value": "UTF-16",
                        "display_name": "UTF-16",
                        "short_desc": "UTF-16 encoding",
                        "desc": "UTF-16 encoding",
                    },
                    <AllowedValueInfo>{
                        "value": "ISO-8859-1",
                        "display_name": "ISO-8859-1",
                        "short_desc": "Latin-1 encoding",
                        "desc": "Latin-1 encoding",
                    },
                ),
            },
            "flow_style": <DataProviderOptionInfo>{
                "display_name": "Flow Style",
                "short_desc": "Use compact inline YAML output",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "When `True`, outputs YAML in flow (compact inline) style. "
                        "When `False` (default), outputs in block (indented) style.",
                "default_value": False,
                "example_value": False,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": True,
                        "display_name": "Flow Style",
                        "short_desc": "Compact inline format",
                        "desc": "Flow style: compact inline format (e.g. {key: value})",
                    },
                    <AllowedValueInfo>{
                        "value": False,
                        "display_name": "Block Style",
                        "short_desc": "Indented format (default)",
                        "desc": "Block style: indented format (default)",
                    },
                ),
            },
        };
    }

    private {
        #! Provider options
        hash<YamlDocumentWriteDataProviderOptions> opts;

        #! Internal buffer for YAML content
        string buffer = "";

        #! The number of records written to the buffer
        int created = 0;
    }

    #! Creates the provider from typed options
    /** @param options provider options

        @throw YAML-WRITE-OPTION-ERROR if both <tt>path</tt> and <tt>stream</tt> are provided
    */
    constructor(hash<YamlDocumentWriteDataProviderOptions> options) {
        if (exists options.path && exists options.stream) {
            throw "YAML-WRITE-OPTION-ERROR", "options `path` and `stream` are mutually exclusive; "
                "provide one or the other, not both";
        }
        opts = options;
    }

    #! Creates the provider from untyped options
    constructor(*hash<auto> options) {
        opts = cast<hash<YamlDocumentWriteDataProviderOptions>>(options ?? {});
        if (exists opts.path && exists opts.stream) {
            throw "YAML-WRITE-OPTION-ERROR", "options `path` and `stream` are mutually exclusive; "
                "provide one or the other, not both";
        }
    }

    #! Returns the provider name
    string getName() {
        return opts.path ?? "yamlwrite";
    }

    #! Returns provider description
    *string getDesc() {
        if (opts.path) {
            return sprintf("YAML document write provider for file: %s", opts.path);
        } else if (opts.stream) {
            return "YAML document write provider for output stream";
        }
        return "YAML document write provider for in-memory buffer";
    }

    #! Creates a record (appends a YAML document to the buffer)
    /** @param rec hash representing a single document to write
        @param create_options create options (unused)
        @return the record that was written
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        buffer += "---\n";
        if (opts.flow_style) {
            buffer += make_yaml(rec);
        } else {
            buffer += make_yaml(rec, YAML::BlockStyle);
        }
        ++created;
        return rec;
    }

    #! Completes the create operations: writes the document stream to its path or stream, if any
    /** @return the generated document stream and the number of records in it, if there is no output path or stream;
        otherwise no value is returned

        @throw YAML-WRITE-ERROR if writing to path or stream fails
    */
    private *hash<GeneratedDataInfo> finishCreateImpl() {
        if (opts.path || opts.stream) {
            commit();
            return;
        }
        return <GeneratedDataInfo>{
            "data": getData(),
            "record_count": created,
        };
    }

    #! Commits the buffer to the configured output path or stream
    /** Writes the accumulated YAML document stream to the output destination.

        @throw YAML-WRITE-ERROR if writing to path or stream fails
    */
    commit() {
        binary data = getData();
        if (opts.path) {
            OutputStreamWrapper os = FileLocationHandler::getOutputStreamForLocation(opts.path);
            on_exit {
                os.close();
                os.waitForIo();
            }
            os.write(data);
        } else if (opts.stream) {
            opts.stream.write(data);
        }
    }

    #! Returns the YAML content as binary data
    /** @return binary representation of the accumulated YAML document stream
    */
    binary getData() {
        return binary(buffer);
    }

    #! Returns an open record type because YAML records can contain arbitrary fields
    /** @param search_options ignored

        @return @ref NOTHING to represent an open record shape
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return NOTHING;
    }

    #! Returns provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace YamlDocumentDataProvider
