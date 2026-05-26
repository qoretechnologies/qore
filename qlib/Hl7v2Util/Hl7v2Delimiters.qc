# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2Delimiters.qc HL7 v2 delimiter support

/*  Hl7v2Delimiters.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! HL7 v2 delimiter information
    public hashdecl Hl7v2DelimiterInfo {
        string field_separator;
        string component_separator;
        string repetition_separator;
        string escape_character;
        string subcomponent_separator;
    }

    #! HL7 v2 delimiter configuration
    public class Hl7v2Delimiters {
        public {
            const DefaultFieldSeparator = "|";
            const DefaultEncodingCharacters = "^~\\&";
        }

        private {
            string field_separator = DefaultFieldSeparator;
            string component_separator = "^";
            string repetition_separator = "~";
            string escape_character = "\\";
            string subcomponent_separator = "&";
        }

        #! Creates default HL7 v2 delimiters
        constructor() {
        }

        #! Creates delimiters from the given MSH segment
        constructor(string msh_segment) {
            parseMshSegment(msh_segment);
        }

        #! Creates delimiters from explicit separator values
        constructor(string field_separator, string encoding_characters) {
            set(field_separator, encoding_characters);
        }

        #! Sets delimiter values from MSH-1 and MSH-2
        set(string field_separator, string encoding_characters) {
            if (field_separator.size() != 1) {
                throw "HL7V2-DELIMITER-ERROR", sprintf("MSH-1 field separator must be one character: %y",
                    field_separator);
            }
            if (encoding_characters.size() < 4) {
                throw "HL7V2-DELIMITER-ERROR", sprintf("MSH-2 encoding characters must have at least four "
                    "characters: %y", encoding_characters);
            }
            self.field_separator = field_separator;
            component_separator = encoding_characters[0];
            repetition_separator = encoding_characters[1];
            escape_character = encoding_characters[2];
            subcomponent_separator = encoding_characters[3];
        }

        #! Parses delimiter values from an MSH segment
        parseMshSegment(string msh_segment) {
            if (msh_segment.size() < 8 || msh_segment.substr(0, 3) != "MSH") {
                throw "HL7V2-DELIMITER-ERROR", sprintf("invalid MSH segment for delimiter parsing: %y",
                    msh_segment);
            }
            set(msh_segment[3], msh_segment.substr(4, 4));
        }

        #! Returns the field separator
        string getFieldSeparator() {
            return field_separator;
        }

        #! Returns the component separator
        string getComponentSeparator() {
            return component_separator;
        }

        #! Returns the repetition separator
        string getRepetitionSeparator() {
            return repetition_separator;
        }

        #! Returns the escape character
        string getEscapeCharacter() {
            return escape_character;
        }

        #! Returns the subcomponent separator
        string getSubcomponentSeparator() {
            return subcomponent_separator;
        }

        #! Returns MSH-2 encoding characters
        string getEncodingCharacters() {
            return component_separator + repetition_separator + escape_character + subcomponent_separator;
        }

        #! Returns delimiter information as a hash
        hash<Hl7v2DelimiterInfo> getInfo() {
            return <Hl7v2DelimiterInfo>{
                "field_separator": field_separator,
                "component_separator": component_separator,
                "repetition_separator": repetition_separator,
                "escape_character": escape_character,
                "subcomponent_separator": subcomponent_separator,
            };
        }

        #! Returns True if \a ch is any active HL7 delimiter or escape character
        bool isDelimiterCharacter(string ch) {
            return ch == field_separator || ch == component_separator || ch == repetition_separator
                || ch == escape_character || ch == subcomponent_separator;
        }

        #! Escapes a scalar HL7 field value
        string escape(string str) {
            string rv = "";
            for (int i = 0; i < str.size(); i++) {
                string ch = str[i];
                if (ch == field_separator) {
                    rv += escape_character + "F" + escape_character;
                } else if (ch == component_separator) {
                    rv += escape_character + "S" + escape_character;
                } else if (ch == repetition_separator) {
                    rv += escape_character + "R" + escape_character;
                } else if (ch == subcomponent_separator) {
                    rv += escape_character + "T" + escape_character;
                } else if (ch == escape_character) {
                    rv += escape_character + "E" + escape_character;
                } else if (ch == "\n") {
                    rv += escape_character + ".br" + escape_character;
                } else {
                    rv += ch;
                }
            }
            return rv;
        }

        #! Unescapes a scalar HL7 field value
        string unescape(string str) {
            string rv = "";
            for (int i = 0; i < str.size(); i++) {
                string ch = str[i];
                if (ch != escape_character) {
                    rv += ch;
                    continue;
                }

                int end = str.find(escape_character, i + 1);
                if (end < 0) {
                    throw "HL7V2-ESCAPE-ERROR", sprintf("unterminated HL7 escape sequence in %y", str);
                }

                string code = str.substr(i + 1, end - i - 1);
                rv += decodeEscapeCode(code);
                i = end;
            }
            return rv;
        }

        #! Decodes one HL7 escape code
        private string decodeEscapeCode(string code) {
            switch (code) {
                case "F":
                    return field_separator;
                case "S":
                    return component_separator;
                case "R":
                    return repetition_separator;
                case "T":
                    return subcomponent_separator;
                case "E":
                    return escape_character;
                case ".br":
                    return "\n";
            }
            if (code =~ /^X(?:[0-9A-Fa-f]{2})+$/) {
                string rv = "";
                for (int i = 1; i < code.size(); i += 2) {
                    rv += chr(code.substr(i, 2).toInt(16));
                }
                return rv;
            }
            return escape_character + code + escape_character;
        }
    }
}
