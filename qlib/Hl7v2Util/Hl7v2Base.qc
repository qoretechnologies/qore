# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2Base.qc HL7 v2 parsing and serialization support

/*  Hl7v2Base.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Parsed HL7 v2 field data
    public hashdecl Hl7v2Field {
        string raw;
        list<auto> repetitions;
    }

    #! Parsed HL7 v2 segment data
    public hashdecl Hl7v2Segment {
        string tag;
        int index;
        list<hash<Hl7v2Field>> fields;
        string raw;
    }

    #! Parsed HL7 v2 message data
    public hashdecl Hl7v2MessageInfo {
        hash<Hl7v2DelimiterInfo> delimiters;
        list<hash<Hl7v2Segment>> segments;
    }

    #! HL7 v2 parser and serializer base class
    public class Hl7v2Base {
        private {
            Hl7v2Delimiters delimiters;
        }

        #! Creates a parser with default delimiters
        constructor() {
            delimiters = new Hl7v2Delimiters();
        }

        #! Creates a parser with explicit delimiters
        constructor(Hl7v2Delimiters delimiters) {
            self.delimiters = delimiters;
        }

        #! Returns current delimiter configuration
        Hl7v2Delimiters getDelimiters() {
            return delimiters;
        }

        #! Sets current delimiters from parsed delimiter metadata
        setDelimiters(hash<Hl7v2DelimiterInfo> info) {
            delimiters = new Hl7v2Delimiters(info.field_separator,
                info.component_separator + info.repetition_separator + info.escape_character
                    + info.subcomponent_separator);
        }

        #! Parses a complete HL7 v2 message
        hash<Hl7v2MessageInfo> parseMessage(string data) {
            list<string> raw_segments = splitMessageSegments(data);
            if (!raw_segments) {
                throw "HL7V2-PARSE-ERROR", "HL7 v2 message has no segments";
            }
            if (raw_segments[0].substr(0, 3) == "MSH") {
                delimiters = new Hl7v2Delimiters(raw_segments[0]);
            }

            list<hash<Hl7v2Segment>> segments = ();
            int index = 0;
            foreach string raw_segment in (raw_segments) {
                ++index;
                push segments, parseSegment(raw_segment, index);
            }
            return <Hl7v2MessageInfo>{
                "delimiters": delimiters.getInfo(),
                "segments": segments,
            };
        }

        #! Parses a single HL7 v2 segment
        hash<Hl7v2Segment> parseSegment(string segment, int index = 1) {
            if (segment.size() < 3) {
                throw "HL7V2-PARSE-ERROR", sprintf("segment %d is too short", index);
            }

            string tag = segment.substr(0, 3);
            validateSegmentTag(tag);

            list<hash<Hl7v2Field>> fields = ();
            if (tag == "MSH") {
                if (segment.size() < 8) {
                    throw "HL7V2-PARSE-ERROR", sprintf("MSH segment %d is missing MSH-1/MSH-2 delimiters",
                        index);
                }
                Hl7v2Delimiters msh_delimiters(segment);
                delimiters = msh_delimiters;
                push fields, makeLiteralField(delimiters.getFieldSeparator());
                push fields, makeLiteralField(delimiters.getEncodingCharacters());
                if (segment.size() > 8) {
                    string rest = segment.substr(8);
                    if (rest[0] == delimiters.getFieldSeparator()) {
                        rest = rest.substr(1);
                    }
                    if (rest.size()) {
                        foreach string raw_field in (splitEscaped(rest, delimiters.getFieldSeparator())) {
                            push fields, parseField(raw_field);
                        }
                    }
                }
            } else if (segment.size() > 3) {
                if (segment[3] != delimiters.getFieldSeparator()) {
                    throw "HL7V2-PARSE-ERROR", sprintf("segment %d (%s) does not use the active field "
                        "separator", index, tag);
                }
                string rest = segment.substr(4);
                foreach string raw_field in (splitEscaped(rest, delimiters.getFieldSeparator())) {
                    push fields, parseField(raw_field);
                }
            }

            return <Hl7v2Segment>{
                "tag": tag,
                "index": index,
                "fields": fields,
                "raw": segment,
            };
        }

        #! Parses one field into repetitions, components, and subcomponents
        hash<Hl7v2Field> parseField(string raw_field) {
            list<auto> repetitions = ();
            foreach string raw_repetition in (splitEscaped(raw_field, delimiters.getRepetitionSeparator())) {
                list<auto> components = ();
                foreach string raw_component in (splitEscaped(raw_repetition, delimiters.getComponentSeparator())) {
                    if (containsUnescaped(raw_component, delimiters.getSubcomponentSeparator())) {
                        list<string> subcomponents = ();
                        foreach string raw_subcomponent in (splitEscaped(raw_component,
                                delimiters.getSubcomponentSeparator())) {
                            push subcomponents, delimiters.unescape(raw_subcomponent);
                        }
                        push components, subcomponents;
                    } else {
                        push components, delimiters.unescape(raw_component);
                    }
                }
                push repetitions, components;
            }
            return <Hl7v2Field>{
                "raw": raw_field,
                "repetitions": repetitions,
            };
        }

        #! Formats a parsed message
        string formatMessage(hash<Hl7v2MessageInfo> message) {
            setDelimiters(message.delimiters);
            list<string> segments = ();
            foreach hash<Hl7v2Segment> segment in (message.segments) {
                push segments, formatSegment(segment);
            }
            return segments ? segments.join("\r") + "\r" : "";
        }

        #! Formats a parsed or simple segment
        string formatSegment(hash<auto> segment) {
            string tag = segment.tag;
            validateSegmentTag(tag);
            if (exists segment.raw && exists segment.fields) {
                return formatParsedSegment(tag, segment.fields);
            }
            if (exists segment.fields) {
                return formatSimpleSegment(tag, segment.fields);
            }
            throw "HL7V2-WRITE-ERROR", "segment must have `tag` and `fields` keys";
        }

        #! Returns a field by one-based HL7 field number
        *hash<Hl7v2Field> getField(hash<Hl7v2Segment> segment, int field_number) {
            if (field_number < 1) {
                throw "HL7V2-ADDRESS-ERROR", sprintf("HL7 field numbers are one-based: %d", field_number);
            }
            int offset = field_number - 1;
            return offset < segment.fields.size() ? segment.fields[offset] : NOTHING;
        }

        #! Returns a field or component value by one-based indexes
        *string getFieldValue(hash<Hl7v2Segment> segment, int field_number, *int repetition = NOTHING,
                *int component = NOTHING, *int subcomponent = NOTHING) {
            *hash<Hl7v2Field> field = getField(segment, field_number);
            if (!field) {
                return NOTHING;
            }
            if (!component) {
                return delimiters.unescape(field.raw);
            }

            int repetition_index = (repetition ?? 1) - 1;
            int component_index = component - 1;
            int subcomponent_index = (subcomponent ?? 1) - 1;
            if (repetition_index < 0 || component_index < 0 || subcomponent_index < 0) {
                throw "HL7V2-ADDRESS-ERROR", "HL7 repetition, component, and subcomponent indexes are one-based";
            }
            if (repetition_index >= field.repetitions.size()) {
                return NOTHING;
            }
            auto rep = field.repetitions[repetition_index];
            if (component_index >= rep.size()) {
                return NOTHING;
            }
            auto comp = rep[component_index];
            if (comp.typeCode() == NT_LIST) {
                return subcomponent_index < comp.size() ? string(comp[subcomponent_index]) : NOTHING;
            }
            return subcomponent_index == 0 ? string(comp) : NOTHING;
        }

        #! Returns the first value matching an address such as \c "MSH-9" or \c "PID-3(2).1"
        *string getAddress(hash<Hl7v2MessageInfo> message, string address) {
            hash<auto> parsed = parseAddress(address);
            foreach hash<Hl7v2Segment> segment in (message.segments) {
                if (segment.tag == parsed.tag) {
                    return getFieldValue(segment, parsed.field, parsed.repetition, parsed.component,
                        parsed.subcomponent);
                }
            }
            return NOTHING;
        }

        #! Splits a message into raw segment strings
        list<string> splitMessageSegments(string data) {
            list<string> segments = ();
            string current = "";
            for (int i = 0; i < data.size(); i++) {
                string ch = data[i];
                if (ch == "\r" || ch == "\n") {
                    if (current.size()) {
                        push segments, current;
                        current = "";
                    }
                    continue;
                }
                current += ch;
            }
            if (current.size()) {
                push segments, current;
            }
            return segments;
        }

        #! Splits a string by a separator while respecting HL7 escape sequences
        list<string> splitEscaped(string str, string separator) {
            list<string> rv = ();
            string current = "";
            bool in_escape = False;

            for (int i = 0; i < str.size(); i++) {
                string ch = str[i];
                if (ch == delimiters.getEscapeCharacter()) {
                    in_escape = !in_escape;
                    current += ch;
                    continue;
                }
                if (!in_escape && ch == separator) {
                    push rv, current;
                    current = "";
                    continue;
                }
                current += ch;
            }

            if (in_escape) {
                throw "HL7V2-ESCAPE-ERROR", sprintf("unterminated HL7 escape sequence in value of length %d",
                    str.size());
            }
            push rv, current;
            return rv;
        }

        #! Returns True if a string contains the given unescaped separator
        bool containsUnescaped(string str, string separator) {
            bool in_escape = False;
            for (int i = 0; i < str.size(); i++) {
                string ch = str[i];
                if (ch == delimiters.getEscapeCharacter()) {
                    in_escape = !in_escape;
                    continue;
                }
                if (!in_escape && ch == separator) {
                    return True;
                }
            }
            if (in_escape) {
                throw "HL7V2-ESCAPE-ERROR", sprintf("unterminated HL7 escape sequence in value of length %d",
                    str.size());
            }
            return False;
        }

        #! Validates an HL7 segment tag
        static validateSegmentTag(string tag) {
            if (tag !~ /^[A-Z0-9]{3}$/) {
                throw "HL7V2-PARSE-ERROR", sprintf("invalid HL7 v2 segment tag: %y", tag);
            }
        }

        #! Parses an HL7 address string
        static hash<auto> parseAddress(string address) {
            *list<*string> parts = regex_extract(address,
                "^([A-Z0-9]{3})-([0-9]+)(?:\\(([0-9]+)\\))?(?:\\.([0-9]+))?(?:\\.([0-9]+))?$");
            if (!parts) {
                throw "HL7V2-ADDRESS-ERROR", sprintf("invalid HL7 v2 address: %y", address);
            }
            return {
                "tag": parts[0],
                "field": int(parts[1]),
                "repetition": parts[2] ? int(parts[2]) : NOTHING,
                "component": parts[3] ? int(parts[3]) : NOTHING,
                "subcomponent": parts[4] ? int(parts[4]) : NOTHING,
            };
        }

        private hash<Hl7v2Field> makeLiteralField(string value) {
            return <Hl7v2Field>{
                "raw": value,
                "repetitions": ((value,),),
            };
        }

        private string formatParsedSegment(string tag, list<hash<Hl7v2Field>> fields) {
            if (tag == "MSH") {
                if (fields.size() < 2) {
                    throw "HL7V2-WRITE-ERROR", "MSH segments require MSH-1 and MSH-2 fields";
                }
                string field_separator = fields[0].raw;
                list<string> raw_fields = ();
                for (int i = 1; i < fields.size(); i++) {
                    push raw_fields, fields[i].raw;
                }
                return "MSH" + field_separator + raw_fields.join(field_separator);
            }
            list<string> raw_fields = ();
            foreach hash<Hl7v2Field> field in (fields) {
                push raw_fields, field.raw;
            }
            return tag + delimiters.getFieldSeparator() + raw_fields.join(delimiters.getFieldSeparator());
        }

        private string formatSimpleSegment(string tag, list<auto> fields) {
            if (tag == "MSH") {
                if (!fields) {
                    throw "HL7V2-WRITE-ERROR", "MSH segments require MSH-2 encoding characters";
                }
                string encoding_characters = string(fields[0]);
                if (encoding_characters.size() < 4) {
                    throw "HL7V2-WRITE-ERROR", sprintf("MSH-2 encoding characters must have at least four "
                        "characters: %y", encoding_characters);
                }
                list<string> msh_out = ("MSH" + delimiters.getFieldSeparator() + encoding_characters,);
                for (int i = 1; i < fields.size(); i++) {
                    push msh_out, formatSimpleField(fields[i]);
                }
                return msh_out.join(delimiters.getFieldSeparator());
            }

            list<string> out = (tag,);
            foreach auto field in (fields) {
                push out, formatSimpleField(field);
            }
            return out.join(delimiters.getFieldSeparator());
        }

        private string formatSimpleField(auto field) {
            if (field.typeCode() == NT_LIST) {
                list<string> components = ();
                foreach auto component in (field) {
                    if (component.typeCode() == NT_LIST) {
                        list<string> subcomponents = ();
                        foreach auto subcomponent in (component) {
                            push subcomponents, delimiters.escape(string(subcomponent));
                        }
                        push components, subcomponents.join(delimiters.getSubcomponentSeparator());
                    } else {
                        push components, delimiters.escape(string(component));
                    }
                }
                return components.join(delimiters.getComponentSeparator());
            }
            return delimiters.escape(string(field));
        }
    }

    #! Parsed HL7 v2 message wrapper
    public class Hl7v2Message {
        private {
            Hl7v2Base base;
            hash<Hl7v2MessageInfo> message;
        }

        #! Parses the given HL7 v2 message text
        constructor(string data) {
            base = new Hl7v2Base();
            message = base.parseMessage(data);
        }

        #! Creates a message wrapper from parsed message data
        constructor(hash<Hl7v2MessageInfo> message) {
            base = new Hl7v2Base();
            base.setDelimiters(message.delimiters);
            self.message = message;
        }

        #! Returns parsed message data
        hash<Hl7v2MessageInfo> getMessage() {
            return message;
        }

        #! Returns parsed segments
        list<hash<Hl7v2Segment>> getSegments() {
            return message.segments;
        }

        #! Returns a value by address
        *string getAddress(string address) {
            return base.getAddress(message, address);
        }

        #! Returns serialized message text with CR segment delimiters
        string getText() {
            return base.formatMessage(message);
        }
    }
}
