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

#! Options for the YAML document data provider
public hashdecl YamlDocumentReadDataProviderOptions {
    #! Path to YAML file to read
    *string path;

    #! Input stream to read from
    *Qore::InputStream stream;

    #! YAML data as string
    *string data;

    #! Skip empty documents (default: True)
    bool skip_empty_documents = True;

    #! Maximum document count to read (0 = unlimited)
    int max_documents = 0;

    # Note: max_document_size option removed as it was not enforced by the underlying iterator

    #! Character encoding
    string encoding = DefaultEncoding;
}

#! YAML document stream read data provider
/** Provides DataProvider interface for reading YAML document streams.
    This enables integration with Qore's data provider framework for reading
    multi-document YAML files in a standardized way.

    @par Example: Using the data provider directly
    @code{.py}
YamlDocumentReadDataProvider provider({"path": "/path/to/docs.yaml"});
AbstractDataProviderRecordIterator iter = provider.searchRecords();
while (iter.next()) {
    hash<auto> record = iter.getValue();
    printf("Document: %y\n", record);
}
    @endcode

    @par Example: Using the factory
    @code{.py}
AbstractDataProvider provider = DataProvider::getFactoryObject("yamldoc", {"path": "/path/to/docs.yaml"});
    @endcode

    @par Example: Reading from string data
    @code{.py}
string yaml = "---\nname: Alice\n---\nname: Bob";
YamlDocumentReadDataProvider provider({"data": yaml});
    @endcode

    @see YamlDocumentDataProviderFactory for factory-based creation
    @see YamlDocumentIterator for lower-level iteration

    @since yaml 1.1
*/
public class YamlDocumentReadDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider information
        const ProviderInfo = <DataProviderInfo>{
            "name": "yamldoc",
            "desc": "YAML document stream data provider for reading multi-document YAML",
            "type": "YamlDocumentReadDataProvider",
            "supports_read": True,
            "supports_create": False,
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
                "short_desc": "Path to YAML file",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The filesystem path to the YAML file containing one or more documents. "
                        "Documents are separated by `---` markers.",
                "example_value": "/data/config.yaml",
            },
            "stream": <DataProviderOptionInfo>{
                "display_name": "Input Stream",
                "short_desc": "Input stream to read",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "An `InputStream` object to read YAML documents from. Use this for "
                        "reading from network connections, compressed files, or other sources.",
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "YAML Data",
                "short_desc": "YAML data as string",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "YAML data provided directly as a string, useful for embedded data "
                        "or testing scenarios.",
                "example_value": "---\nname: Alice\n---\nname: Bob",
            },
            "skip_empty_documents": <DataProviderOptionInfo>{
                "display_name": "Skip Empty Documents",
                "short_desc": "Skip empty documents",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "When `True` (default), documents that parse to `NOTHING` are skipped. "
                        "Set to `False` to include empty documents in iteration.",
                "default_value": True,
                "example_value": True,
            },
            "max_documents": <DataProviderOptionInfo>{
                "display_name": "Max Documents",
                "short_desc": "Maximum documents to read",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Maximum number of documents to read from the stream. Use `0` (default) "
                        "for unlimited. Useful for sampling or limiting memory usage.",
                "default_value": 0,
                "example_value": 100,
            },
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Character Encoding",
                "short_desc": "Input encoding",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Character encoding for reading the YAML data. Default: `UTF-8`.",
                "default_value": DefaultEncoding,
                "example_value": "UTF-8",
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
        };
    }

    private {
        #! Provider options
        hash<YamlDocumentReadDataProviderOptions> opts;
    }

    #! Creates the provider from typed options
    /** @param options provider options

        @throw YAML-PROVIDER-ERROR if no input source specified
    */
    constructor(hash<YamlDocumentReadDataProviderOptions> options) {
        if (!exists options.path && !exists options.stream && !exists options.data) {
            throw "YAML-PROVIDER-ERROR", "one of 'path', 'stream', or 'data' must be provided";
        }
        opts = options;
    }

    #! Creates the provider from untyped options
    constructor(*hash<auto> options) {
        if (!exists options.path && !exists options.stream && !exists options.data) {
            throw "YAML-PROVIDER-ERROR", "one of 'path', 'stream', or 'data' must be provided";
        }
        # Convert to typed options
        opts = cast<hash<YamlDocumentReadDataProviderOptions>>(options);
    }

    #! Returns the provider name
    string getName() {
        return opts.path ?? "yamldoc";
    }

    #! Returns provider description
    *string getDesc() {
        if (opts.path) {
            return sprintf("YAML document provider for file: %s", opts.path);
        } else if (opts.stream) {
            return "YAML document provider for input stream";
        }
        return "YAML document provider for inline data";
    }

    #! Returns the record iterator
    private AbstractDataProviderRecordIterator searchRecordsImpl(
            *hash<auto> where_cond, *hash<auto> search_options) {
        return new YamlDocumentRecordIterator(getDocumentIterator());
    }

    #! Returns provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Creates the underlying document iterator
    private YAML::YamlDocumentIterator getDocumentIterator() {
        hash<YAML::YamlDocumentIteratorOptions> iter_opts = <YAML::YamlDocumentIteratorOptions>{
            "skip_empty_documents": opts.skip_empty_documents,
            "max_documents": opts.max_documents,
            "encoding": opts.encoding,
        };

        if (exists opts.data) {
            return new YAML::YamlDocumentIterator(opts.data, iter_opts);
        }

        Qore::InputStream stream;
        if (exists opts.stream) {
            stream = opts.stream;
        } else {
            stream = new FileInputStream(opts.path);
        }
        return new YAML::YamlDocumentIterator(stream, iter_opts);
    }
}

#! Record iterator wrapper for YAML documents
class YamlDocumentRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        YAML::YamlDocumentIterator iter;
    }

    constructor(YAML::YamlDocumentIterator iterator) {
        iter = iterator;
    }

    private bool nextImpl() {
        return iter.next();
    }

    hash<auto> getValue() {
        auto val = iter.getValue();
        # Wrap non-hash values in standard record format
        if (val.typeCode() != NT_HASH) {
            return {
                "document_number": iter.getDocumentNumber(),
                "value": val,
            };
        }
        return val;
    }

    bool valid() {
        return iter.valid();
    }
}

} # namespace YamlDocumentDataProvider
