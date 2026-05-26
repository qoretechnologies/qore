# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2DataProviders.qc HL7 v2 DataProvider integration

/*  Hl7v2DataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Hl7v2Util module namespace
public namespace Hl7v2Util {
    #! HL7 v2 read data provider
    public class Hl7v2ReadDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            const ConstructorOptions = {
                "path": <DataProviderOptionInfo>{
                    "display_name": "File Path",
                    "short_desc": "Path to the HL7 v2 message file",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Path to a file containing one HL7 v2 message.",
                },
                "data": <DataProviderOptionInfo>{
                    "display_name": "HL7 v2 Data",
                    "short_desc": "HL7 v2 message data as a string",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Inline HL7 v2 message data. Use this option when the message is already in memory.",
                },
            };

            const ProviderInfo = <DataProviderInfo>{
                "name": "hl7v2read",
                "desc": "HL7 v2 read data provider",
                "type": "Hl7v2ReadDataProvider",
                "supports_read": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            hash<Hl7v2MessageInfo> message;
        }

        #! Creates the read provider from options
        constructor(*hash<auto> opts) {
            string data;
            if (opts.data) {
                data = opts.data;
            } else if (opts.path) {
                data = File::readTextFile(opts.path);
            } else {
                throw "HL7V2-READ-ERROR", "one of `path` or `data` must be provided";
            }
            Hl7v2Base base();
            message = base.parseMessage(data);
        }

        #! Returns the provider name
        string getName() {
            return "hl7v2read";
        }

        private hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
            return Hl7v2SegmentRecordIterator::getSegmentRecordType();
        }

        private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
                *hash<auto> search_options) {
            return new Hl7v2SegmentRecordIterator(message.segments, where_cond, search_options);
        }

        private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<DataProviderExpression> where_cond,
                *hash<auto> search_options) {
            return new Hl7v2SegmentRecordIterator(message.segments, where_cond, search_options);
        }
    }

    #! HL7 v2 write data provider
    public class Hl7v2WriteDataProvider inherits DataProvider::AbstractDataProvider {
        public {
            const ConstructorOptions = {
                "path": <DataProviderOptionInfo>{
                    "display_name": "File Path",
                    "short_desc": "Path to the HL7 v2 output file",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Path to write the serialized HL7 v2 message.",
                    "required": True,
                },
            };

            const ProviderInfo = <DataProviderInfo>{
                "name": "hl7v2write",
                "desc": "HL7 v2 write data provider",
                "type": "Hl7v2WriteDataProvider",
                "supports_create": True,
                "constructor_options": ConstructorOptions,
            };
        }

        private {
            string path;
        }

        #! Creates the write provider from options
        constructor(*hash<auto> opts) {
            if (!opts.path) {
                throw "HL7V2-WRITE-ERROR", "`path` must be provided";
            }
            path = opts.path;
        }

        #! Returns the provider name
        string getName() {
            return "hl7v2write";
        }

        private hash<DataProviderInfo> getStaticInfoImpl() {
            return ProviderInfo;
        }

        private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
            Hl7v2Base base();
            string text;
            if (exists rec.message) {
                text = base.formatMessage(rec.message);
            } else if (exists rec.segments) {
                text = base.formatMessage(<Hl7v2MessageInfo>{
                    "delimiters": base.getDelimiters().getInfo(),
                    "segments": rec.segments,
                });
            } else if (exists rec.tag && exists rec.fields) {
                text = base.formatSegment(rec) + "\r";
            } else {
                throw "HL7V2-WRITE-ERROR", "record must contain `message`, `segments`, or `tag` and `fields`";
            }
            File f();
            f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
            f.write(text);
            return rec;
        }
    }

    #! Segment record iterator
    class Hl7v2SegmentRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
        private {
            list<hash<Hl7v2Segment>> segments;
            *hash<auto> where_cond;
            *hash<auto> search_options;
            int index = -1;
        }

        constructor(list<hash<Hl7v2Segment>> segments, *hash<auto> where_cond, *hash<auto> search_options)
                : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
            self.segments = segments;
            self.where_cond = where_cond;
            self.search_options = search_options;
        }

        bool valid() {
            return index >= 0 && index < segments.size();
        }

        hash<auto> getValue() {
            if (!valid()) {
                throw "INVALID-ITERATOR", "the HL7 v2 segment iterator is not valid";
            }
            return segments[index];
        }

        *hash<string, DataProvider::AbstractDataField> getRecordType() {
            return Hl7v2SegmentRecordIterator::getSegmentRecordType();
        }

        static hash<string, DataProvider::AbstractDataField> getSegmentRecordType() {
            return {
                "tag": new DataProvider::QoreDataField("tag", "Segment tag", StringType),
                "index": new DataProvider::QoreDataField("index", "Segment index", IntType),
                "fields": new DataProvider::QoreDataField("fields", "Parsed segment fields", AutoListType),
                "raw": new DataProvider::QoreDataField("raw", "Raw segment text", StringType),
            };
        }

        private bool nextImpl() {
            while (++index < segments.size()) {
                if (!where_cond || matchGeneric(segments[index], where_cond)) {
                    return True;
                }
            }
            return False;
        }
    }

    #! Factory for HL7 v2 read data provider
    public class Hl7v2ReadDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "hl7v2read",
                "desc": "HL7 v2 read data provider factory",
                "children_can_support_records": True,
            };
        }

        string getName() {
            return "hl7v2read";
        }

        private hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        private hash<DataProviderInfo> getProviderInfoImpl() {
            return Hl7v2ReadDataProvider::ProviderInfo;
        }

        private Class getClassImpl() {
            return Class::forName("Hl7v2Util::Hl7v2ReadDataProvider");
        }
    }

    #! Factory for HL7 v2 write data provider
    public class Hl7v2WriteDataProviderFactory inherits DataProvider::AbstractDataProviderFactory {
        public {
            const FactoryInfo = <DataProviderFactoryInfo>{
                "name": "hl7v2write",
                "desc": "HL7 v2 write data provider factory",
                "children_can_support_records": True,
            };
        }

        string getName() {
            return "hl7v2write";
        }

        private hash<DataProviderFactoryInfo> getInfoImpl() {
            return FactoryInfo;
        }

        private hash<DataProviderInfo> getProviderInfoImpl() {
            return Hl7v2WriteDataProvider::ProviderInfo;
        }

        private Class getClassImpl() {
            return Class::forName("Hl7v2Util::Hl7v2WriteDataProvider");
        }
    }
}
