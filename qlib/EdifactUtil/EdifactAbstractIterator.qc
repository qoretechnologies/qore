# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactAbstractIterator.qc EDIFACT abstract iterator base class

/*  EdifactAbstractIterator.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Abstract iterator options
    public const EdifactIteratorOptions = {
        "encoding": "The character encoding for the data",
        "skip_service_segments": "If True, skip UNA/UNB/UNZ/UNH/UNT segments in iteration (default: False)",
        "extended_record": "If True, return extended format with 'type' and 'record' keys (default: False)",
    };

    #! Abstract base iterator class for EDIFACT messages
    /** This class provides the base functionality for iterating over EDIFACT segments.

        @par Example
        @code{.py}
# Subclasses implement the actual data source
EdifactFileIterator i("orders.edi");
while (i.next()) {
    hash<auto> segment = i.getValue();
    printf("Segment: %s\n", segment.tag);
}
        @endcode
    */
    public class EdifactAbstractIterator inherits EdifactBase, Qore::AbstractIterator {
        private {
            #! List of parsed segment strings
            list<string> m_segments;

            #! Current position in segment list
            int m_index = -1;

            #! Current parsed segment
            *hash<EdifactSegment> m_current;

            #! Interchange header data (from UNB)
            *hash<auto> m_interchange;

            #! Current message header data (from UNH)
            *hash<auto> m_message;

            #! Skip service segments flag
            bool m_skip_service = False;

            #! Extended record format flag
            bool m_extended_record = False;

            #! Encoding for string conversion
            *string m_encoding;

            #! Segment count for current message
            int m_segment_count = 0;

            #! Message count for current interchange
            int m_message_count = 0;
        }

        #! Creates an EdifactAbstractIterator with default options
        constructor() : EdifactBase() {
        }

        #! Creates an EdifactAbstractIterator with specified options
        /** @param opts options hash; see @ref EdifactIteratorOptions for valid options
        */
        constructor(*hash<auto> opts) : EdifactBase(opts) {
            processOptions(opts);
        }

        #! Process options
        private processOptions(*hash<auto> opts) {
            if (opts.encoding) {
                m_encoding = opts.encoding;
            }
            if (opts.skip_service_segments) {
                m_skip_service = parse_boolean(opts.skip_service_segments);
            }
            if (opts.extended_record) {
                m_extended_record = parse_boolean(opts.extended_record);
            }
        }

        #! Initialize the iterator with raw EDIFACT data
        /** @param data the raw EDIFACT message data
        */
        private initializeFromData(string data) {
            @assert(data.val(), "EDIFACT data cannot be empty");
            # Split into segments (this also handles UNA parsing)
            m_segments = splitIntoSegments(data);
            m_index = -1;
            m_current = NOTHING;
            @debug(printf("EdifactAbstractIterator::initializeFromData(): parsed %d segments\n", m_segments.size()));
        }

        #! Returns True if the iterator is currently valid
        bool valid() {
            return m_index >= 0 && m_index < m_segments.size() && exists m_current;
        }

        #! Moves to the next segment
        /** @return True if there is a next segment, False if iteration is complete
        */
        bool next() {
            while (True) {
                m_index++;

                if (m_index >= m_segments.size()) {
                    m_current = NOTHING;
                    return False;
                }

                string segment_str = m_segments[m_index];

                # Handle UNA segment specially (it's not a normal segment)
                if (segment_str.size() >= 3 && segment_str.substr(0, 3) == UnaSegmentTag) {
                    if (m_skip_service) {
                        continue;
                    }
                    # Create a pseudo-segment for UNA
                    m_current = <EdifactSegment>{
                        "tag": UnaSegmentTag,
                        "data_elements": (),
                        "raw": segment_str,
                    };
                    return True;
                }

                # Parse the segment
                m_current = parseSegment(segment_str);
                m_segment_count++;
                @debug(printf("EdifactAbstractIterator::next(): parsed segment %s at index %d\n", m_current.tag, m_index));

                # Track interchange and message headers
                switch (m_current.tag) {
                    case UnbSegmentTag:
                        m_interchange = parseUnbSegment(m_current.data_elements);
                        m_message_count = 0;
                        break;
                    case UnhSegmentTag:
                        m_message = parseUnhSegment(m_current.data_elements);
                        m_segment_count = 1;
                        m_message_count++;
                        break;
                    case UntSegmentTag:
                        # Message trailer - could validate segment count here
                        break;
                    case UnzSegmentTag:
                        # Interchange trailer - could validate message count here
                        break;
                }

                # Skip service segments if configured
                if (m_skip_service && isServiceSegment(m_current.tag)) {
                    continue;
                }

                return True;
            }
        }

        #! Returns the current segment
        /** @return a hash with the current segment data

            If \c extended_record option is False (default):
            - \c tag: the segment tag
            - \c data_elements: list of elements (each element is a list of components)
            - \c raw: the raw segment string

            If \c extended_record option is True:
            - \c type: the segment tag
            - \c record: hash with tag, data_elements, raw
        */
        hash<auto> getValue() {
            if (!valid()) {
                throw "INVALID-ITERATOR", "iterator is not valid; call next() first";
            }

            if (m_extended_record) {
                return {
                    "type": m_current.tag,
                    "record": m_current,
                };
            }

            return m_current;
        }

        #! Returns the current segment tag
        /** @return the segment tag (e.g., "BGM", "DTM", "NAD")
        */
        string getTag() {
            if (!valid()) {
                throw "INVALID-ITERATOR", "iterator is not valid; call next() first";
            }
            return m_current.tag;
        }

        #! Returns the current segment's elements
        /** @return list of elements
        */
        list<auto> getElements() {
            if (!valid()) {
                throw "INVALID-ITERATOR", "iterator is not valid; call next() first";
            }
            return m_current.data_elements;
        }

        #! Returns the raw segment string
        /** @return the raw segment string (without terminator)
        */
        string getRawSegment() {
            if (!valid()) {
                throw "INVALID-ITERATOR", "iterator is not valid; call next() first";
            }
            return m_current.raw;
        }

        #! Returns the current index (0-based segment position)
        int index() {
            return m_index;
        }

        #! Returns the interchange header information (from UNB segment)
        /** @return a hash with interchange header data, or NOTHING if not yet parsed

            Keys include:
            - \c syntax_identifier: syntax identifier (e.g., "UNOC")
            - \c syntax_version: syntax version number
            - \c sender_id: interchange sender identification
            - \c sender_qualifier: sender identification qualifier
            - \c recipient_id: interchange recipient identification
            - \c recipient_qualifier: recipient identification qualifier
            - \c prep_date: date of preparation (YYMMDD or CCYYMMDD)
            - \c prep_time: time of preparation (HHMM)
            - \c control_reference: interchange control reference
        */
        *hash<auto> getInterchange() {
            return m_interchange;
        }

        #! Returns the current message header information (from UNH segment)
        /** @return a hash with message header data, or NOTHING if not yet parsed

            Keys include:
            - \c message_reference: message reference number
            - \c message_type: message type (e.g., "ORDERS", "INVOIC")
            - \c message_version: message version (e.g., "D")
            - \c message_release: message release (e.g., "96A", "01B")
            - \c controlling_agency: controlling agency (e.g., "UN")
        */
        *hash<auto> getMessage() {
            return m_message;
        }

        #! Returns the delimiter configuration
        EdifactDelimiters getDelimiters() {
            return delimiters;
        }

        #! Returns the total number of segments
        int getSegmentCount() {
            return m_segments.size();
        }

        #! Resets the iterator to the beginning
        reset() {
            m_index = -1;
            m_current = NOTHING;
            m_interchange = NOTHING;
            m_message = NOTHING;
            m_segment_count = 0;
            m_message_count = 0;
        }

        #! Returns all segments as a list of hashes
        /** @return a list of all parsed segments

            @note This method iterates through all segments, resetting the iterator afterward
        */
        list<hash<EdifactSegment>> getAllSegments() {
            list<hash<EdifactSegment>> result = ();
            reset();
            while (next()) {
                push result, m_current;
            }
            reset();
            return result;
        }
    }
}
