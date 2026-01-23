# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactAbstractWriter.qc EDIFACT abstract writer base class

/*  EdifactAbstractWriter.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Writer options
    public const EdifactWriterOptions = {
        "encoding": "The character encoding for the output (default: ISO-8859-1)",
        "write_una": "If True, write UNA segment at the start (default: True)",
        "line_break": "If True, add newline after each segment for readability (default: False)",
        "syntax_identifier": "Syntax identifier for UNB (default: 'UNOC')",
        "syntax_version": "Syntax version for UNB (default: '3')",
    };

    #! Hashdecl for interchange header options
    public hashdecl EdifactInterchangeInfo {
        #! Sender identification
        string sender_id;
        #! Sender identification qualifier
        *string sender_qualifier;
        #! Recipient identification
        string recipient_id;
        #! Recipient identification qualifier
        *string recipient_qualifier;
        #! Interchange control reference
        string control_reference;
        #! Application reference
        *string application_reference;
        #! Syntax identifier (default: "UNOC")
        *string syntax_identifier;
        #! Syntax version (default: "3")
        *string syntax_version;
    }

    #! Hashdecl for message header options
    public hashdecl EdifactMessageInfo {
        #! Message reference number
        string message_reference;
        #! Message type (e.g., "ORDERS", "INVOIC")
        string message_type;
        #! Message version (e.g., "D")
        string message_version;
        #! Message release (e.g., "96A", "01B")
        string message_release;
        #! Controlling agency (default: "UN")
        *string controlling_agency;
    }

    #! Abstract base writer class for EDIFACT messages
    /** This class provides the base functionality for writing EDIFACT messages.

        @par Example
        @code{.py}
# Subclasses implement the actual output destination
EdifactFileWriter w("orders.edi");
w.writeInterchangeHeader(<EdifactInterchangeInfo>{
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
});
w.writeMessageHeader(<EdifactMessageInfo>{
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "96A",
});
w.writeSegment("BGM", (("220",), ("PO12345",), ("9",)));
w.writeMessageTrailer();
w.writeInterchangeTrailer();
        @endcode
    */
    public class EdifactAbstractWriter inherits EdifactBase {
        private {
            #! Write UNA segment flag
            bool m_write_una = True;

            #! Add line breaks flag
            bool m_line_break = False;

            #! Encoding for output
            string m_encoding = "ISO-8859-1";

            #! Default syntax identifier
            string m_syntax_identifier = "UNOC";

            #! Default syntax version
            string m_syntax_version = "3";

            #! Segment count for current message
            int m_segment_count = 0;

            #! Message count for current interchange
            int m_message_count = 0;

            #! Current message reference
            *string m_message_reference;

            #! Current interchange control reference
            *string m_control_reference;

            #! Flag indicating if UNA was written
            bool m_una_written = False;

            #! Flag indicating if interchange header was written
            bool m_interchange_started = False;

            #! Flag indicating if message header was written
            bool m_message_started = False;
        }

        #! Creates an EdifactAbstractWriter with default options
        constructor() : EdifactBase() {
        }

        #! Creates an EdifactAbstractWriter with specified options
        /** @param opts options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(*hash<auto> opts) : EdifactBase(opts) {
            processWriterOptions(opts);
        }

        #! Creates an EdifactAbstractWriter with specified delimiters and options
        /** @param delim the delimiter configuration
            @param opts options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(EdifactDelimiters delim, *hash<auto> opts) : EdifactBase(delim) {
            processWriterOptions(opts);
        }

        #! Process writer options
        private processWriterOptions(*hash<auto> opts) {
            if (opts.encoding) {
                m_encoding = opts.encoding;
            }
            if (exists opts.write_una) {
                m_write_una = parse_boolean(opts.write_una);
            }
            if (exists opts.line_break) {
                m_line_break = parse_boolean(opts.line_break);
            }
            if (opts.syntax_identifier) {
                m_syntax_identifier = opts.syntax_identifier;
            }
            if (opts.syntax_version) {
                m_syntax_version = opts.syntax_version;
            }
        }

        #! Abstract method to write data to the output
        /** @param data the data to write

            Subclasses must implement this method to write to their specific output destination.
        */
        abstract private writeOutput(string data);

        #! Writes the UNA segment if configured
        /** This is called automatically by writeInterchangeHeader() if write_una is True.
            Can also be called explicitly.
        */
        writeUna() {
            if (m_una_written) {
                return;
            }
            string una = delimiters.generateUna();
            writeOutput(una);
            if (m_line_break) {
                writeOutput("\n");
            }
            m_una_written = True;
        }

        #! Writes an interchange header (UNB segment)
        /** @param info interchange header information

            @par Example
            @code{.py}
w.writeInterchangeHeader(<EdifactInterchangeInfo>{
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
});
            @endcode
        */
        writeInterchangeHeader(hash<EdifactInterchangeInfo> info) {
            @assert(info.sender_id.val(), "sender_id is required");
            @assert(info.recipient_id.val(), "recipient_id is required");
            @assert(info.control_reference.val(), "control_reference is required");
            @debug(printf("EdifactAbstractWriter::writeInterchangeHeader(): sender=%y recipient=%y ref=%y\n",
                info.sender_id, info.recipient_id, info.control_reference));
            # Write UNA first if configured
            if (m_write_una && !m_una_written) {
                writeUna();
            }

            # Build UNB data elements
            list<auto> elems = ();

            # Syntax identifier (composite)
            push elems, (info.syntax_identifier ?? m_syntax_identifier,
                           info.syntax_version ?? m_syntax_version);

            # Sender (composite)
            if (info.sender_qualifier) {
                push elems, (info.sender_id, info.sender_qualifier);
            } else {
                push elems, (info.sender_id,);
            }

            # Recipient (composite)
            if (info.recipient_qualifier) {
                push elems, (info.recipient_id, info.recipient_qualifier);
            } else {
                push elems, (info.recipient_id,);
            }

            # Date/time of preparation
            date now = now_us();
            string date_str = format_date("YYMMDD", now);
            string time_str = format_date("HHMM", now);
            push elems, (date_str, time_str);

            # Interchange control reference
            push elems, (info.control_reference,);

            # Application reference (optional)
            if (info.application_reference) {
                push elems, (info.application_reference,);
            }

            writeSegment(UnbSegmentTag, elems);
            m_control_reference = info.control_reference;
            m_interchange_started = True;
            m_message_count = 0;
        }

        #! Writes an interchange trailer (UNZ segment)
        writeInterchangeTrailer() {
            if (!m_interchange_started) {
                throw "EDIFACT-WRITE-ERROR", "cannot write interchange trailer without interchange header";
            }

            list<auto> elems = (
                (string(m_message_count),),  # Number of messages
                (m_control_reference,),       # Interchange control reference
            );

            writeSegment(UnzSegmentTag, elems);
            m_interchange_started = False;
        }

        #! Writes a message header (UNH segment)
        /** @param info message header information

            @par Example
            @code{.py}
w.writeMessageHeader(<EdifactMessageInfo>{
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "96A",
});
            @endcode
        */
        writeMessageHeader(hash<EdifactMessageInfo> info) {
            @assert(info.message_reference.val(), "message_reference is required");
            @assert(info.message_type.val(), "message_type is required");
            @assert(info.message_version.val(), "message_version is required");
            @assert(info.message_release.val(), "message_release is required");
            @debug(printf("EdifactAbstractWriter::writeMessageHeader(): ref=%y type=%y version=%y release=%y\n",
                info.message_reference, info.message_type, info.message_version, info.message_release));
            if (!m_interchange_started) {
                throw "EDIFACT-WRITE-ERROR", "cannot write message header without interchange header";
            }

            # Build UNH data elements
            list<auto> elems = ();

            # Message reference number
            push elems, (info.message_reference,);

            # Message identifier (composite)
            push elems, (
                info.message_type,
                info.message_version,
                info.message_release,
                info.controlling_agency ?? "UN",
            );

            writeSegment(UnhSegmentTag, elems);
            m_message_reference = info.message_reference;
            m_message_started = True;
            m_segment_count = 1;  # UNH counts as 1
        }

        #! Writes a message trailer (UNT segment)
        writeMessageTrailer() {
            if (!m_message_started) {
                throw "EDIFACT-WRITE-ERROR", "cannot write message trailer without message header";
            }

            # UNT includes itself in the count
            m_segment_count++;

            list<auto> elems = (
                (string(m_segment_count),),  # Number of segments
                (m_message_reference,),       # Message reference number
            );

            writeSegment(UntSegmentTag, elems);
            m_message_started = False;
            m_message_count++;
        }

        #! Writes a segment
        /** @param tag the segment tag (3 uppercase letters)
            @param elems the elements list (each element is a list of components)

            @par Example
            @code{.py}
# Simple segment
w.writeSegment("BGM", (("220",), ("PO12345",), ("9",)));

# Segment with composite elements
w.writeSegment("DTM", (("137", "20260122", "102"),));
            @endcode
        */
        writeSegment(string tag, list<auto> elems) {
            @assert(tag.val(), "segment tag cannot be empty");
            @assert(tag.regex("^[A-Z]{3}$"), sprintf("segment tag must be 3 uppercase letters: %y", tag));
            @debug(printf("EdifactAbstractWriter::writeSegment(): tag=%y elems=%d\n", tag, elems.size()));
            string segment = generateSegment(tag, elems, True);
            writeOutput(segment);
            if (m_line_break) {
                writeOutput("\n");
            }

            # Count segment if in a message (but not UNH/UNT which are counted separately)
            if (m_message_started && tag != UnhSegmentTag && tag != UntSegmentTag) {
                m_segment_count++;
            }
        }

        #! Writes a raw segment string
        /** @param segment_str the raw segment string (terminator will be added if missing)
        */
        writeRawSegment(string segment_str) {
            # Add terminator if not present
            if (segment_str.size() > 0 && segment_str[segment_str.size() - 1] != delimiters.segment_terminator) {
                segment_str += delimiters.segment_terminator;
            }
            writeOutput(segment_str);
            if (m_line_break) {
                writeOutput("\n");
            }

            if (m_message_started) {
                m_segment_count++;
            }
        }

        #! Writes multiple segments from a list
        /** @param segments a list of segment hashes, each with "tag" and "elements" keys

            @par Example
            @code{.py}
w.write((
    {"tag": "BGM", "elements": (("220",), ("PO12345",), ("9",))},
    {"tag": "DTM", "elements": (("137", "20260122", "102"),)},
));
            @endcode
        */
        write(list<hash<auto>> segments) {
            foreach hash<auto> seg in (segments) {
                writeSegment(seg.tag, seg.elements);
            }
        }

        #! Returns the current segment count for the current message
        int getSegmentCount() {
            return m_segment_count;
        }

        #! Returns the current message count for the current interchange
        int getMessageCount() {
            return m_message_count;
        }
    }
}
