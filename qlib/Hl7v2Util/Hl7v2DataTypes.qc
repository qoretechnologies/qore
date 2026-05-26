# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2DataTypes.qc HL7 v2 DataProvider data type metadata

/*  Hl7v2DataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! DataProvider metadata for HL7 v2 delimiter settings
    public class Hl7v2DelimiterInfoDataType inherits HashDataType {
        private {
            const Fields = {
                "field_separator": {
                    "display_name": "Field Separator",
                    "short_desc": "Character separating fields",
                    "desc": "The HL7 v2 field separator from `MSH-1`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "|",
                },
                "component_separator": {
                    "display_name": "Component Separator",
                    "short_desc": "Character separating components",
                    "desc": "The HL7 v2 component separator from `MSH-2`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "^",
                },
                "repetition_separator": {
                    "display_name": "Repetition Separator",
                    "short_desc": "Character separating repetitions",
                    "desc": "The HL7 v2 repetition separator from `MSH-2`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "~",
                },
                "escape_character": {
                    "display_name": "Escape Character",
                    "short_desc": "Character introducing escape sequences",
                    "desc": "The HL7 v2 escape character from `MSH-2`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "\\",
                },
                "subcomponent_separator": {
                    "display_name": "Subcomponent Separator",
                    "short_desc": "Character separating subcomponents",
                    "desc": "The HL7 v2 subcomponent separator from `MSH-2`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "&",
                },
            };
        }

        constructor() : HashDataType("Hl7v2DelimiterInfo") {
            addQoreFields(Fields);
        }
    }

    #! Singleton delimiter metadata data type
    public const Hl7v2DelimiterInfoDataType = new Hl7v2DelimiterInfoDataType();

    #! DataProvider metadata for one parsed HL7 v2 field
    public class Hl7v2FieldDataType inherits HashDataType {
        private {
            const Fields = {
                "raw": {
                    "display_name": "Raw Field",
                    "short_desc": "Original field text",
                    "desc": "The original escaped HL7 v2 field text before component parsing.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "Doe^Jane",
                },
                "repetitions": {
                    "display_name": "Repetitions",
                    "short_desc": "Parsed repetitions and components",
                    "desc": "Parsed field values as `repetition -> component -> subcomponent` lists. "
                        "Leaf values are unescaped strings.",
                    "type": AbstractDataProviderTypeMap."list",
                },
            };
        }

        constructor() : HashDataType("Hl7v2Field") {
            addQoreFields(Fields);
        }
    }

    #! Singleton field metadata data type
    public const Hl7v2FieldDataType = new Hl7v2FieldDataType();

    #! DataProvider metadata for one parsed HL7 v2 segment record
    public class Hl7v2SegmentDataType inherits HashDataType {
        private {
            const Fields = {
                "tag": {
                    "display_name": "Segment Tag",
                    "short_desc": "Three-character HL7 segment tag",
                    "desc": "The three-character HL7 v2 segment tag, for example `MSH`, `PID`, or `OBX`.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "PID",
                },
                "index": {
                    "display_name": "Segment Index",
                    "short_desc": "One-based segment position",
                    "desc": "The one-based position of the segment in the message.",
                    "type": AbstractDataProviderTypeMap."int",
                },
                "fields": {
                    "display_name": "Fields",
                    "short_desc": "Parsed HL7 fields",
                    "desc": "Parsed fields for the segment. For `MSH`, `fields[0]` is `MSH-1` and "
                        "`fields[1]` is `MSH-2`; other segments start with field `1`.",
                    "type": new ListDataType(Hl7v2FieldDataType),
                },
                "raw": {
                    "display_name": "Raw Segment",
                    "short_desc": "Original segment text",
                    "desc": "The original segment text without the trailing message segment separator.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "PID|1||12345^^^MRN^MR||Doe^Jane",
                },
            };
        }

        constructor() : HashDataType("Hl7v2Segment") {
            addQoreFields(Fields);
        }
    }

    #! Singleton segment metadata data type
    public const Hl7v2SegmentDataType = new Hl7v2SegmentDataType();

    #! DataProvider metadata for a parsed HL7 v2 message
    public class Hl7v2MessageDataType inherits HashDataType {
        private {
            const Fields = {
                "delimiters": {
                    "display_name": "Delimiters",
                    "short_desc": "Message delimiter settings",
                    "desc": "Delimiter settings parsed from the message `MSH` segment.",
                    "type": Hl7v2DelimiterInfoDataType,
                },
                "segments": {
                    "display_name": "Segments",
                    "short_desc": "Parsed message segments",
                    "desc": "Parsed message segments in source order.",
                    "type": new ListDataType(Hl7v2SegmentDataType),
                },
            };
        }

        constructor() : HashDataType("Hl7v2Message") {
            addQoreFields(Fields);
        }
    }

    #! Singleton message metadata data type
    public const Hl7v2MessageDataType = new Hl7v2MessageDataType();

    #! DataProvider metadata for records accepted by the write provider
    public class Hl7v2WriteRecordDataType inherits HashDataType {
        public {
            const Fields = {
                "message": {
                    "display_name": "Message",
                    "short_desc": "Parsed message to serialize",
                    "desc": "A parsed HL7 v2 message hash with `delimiters` and `segments` keys. Use this "
                        "when round-tripping a parsed message or preserving custom delimiters.",
                    "type": Hl7v2MessageDataType,
                },
                "segments": {
                    "display_name": "Segments",
                    "short_desc": "Parsed segments to serialize",
                    "desc": "Parsed HL7 v2 segments to serialize with default delimiters.",
                    "type": new ListDataType(Hl7v2SegmentDataType),
                },
                "tag": {
                    "display_name": "Segment Tag",
                    "short_desc": "Single segment tag",
                    "desc": "The three-character HL7 v2 segment tag for a single segment record.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "PID",
                },
                "fields": {
                    "display_name": "Fields",
                    "short_desc": "Single segment fields",
                    "desc": "Field values for a single segment record. Each item may be a scalar value or "
                        "a nested component list.",
                    "type": AbstractDataProviderTypeMap."list",
                },
            };
        }

        constructor() : HashDataType("Hl7v2WriteRecord") {
            addQoreFields(Fields);
        }
    }

    #! Singleton write request metadata data type
    public const Hl7v2WriteRecordDataType = new Hl7v2WriteRecordDataType();

    #! DataProvider metadata for write action responses
    public class Hl7v2WriteResultDataType inherits HashDataType {
        private {
            const Fields = {
                "path": {
                    "display_name": "File Path",
                    "short_desc": "Path written",
                    "desc": "The output file path that received the serialized HL7 v2 message.",
                    "type": AbstractDataProviderTypeMap."string",
                    "example_value": "/tmp/out.hl7",
                },
                "characters": {
                    "display_name": "Characters Written",
                    "short_desc": "Serialized character count",
                    "desc": "The number of characters written to the output file.",
                    "type": AbstractDataProviderTypeMap."int",
                },
                "segments": {
                    "display_name": "Segments Written",
                    "short_desc": "Serialized segment count",
                    "desc": "The number of HL7 v2 segments serialized to the output file.",
                    "type": AbstractDataProviderTypeMap."int",
                },
            };
        }

        constructor() : HashDataType("Hl7v2WriteResult") {
            addQoreFields(Fields);
        }
    }

    #! Singleton write response metadata data type
    public const Hl7v2WriteResultDataType = new Hl7v2WriteResultDataType();
}
