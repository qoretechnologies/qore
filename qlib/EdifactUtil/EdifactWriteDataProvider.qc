# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactWriteDataProvider.qc EDIFACT write data provider

/*  EdifactWriteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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


#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! EDIFACT write data provider
    /** Provides a data provider interface for writing EDIFACT messages.
    */
    public class EdifactWriteDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            #! Constructor options
            const ConstructorOptions = {
                "path": <DataProviderOptionInfo>{
                    "display_name": "File Path",
                    "short_desc": "The path to the EDIFACT output file",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The absolute or relative path to write the EDIFACT file. Supports standard "
                        "file location syntax including file://, sftp://, etc.",
                    "required": True,
                },
                "encoding": <DataProviderOptionInfo>{
                    "display_name": "Character Encoding",
                    "short_desc": "Character encoding for the output",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The character encoding to use for the EDIFACT output. Should match the "
                        "syntax identifier (e.g., ISO-8859-1 for UNOC, UTF-8 for UNOY).",
                    "default_value": EdifactEncoding::ISO_8859_1,
                    "allowed_values": AllowedEncodings,
                },
                "write_una": <DataProviderOptionInfo>{
                    "display_name": "Write UNA Segment",
                    "short_desc": "Include UNA service string advice",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If `True`, write the UNA segment at the beginning of the interchange to "
                        "explicitly define the delimiter characters. Recommended for interoperability.",
                    "default_value": True,
                },
                "line_break": <DataProviderOptionInfo>{
                    "display_name": "Add Line Breaks",
                    "short_desc": "Add newlines after each segment",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "If `True`, add a newline character after each segment terminator. This makes "
                        "the output more readable but increases file size slightly.",
                    "default_value": False,
                },
                "syntax_identifier": <DataProviderOptionInfo>{
                    "display_name": "Syntax Identifier",
                    "short_desc": "EDIFACT syntax identifier for UNB",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The syntax identifier to use in the UNB segment. Defines the character set "
                        "and syntax rules. UNOC (ISO 8859-1) is most common.",
                    "default_value": EdifactSyntaxIdentifier::UNOC,
                    "allowed_values": AllowedSyntaxIdentifiers,
                },
                "syntax_version": <DataProviderOptionInfo>{
                    "display_name": "Syntax Version",
                    "short_desc": "EDIFACT syntax version for UNB",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The syntax version number to use in the UNB segment. Version 3 is most "
                        "commonly used.",
                    "default_value": EdifactSyntaxVersion::V3,
                    "allowed_values": AllowedSyntaxVersions,
                },
            };

            #! Provider info
            const ProviderInfo = <DataProviderInfo>{
                "name": "edifactwrite",
                "desc": "EDIFACT write data provider",
                "type": "EdifactWriteDataProvider",
                "supports_create": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            #! The writer
            EdifactAbstractWriter m_writer;

            #! Options
            *hash<auto> m_opts;

            #! File path
            *string m_path;
        }

        #! Creates the data provider
        constructor(string path, *hash<auto> opts) {
            m_opts = opts;
            m_path = path;
            m_writer = new EdifactFileWriter(path, opts);
        }

        #! Creates the data provider from a hash
        constructor(*hash<auto> opts) {
            m_opts = opts;
            if (opts.path) {
                m_path = opts.path;
                m_writer = new EdifactFileWriter(opts.path, opts);
            } else {
                throw "EDIFACT-WRITE-ERROR", "'path' must be provided";
            }
        }

        #! Returns the data provider name
        string getName() {
            return "edifactwrite";
        }

        #! Returns the data provider info
        hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        #! Creates a record (writes a segment)
        *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
            if (rec.tag && rec.elements) {
                m_writer.writeSegment(rec.tag, rec.elements);
            } else {
                throw "EDIFACT-WRITE-ERROR", "record must have 'tag' and 'elements' keys";
            }
            return rec;
        }
    }

    #! Factory for EDIFACT write data provider
    public class EdifactWriteDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            #! Factory info
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "edifactwrite",
                "desc": "EDIFACT write data provider factory",
                "children_can_support_records": True,
            };
        }

        #! Returns the factory name
        string getName() {
            return "edifactwrite";
        }

        #! Returns the factory info
        hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        #! Returns the provider info
        hash<DataProviderInfo> getProviderInfoImpl() {
            return EdifactWriteDataProvider::ProviderInfo;
        }

        #! Returns the class for the data provider
        Class getClassImpl() {
            return Class::forName("EdifactUtil::EdifactWriteDataProvider");
        }
    }

}
