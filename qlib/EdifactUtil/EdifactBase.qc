# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactBase.qc EDIFACT base class with core parsing utilities

/*  EdifactBase.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Hashdecl for a parsed EDIFACT segment
    public hashdecl EdifactSegment {
        #! The segment tag (3 uppercase letters, e.g., "BGM", "DTM", "NAD")
        string tag;

        #! The parsed data elements (list of strings or lists for composite elements)
        list<auto> data_elements;

        #! The raw segment string (without terminator)
        string raw;
    }

    #! Hashdecl for iterator return value
    public hashdecl EdifactRecord {
        #! The segment type/tag
        string type;

        #! The parsed record data
        hash<auto> record;
    }

    #! Base class for EDIFACT parsing and generation
    /** This class provides core utility methods for parsing and generating EDIFACT messages,
        including splitting strings while respecting release characters.
    */
    public class EdifactBase {
        public {
            #! Delimiter configuration
            EdifactDelimiters delimiters;
        }

        private {
            #! Global options
            *hash<auto> m_opts;
        }

        #! Creates an EdifactBase object with default delimiters
        constructor() {
            delimiters = new EdifactDelimiters();
        }

        #! Creates an EdifactBase object with specified delimiters
        /** @param delim the delimiter configuration
        */
        constructor(EdifactDelimiters delim) {
            delimiters = delim;
        }

        #! Creates an EdifactBase object with specified options
        /** @param opts options hash
        */
        constructor(*hash<auto> opts) {
            delimiters = new EdifactDelimiters();
            m_opts = opts;
        }

        #! Splits a string by a separator while respecting the release character
        /** @param str the string to split
            @param separator the separator character
            @param final_unescape if True, unescape all release sequences in the result (default: True)
            @return a list of split parts

            @par Example
            @code{.py}
EdifactBase base();
list<string> parts = base.splitWithRelease("A+B?+C+D", "+");
# Returns: ("A", "B+C", "D")
            @endcode
        */
        list<string> splitWithRelease(string str, string separator, bool final_unescape = True) {
            list<string> result = ();
            string current = "";
            bool escaped = False;

            for (int i = 0; i < str.size(); i++) {
                string char = str[i];

                if (escaped) {
                    # Previous character was release character, so this character is literal
                    # Only unescape the specific separator being split; keep other escapes intact
                    if (char != separator && !final_unescape) {
                        # Preserve the release character for non-separator escapes
                        current += delimiters.release_character;
                    }
                    current += char;
                    escaped = False;
                    continue;
                }

                if (char == delimiters.release_character) {
                    # This is a release character, next character will be literal
                    escaped = True;
                    continue;
                }

                if (char == separator) {
                    # Found separator, add current part to result
                    push result, current;
                    current = "";
                    continue;
                }

                current += char;
            }

            # Add the last part
            push result, current;

            return result;
        }

        #! Parses a raw segment string into tag and elements
        /** @param segment_str the segment string (without terminator)
            @return a hash with tag and elements

            @par Example
            @code{.py}
EdifactBase base();
hash<EdifactSegment> seg = base.parseSegment("BGM+220+PO12345+9");
# Returns: {"tag": "BGM", "elements": (("220",), ("PO12345",), ("9",)), "raw": "BGM+220+PO12345+9"}
            @endcode
        */
        hash<EdifactSegment> parseSegment(string segment_str) {
            # Trim any whitespace
            segment_str = trim(segment_str);

            if (segment_str.empty()) {
                throw "EDIFACT-PARSE-ERROR", "empty segment string";
            }

            # Extract tag (first 3 characters)
            if (segment_str.size() < 3) {
                throw "EDIFACT-PARSE-ERROR", sprintf("segment too short: %y", segment_str);
            }

            string tag = segment_str.substr(0, 3);

            # Validate tag format (3 uppercase letters)
            if (!tag.regex("^[A-Z]{3}$")) {
                throw "EDIFACT-PARSE-ERROR", sprintf("invalid segment tag: %y (must be 3 uppercase letters)", tag);
            }

            @debug {
                if (getenv("QORE_EDIFACT_DEBUG")) {
                    printf("EdifactBase::parseSegment(): tag=%y len=%d\n", tag, segment_str.size());
                }
            }

            # Rest of the segment after tag
            string data = segment_str.substr(3);

            # Parse data elements
            list<auto> elems = ();

            if (data.size() > 0) {
                # Remove leading element separator if present
                if (data[0] == delimiters.element_separator) {
                    data = data.substr(1);
                }

                if (data.size() > 0) {
                    # Split by element separator, preserving escapes for component-level processing
                    list<string> raw_elems = splitWithRelease(data, delimiters.element_separator, False);

                    # Parse each element for composite components
                    foreach string elem in (raw_elems) {
                        push elems, parseElement(elem);
                    }
                }
            }

            return <EdifactSegment>{
                "tag": tag,
                "data_elements": elems,
                "raw": segment_str,
            };
        }

        #! Parses an element, splitting into components if composite
        /** @param elem_str the element string
            @return a list of components (single element list for simple elements)
        */
        list<string> parseElement(string elem_str) {
            # Check if this is a composite element
            if (containsUnescaped(elem_str, delimiters.component_separator)) {
                return splitWithRelease(elem_str, delimiters.component_separator);
            }
            # Simple element - unescape and return as single-element list for consistency
            return (delimiters.unescape(elem_str),);
        }

        #! Checks if a string contains an unescaped occurrence of a character
        /** @param str the string to check
            @param ch the character to look for
            @return True if the string contains an unescaped occurrence
        */
        bool containsUnescaped(string str, string ch) {
            bool escaped = False;
            for (int i = 0; i < str.size(); i++) {
                string c = str[i];
                if (escaped) {
                    escaped = False;
                    continue;
                }
                if (c == delimiters.release_character) {
                    escaped = True;
                    continue;
                }
                if (c == ch) {
                    return True;
                }
            }
            return False;
        }

        #! Generates a segment string from tag and elements
        /** @param tag the segment tag
            @param elems the elements (list of strings or lists for composites)
            @param include_terminator if True, append segment terminator
            @return the segment string

            @par Example
            @code{.py}
EdifactBase base();
string seg = base.generateSegment("BGM", (("220",), ("PO12345",), ("9",)));
# Returns: "BGM+220+PO12345+9'"
            @endcode
        */
        string generateSegment(string tag, list<auto> elems, bool include_terminator = True) {
            string result = tag;

            foreach auto elem in (elems) {
                result += delimiters.element_separator;
                if (elem.typeCode() == NT_LIST) {
                    # Composite element
                    list<string> escaped_components = ();
                    foreach auto comp in (elem) {
                        push escaped_components, delimiters.escape(string(comp));
                    }
                    result += escaped_components.join(delimiters.component_separator);
                } else {
                    # Simple element
                    result += delimiters.escape(string(elem));
                }
            }

            if (include_terminator) {
                result += delimiters.segment_terminator;
            }

            return result;
        }

        #! Splits raw EDIFACT data into individual segment strings
        /** @param data the raw EDIFACT data
            @return a list of segment strings (without terminators)

            @note This method handles the UNA segment specially if present
        */
        list<string> splitIntoSegments(string data) {
            list<string> segments = ();

            # Check for UNA segment at the start
            if (data.size() >= 9 && data.substr(0, 3) == UnaSegmentTag) {
                # UNA segment is exactly 9 characters, no terminator
                string una = data.substr(0, 9);
                push segments, una;
                data = data.substr(9);

                # Parse UNA to update delimiters
                delimiters.parseUna(una);
            }

            # Now split the rest by segment terminator
            string current = "";
            bool escaped = False;

            for (int i = 0; i < data.size(); i++) {
                string char = data[i];

                if (escaped) {
                    current += char;
                    escaped = False;
                    continue;
                }

                if (char == delimiters.release_character) {
                    current += char;
                    escaped = True;
                    continue;
                }

                if (char == delimiters.segment_terminator) {
                    # Found segment terminator
                    string segment = trim(current);
                    if (segment.size() > 0) {
                        push segments, segment;
                    }
                    current = "";
                    continue;
                }

                # Skip newlines and carriage returns (commonly used for readability)
                if (char == "\n" || char == "\r") {
                    continue;
                }

                current += char;
            }

            # Handle any remaining content (segment without terminator)
            string remaining = trim(current);
            if (remaining.size() > 0) {
                push segments, remaining;
            }

            return segments;
        }

        #! Checks if a segment tag is a service segment (UNA, UNB, UNZ, UNH, UNT, etc.)
        /** @param tag the segment tag
            @return True if the tag is a service segment
        */
        static bool isServiceSegment(string tag) {
            return ServiceSegmentTags{tag} ?? False;
        }

        #! Parses a UNB (interchange header) segment into a structured hash
        /** @param elems the parsed elements from the UNB segment
            @return a hash with interchange header information
        */
        static hash<auto> parseUnbSegment(list<auto> elems) {
            hash<auto> result;

            # Element 0: Syntax identifier (composite)
            if (elems.size() > 0 && elems[0].size() > 0) {
                result.syntax_identifier = elems[0][0];
                if (elems[0].size() > 1) {
                    result.syntax_version = elems[0][1];
                }
            }

            # Element 1: Interchange sender (composite)
            if (elems.size() > 1 && elems[1].size() > 0) {
                result.sender_id = elems[1][0];
                if (elems[1].size() > 1) {
                    result.sender_qualifier = elems[1][1];
                }
            }

            # Element 2: Interchange recipient (composite)
            if (elems.size() > 2 && elems[2].size() > 0) {
                result.recipient_id = elems[2][0];
                if (elems[2].size() > 1) {
                    result.recipient_qualifier = elems[2][1];
                }
            }

            # Element 3: Date/time of preparation (composite)
            if (elems.size() > 3 && elems[3].size() > 0) {
                result.prep_date = elems[3][0];
                if (elems[3].size() > 1) {
                    result.prep_time = elems[3][1];
                }
            }

            # Element 4: Interchange control reference
            if (elems.size() > 4 && elems[4].size() > 0) {
                result.control_reference = elems[4][0];
            }

            return result;
        }

        #! Parses a UNH (message header) segment into a structured hash
        /** @param elems the parsed elements from the UNH segment
            @return a hash with message header information
        */
        static hash<auto> parseUnhSegment(list<auto> elems) {
            hash<auto> result;

            # Element 0: Message reference number
            if (elems.size() > 0 && elems[0].size() > 0) {
                result.message_reference = elems[0][0];
            }

            # Element 1: Message identifier (composite)
            if (elems.size() > 1 && elems[1].size() > 0) {
                result.message_type = elems[1][0];
                if (elems[1].size() > 1) {
                    result.message_version = elems[1][1];
                }
                if (elems[1].size() > 2) {
                    result.message_release = elems[1][2];
                }
                if (elems[1].size() > 3) {
                    result.controlling_agency = elems[1][3];
                }
            }

            return result;
        }
    }
}
