# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactReadDataProvider.qc EDIFACT read data provider

/*  EdifactReadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! EDIFACT read data provider
    /** Provides a data provider interface for reading EDIFACT messages.
    */
    public class EdifactReadDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            #! Constructor options
            const ConstructorOptions = {
                "path": <DataProviderOptionInfo>{
                    "display_name": "File Path",
                    "short_desc": "The path to the EDIFACT file to read",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The absolute or relative path to the EDIFACT file. Supports standard file "
                        "location syntax including file://, sftp://, etc.",
                    "required": True,
                },
                "data": <DataProviderOptionInfo>{
                    "display_name": "EDIFACT Data",
                    "short_desc": "EDIFACT message data as a string",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The raw EDIFACT message data as a string. Use this option instead of 'path' "
                        "when the data is already in memory.",
                },
                "stream": <DataProviderOptionInfo>{
                    "display_name": "Input Stream",
                    "short_desc": "Input stream containing EDIFACT data",
                    "type": AbstractDataProviderTypeMap."any",
                    "desc": "An InputStream object to read EDIFACT data from. Use this option for "
                        "streaming large files or network sources.",
                },
                "encoding": <DataProviderOptionInfo>{
                    "display_name": "Character Encoding",
                    "short_desc": "Character encoding of the EDIFACT data",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The character encoding used in the EDIFACT message. Common values are "
                        "ISO-8859-1 (Latin-1, default for UNOC) and UTF-8 (for UNOY).",
                    "default_value": EdifactEncoding::ISO_8859_1,
                    "allowed_values": AllowedEncodings,
                },
                "skip_service_segments": <DataProviderOptionInfo>{
                    "display_name": "Skip Service Segments",
                    "short_desc": "Skip envelope segments during iteration",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If True, skip service segments (UNA, UNB, UNZ, UNH, UNT, UNG, UNE, UNS) "
                        "during iteration, returning only business data segments.",
                    "default_value": False,
                },
            };

            #! Provider info
            const ProviderInfo = <DataProviderInfo>{
                "name": "edifactread",
                "desc": "EDIFACT read data provider",
                "type": "EdifactReadDataProvider",
                "supports_read": True,
                "supports_search_expressions": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            #! The iterator
            EdifactAbstractIterator m_iterator;

            #! Options
            *hash<auto> m_opts;
        }

        #! Creates the data provider
        constructor(string path, *hash<auto> opts) {
            m_opts = opts;
            m_iterator = new EdifactFileIterator(path, opts);
        }

        #! Creates the data provider from a hash
        constructor(*hash<auto> opts) {
            m_opts = opts;
            if (opts.path) {
                m_iterator = new EdifactFileIterator(opts.path, opts);
            } else if (opts.data) {
                m_iterator = new EdifactDataIterator(opts.data, opts);
            } else if (opts.stream) {
                m_iterator = new EdifactIterator(opts.stream, opts);
            } else {
                throw "EDIFACT-READ-ERROR", "one of 'path', 'data', or 'stream' must be provided";
            }
        }

        #! Returns the data provider name
        string getName() {
            return "edifactread";
        }

        #! Returns the data provider info
        hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        #! Returns an iterator for the data
        AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
            return new EdifactReadDataProviderRecordIterator(m_iterator);
        }
    }

    #! Record iterator for EDIFACT read data provider
    class EdifactReadDataProviderRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
        private {
            EdifactAbstractIterator m_iterator;
        }

        constructor(EdifactAbstractIterator iterator) {
            m_iterator = iterator;
        }

        bool valid() {
            return m_iterator.valid();
        }

        bool nextImpl() {
            return m_iterator.next();
        }

        hash<auto> getValue() {
            return m_iterator.getValue();
        }

        *hash<string, DataProvider::AbstractDataField> getRecordType() {
            # Return a generic record type for EDIFACT segments
            return {
                "tag": new DataProvider::QoreDataField("tag", "Segment tag", StringType),
                "data_elements": new DataProvider::QoreDataField("data_elements", "Segment elements", AutoListType),
                "raw": new DataProvider::QoreDataField("raw", "Raw segment string", StringType),
            };
        }
    }

    #! Factory for EDIFACT read data provider
    public class EdifactReadDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            #! Factory info
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "edifactread",
                "desc": "EDIFACT read data provider factory",
                "children_can_support_records": True,
            };
        }

        #! Returns the factory name
        string getName() {
            return "edifactread";
        }

        #! Returns the factory info
        hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        #! Returns the provider info
        hash<DataProviderInfo> getProviderInfoImpl() {
            return EdifactReadDataProvider::ProviderInfo;
        }

        #! Returns the class for the data provider
        Class getClassImpl() {
            return Class::forName("EdifactUtil::EdifactReadDataProvider");
        }
    }

}
