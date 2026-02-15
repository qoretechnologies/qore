# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactDelimiters.qc EDIFACT delimiter configuration class

/*  EdifactDelimiters.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Hashdecl for delimiter configuration
    public hashdecl EdifactDelimiterInfo {
        #! Component data element separator (default: ":")
        string component_separator = DefaultComponentSeparator;

        #! Data element separator (default: "+")
        string element_separator = DefaultElementSeparator;

        #! Decimal notation character (default: ".")
        string decimal_notation = DefaultDecimalNotation;

        #! Release character / escape character (default: "?")
        string release_character = DefaultReleaseCharacter;

        #! Segment terminator (default: "'")
        string segment_terminator = DefaultSegmentTerminator;
    }

    #! EDIFACT delimiter configuration class
    /** This class handles parsing of the UNA segment and provides delimiter configuration
        for EDIFACT message parsing and generation.

        The UNA segment is optional and defines the special characters used in the message.
        If not present, default delimiters are used.

        @par UNA Segment Format
        The UNA segment is exactly 9 characters:
        @verbatim
        UNA:+.? '
           ||||| |
           ||||| +-- Segment terminator (position 8)
           ||||+---- Reserved (position 7, typically space)
           |||+----- Release character (position 6)
           ||+------ Decimal notation (position 5)
           |+------- Element separator (position 4)
           +-------- Component separator (position 3)
        @endverbatim

        @par Example
        @code{.py}
# Parse UNA segment
EdifactDelimiters delim("UNA:+.? '");
printf("Element separator: %y\n", delim.element_separator);  # "+"

# Use default delimiters
EdifactDelimiters default_delim();
printf("Segment terminator: %y\n", default_delim.segment_terminator);  # "'"
        @endcode
    */
    public class EdifactDelimiters {
        public {
            #! Component data element separator (separates components within composite elements)
            string component_separator = DefaultComponentSeparator;

            #! Data element separator (separates elements within a segment)
            string element_separator = DefaultElementSeparator;

            #! Decimal notation character
            string decimal_notation = DefaultDecimalNotation;

            #! Release character (escape character for special characters)
            string release_character = DefaultReleaseCharacter;

            #! Segment terminator
            string segment_terminator = DefaultSegmentTerminator;

            #! Flag indicating if UNA segment was present/parsed
            bool una_present = False;
        }

        #! Creates an EdifactDelimiters object with default delimiters
        /** @par Example
            @code{.py}
EdifactDelimiters delim();
            @endcode
        */
        constructor() {
        }

        #! Creates an EdifactDelimiters object from a UNA segment string
        /** @param una_segment the UNA segment string (must start with "UNA" and be at least 9 characters)

            @par Example
            @code{.py}
EdifactDelimiters delim("UNA:+.? '");
            @endcode

            @throw EDIFACT-DELIMITER-ERROR if the UNA segment is invalid
        */
        constructor(string una_segment) {
            @assert(una_segment.val(), "una_segment cannot be empty");
            parseUna(una_segment);
        }

        #! Creates an EdifactDelimiters object from a delimiter info hash
        /** @param info the delimiter configuration hash

            @par Example
            @code{.py}
EdifactDelimiters delim(<EdifactDelimiterInfo>{
    "component_separator": ":",
    "element_separator": "+",
    "segment_terminator": "'",
});
            @endcode
        */
        constructor(hash<EdifactDelimiterInfo> info) {
            component_separator = info.component_separator;
            element_separator = info.element_separator;
            decimal_notation = info.decimal_notation;
            release_character = info.release_character;
            segment_terminator = info.segment_terminator;
        }

        #! Parses a UNA segment and updates delimiter configuration
        /** @param una_segment the UNA segment string

            @throw EDIFACT-DELIMITER-ERROR if the UNA segment is invalid
        */
        parseUna(string una_segment) {
            if (una_segment.size() < 9) {
                throw "EDIFACT-DELIMITER-ERROR", sprintf("UNA segment too short: %d characters (minimum 9 required)",
                    una_segment.size());
            }

            if (una_segment.substr(0, 3) != UnaSegmentTag) {
                throw "EDIFACT-DELIMITER-ERROR", sprintf("invalid UNA segment: does not start with 'UNA': %y",
                    una_segment.substr(0, 3));
            }

            # UNA positions (0-indexed):
            # 0-2: "UNA"
            # 3: component separator
            # 4: element separator
            # 5: decimal notation
            # 6: release character
            # 7: reserved (typically space)
            # 8: segment terminator
            component_separator = una_segment[3];
            element_separator = una_segment[4];
            decimal_notation = una_segment[5];
            release_character = una_segment[6];
            # position 7 is reserved
            segment_terminator = una_segment[8];
            una_present = True;

            @debug {
                if (getenv("QORE_EDIFACT_DEBUG")) {
                    printf("EdifactDelimiters::parseUna(): comp=%y elem=%y dec=%y rel=%y term=%y\n",
                        component_separator, element_separator, decimal_notation,
                        release_character, segment_terminator);
                }
            }

            validateDelimiters();
        }

        #! Returns the delimiter configuration as a hash
        /** @return a hash with delimiter configuration
        */
        hash<EdifactDelimiterInfo> getInfo() {
            return <EdifactDelimiterInfo>{
                "component_separator": component_separator,
                "element_separator": element_separator,
                "decimal_notation": decimal_notation,
                "release_character": release_character,
                "segment_terminator": segment_terminator,
            };
        }

        #! Generates a UNA segment string from current delimiter configuration
        /** @return the UNA segment string

            @par Example
            @code{.py}
EdifactDelimiters delim();
string una = delim.generateUna();  # "UNA:+.? '"
            @endcode
        */
        string generateUna() {
            return sprintf("UNA%s%s%s%s %s", component_separator, element_separator,
                decimal_notation, release_character, segment_terminator);
        }

        #! Checks if the given character is a special delimiter character
        /** @param ch the character to check
            @return True if the character is a delimiter
        */
        bool isDelimiter(string ch) {
            return ch == component_separator
                || ch == element_separator
                || ch == release_character
                || ch == segment_terminator;
        }

        #! Escapes special characters in a value using the release character
        /** @param value the value to escape
            @return the escaped value

            @par Example
            @code{.py}
EdifactDelimiters delim();
string escaped = delim.escape("Price: $10.50");  # "Price?: $10.50" (if : is component sep)
            @endcode
        */
        string escape(string value) {
            string result = "";
            for (int i = 0; i < value.size(); i++) {
                string char = value[i];
                if (isDelimiter(char) || char == release_character) {
                    result += release_character;
                }
                result += char;
            }
            return result;
        }

        #! Unescapes a value by removing release characters
        /** @param value the value to unescape
            @return the unescaped value
        */
        string unescape(string value) {
            string result = "";
            bool escaped = False;
            for (int i = 0; i < value.size(); i++) {
                string char = value[i];
                if (!escaped && char == release_character) {
                    escaped = True;
                    continue;
                }
                result += char;
                escaped = False;
            }
            return result;
        }

        #! Validates that delimiters are distinct and valid
        /** @throw EDIFACT-DELIMITER-ERROR if delimiters are invalid
        */
        private validateDelimiters() {
            # Check that all delimiters are single characters
            if (component_separator.size() != 1) {
                throw "EDIFACT-DELIMITER-ERROR", "component separator must be exactly 1 character";
            }
            if (element_separator.size() != 1) {
                throw "EDIFACT-DELIMITER-ERROR", "element separator must be exactly 1 character";
            }
            if (decimal_notation.size() != 1) {
                throw "EDIFACT-DELIMITER-ERROR", "decimal notation must be exactly 1 character";
            }
            if (release_character.size() != 1) {
                throw "EDIFACT-DELIMITER-ERROR", "release character must be exactly 1 character";
            }
            if (segment_terminator.size() != 1) {
                throw "EDIFACT-DELIMITER-ERROR", "segment terminator must be exactly 1 character";
            }

            # Check that critical delimiters are distinct
            hash<string, bool> seen;
            list<string> critical = (component_separator, element_separator, release_character, segment_terminator);
            list<string> names = ("component_separator", "element_separator", "release_character", "segment_terminator");

            for (int i = 0; i < critical.size(); i++) {
                if (seen{critical[i]}) {
                    throw "EDIFACT-DELIMITER-ERROR", sprintf("%s '%s' conflicts with another delimiter",
                        names[i], critical[i]);
                }
                seen{critical[i]} = True;
            }
        }
    }
}
