# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file MllpFraming.qc MLLP framing and HL7 ACK helpers

/*  MllpFraming.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MllpUtil {
    #! Returns \a message with MLLP frame delimiters
    /** @param message the HL7 v2 message text
        @param encoding optional target encoding before byte conversion

        @return an MLLP-framed binary value
    */
    public binary sub mllp_frame(string message, *string encoding = NOTHING) {
        string data = encoding ? convert_encoding(message, encoding) : message;
        return chr(MllpStartBlock).toBinary() + data.toBinary() + chr(MllpEndBlock).toBinary()
            + chr(MllpCarriageReturn).toBinary();
    }

    #! Extracts the payload bytes from a complete MLLP frame
    /** @param frame a complete MLLP frame

        @throw MLLP-FRAME-ERROR the frame is missing required delimiters
    */
    public binary sub mllp_extract_payload(binary frame) {
        if (frame.size() < 3) {
            throw "MLLP-FRAME-ERROR", "MLLP frame is shorter than the required delimiters";
        }
        if (frame[0] != MllpStartBlock) {
            throw "MLLP-FRAME-ERROR", "MLLP frame does not start with the start block byte";
        }
        if (frame[frame.size() - 2] != MllpEndBlock || frame[frame.size() - 1] != MllpCarriageReturn) {
            throw "MLLP-FRAME-ERROR", "MLLP frame does not end with the end block and carriage return bytes";
        }
        return frame.substr(1, frame.size() - 3);
    }

    #! Extracts and decodes the payload from a complete MLLP frame
    /** @param frame a complete MLLP frame
        @param encoding source payload encoding

        @throw MLLP-FRAME-ERROR the frame is missing required delimiters
    */
    public string sub mllp_unframe(binary frame, string encoding = "UTF-8") {
        return mllp_extract_payload(frame).toString(encoding);
    }

    #! Reads one MLLP payload from a socket
    /** @param sock connected socket
        @param to socket read timeout
        @param max_frame_size maximum payload size in bytes

        @return payload bytes without MLLP delimiters

        @throw MLLP-FRAME-ERROR invalid frame delimiters were received
        @throw MLLP-FRAME-TOO-LARGE the payload exceeds \a max_frame_size
    */
    public binary sub mllp_read_payload(Socket sock, timeout to = DefaultTimeout,
            int max_frame_size = DefaultMaxFrameSize) {
        validateMaxFrameSize(max_frame_size);

        int c = sock.recvu1(to);
        if (c != MllpStartBlock) {
            throw "MLLP-FRAME-ERROR", "MLLP frame does not start with the start block byte";
        }

        BinaryOutputStream payload();
        int payload_size = 0;
        while (True) {
            c = sock.recvu1(to);
            if (c == MllpEndBlock) {
                int trailer = sock.recvu1(to);
                if (trailer != MllpCarriageReturn) {
                    throw "MLLP-FRAME-ERROR", "MLLP end block is not followed by carriage return";
                }
                return payload.getData();
            }
            payload.write(chr(c).toBinary());
            ++payload_size;
            if (payload_size > max_frame_size) {
                throw "MLLP-FRAME-TOO-LARGE", sprintf("MLLP frame exceeds the configured maximum size of %d bytes",
                    max_frame_size);
            }
        }
    }

    #! Reads and decodes one MLLP-framed HL7 v2 message from a socket
    /** @param sock connected socket
        @param to socket read timeout
        @param max_frame_size maximum payload size in bytes
        @param encoding source payload encoding
    */
    public string sub mllp_read_message(Socket sock, timeout to = DefaultTimeout,
            int max_frame_size = DefaultMaxFrameSize, *string encoding = NOTHING) {
        return mllp_read_payload(sock, to, max_frame_size).toString(encoding ?? sock.getEncoding() ?? "UTF-8");
    }

    #! Sends one HL7 v2 message as an MLLP frame
    /** @param sock connected socket
        @param message HL7 v2 message text
        @param to socket send timeout
        @param encoding optional target encoding before byte conversion
    */
    public sub mllp_send_message(Socket sock, string message, timeout to = DefaultTimeout,
            *string encoding = NOTHING) {
        sock.send(mllp_frame(message, encoding), to);
    }

    #! Parses a standard HL7 v2 ACK message
    /** @param ack HL7 v2 ACK message text

        @return parsed ACK metadata

        @throw MLLP-ACK-ERROR the message does not contain an MSA acknowledgment code
    */
    public hash<MllpAckInfo> sub mllp_parse_ack(string ack) {
        Hl7v2Util::Hl7v2Message msg(ack);
        *string ack_code = msg.getAddress("MSA-1");
        if (!ack_code) {
            throw "MLLP-ACK-ERROR", "HL7 ACK message does not contain MSA-1";
        }
        return <MllpAckInfo>{
            "acknowledgment_code": ack_code,
            "message_control_id": msg.getAddress("MSA-2"),
            "text_message": msg.getAddress("MSA-3"),
            "accepted": mllpIsAcceptedAckCode(ack_code),
            "errors": mllpParseErrSegments(msg),
            "raw_message": ack,
        };
    }

    #! Builds a standard HL7 v2 ACK message for a request
    /** @param request original HL7 v2 request message
        @param code ACK code to place in MSA-1
        @param text optional text message for MSA-3
        @param opts optional ACK options:
        - \c ack_control_id: override generated \c MSH-10
        - \c timestamp: override generated \c MSH-7 as \c YYYYMMDDHHMMSS
        - \c error: a single ERR descriptor hash
        - \c errors: a list of ERR descriptor hashes

        ERR descriptor hashes support \c location, \c error_code, \c error_text, \c severity,
        and \c user_message. \c error_code can be a string, a component list, or a hash with
        \c code, \c text, and \c coding_system.

        @return serialized ACK message
    */
    public string sub mllp_build_ack(string request, MllpAckCode code = MllpAckCode::ApplicationAccept,
            *string text = NOTHING, *hash<auto> opts = NOTHING) {
        Hl7v2Util::Hl7v2Message msg(request);
        Hl7v2Util::Hl7v2Base base();
        base.setDelimiters(msg.getMessage().delimiters);
        hash<auto> ack_opts = opts ?? {};

        *string request_control_id = msg.getAddress("MSH-10");
        string ack_control_id = mllpOptionalString(ack_opts, "ack_control_id",
            (request_control_id ?? "MSG") + "-ACK");
        string timestamp = mllpOptionalString(ack_opts, "timestamp", format_date("YYYYMMDDHHmmSS", now()));

        list<auto> msh_fields = (
            base.getDelimiters().getEncodingCharacters(),
            msg.getAddress("MSH-5") ?? "",
            msg.getAddress("MSH-6") ?? "",
            msg.getAddress("MSH-3") ?? "",
            msg.getAddress("MSH-4") ?? "",
            timestamp,
            "",
            ("ACK", msg.getAddress("MSH-9.2") ?? "", "ACK"),
            ack_control_id,
            msg.getAddress("MSH-11") ?? "P",
            msg.getAddress("MSH-12") ?? "2.5.1"
        );

        list<auto> msa_fields = (
            string(code),
            request_control_id ?? ""
        );
        if (text) {
            push msa_fields, text;
        }

        list<string> segments = (
            base.formatSegment({"tag": "MSH", "fields": msh_fields}),
            base.formatSegment({"tag": "MSA", "fields": msa_fields})
        );
        foreach hash<auto> err in (mllpGetAckErrors(code, text, ack_opts)) {
            push segments, base.formatSegment({"tag": "ERR", "fields": mllpErrFields(err, text)});
        }

        return segments.join("\r") + "\r";
    }

    sub validateMaxFrameSize(int max_frame_size) {
        if (max_frame_size <= 0) {
            throw "MLLP-OPTION-ERROR", sprintf("maximum frame size must be positive; got %d", max_frame_size);
        }
    }

    bool sub mllpIsAcceptedAckCode(string ack_code) {
        return ack_code == string(MllpAckCode::ApplicationAccept)
            || ack_code == string(MllpAckCode::CommitAccept);
    }

    list<hash<MllpErrInfo>> sub mllpParseErrSegments(Hl7v2Util::Hl7v2Message msg) {
        Hl7v2Util::Hl7v2Base base();
        base.setDelimiters(msg.getMessage().delimiters);
        list<hash<MllpErrInfo>> errors = ();
        foreach hash<Hl7v2Util::Hl7v2Segment> segment in (msg.getSegments()) {
            if (segment.tag != "ERR") {
                continue;
            }
            push errors, <MllpErrInfo>{
                "location": base.getFieldValue(segment, 2),
                "error_code": base.getFieldValue(segment, 3, NOTHING, 1),
                "error_text": base.getFieldValue(segment, 3, NOTHING, 2),
                "severity": base.getFieldValue(segment, 4),
                "user_message": base.getFieldValue(segment, 8),
                "raw_segment": segment.raw,
            };
        }
        return errors;
    }

    list<hash<auto>> sub mllpGetAckErrors(MllpAckCode code, *string text, hash<auto> opts) {
        if (exists opts.errors) {
            if (opts.errors.typeCode() != NT_LIST) {
                throw "MLLP-ACK-ERROR", "ACK option \"errors\" must be a list of hashes";
            }
            list<hash<auto>> errors = ();
            foreach auto err in (opts.errors) {
                if (err.typeCode() != NT_HASH) {
                    throw "MLLP-ACK-ERROR", "ACK option \"errors\" must be a list of hashes";
                }
                push errors, cast<hash<auto>>(err);
            }
            return errors;
        }
        if (exists opts.error) {
            if (opts.error.typeCode() != NT_HASH) {
                throw "MLLP-ACK-ERROR", "ACK option \"error\" must be a hash";
            }
            return (cast<hash<auto>>(opts.error),);
        }
        if (!mllpIsAcceptedAckCode(string(code)) && text) {
            return ({
                "user_message": text,
            },);
        }
        return ();
    }

    list<auto> sub mllpErrFields(hash<auto> err, *string text) {
        return (
            "",
            mllpOptionalComposite(err, "location"),
            exists err.error_code
                ? mllpCodedElement(err.error_code, "ACK error_code")
                : ("207", "Application internal error", "HL70357"),
            mllpOptionalString(err, "severity", "E"),
            "",
            "",
            "",
            mllpOptionalString(err, "user_message", text ?? "")
        );
    }

    list<auto> sub mllpCodedElement(auto value, string ctx) {
        if (value.typeCode() == NT_HASH) {
            hash<auto> h = cast<hash<auto>>(value);
            return (
                mllpRequiredString(h, "code", ctx),
                mllpOptionalString(h, "text", mllpOptionalString(h, "display")),
                mllpOptionalString(h, "coding_system", mllpOptionalString(h, "system"))
            );
        }
        return mllpComposite(value, ctx);
    }

    list<auto> sub mllpOptionalComposite(hash<auto> opts, string key) {
        return exists opts{key} ? mllpComposite(opts{key}, key) : ();
    }

    list<auto> sub mllpComposite(auto value, string ctx) {
        if (value.typeCode() == NT_LIST) {
            return value;
        }
        if (value.typeCode() == NT_STRING) {
            return (value,);
        }
        throw "MLLP-ACK-ERROR", sprintf("%s must be a string, list, or coded hash", ctx);
    }

    string sub mllpRequiredString(hash<auto> opts, string key, string ctx) {
        if (opts{key}.typeCode() != NT_STRING || !opts{key}) {
            throw "MLLP-ACK-ERROR", sprintf("%s %y must be a non-empty string", ctx, key);
        }
        return opts{key};
    }

    string sub mllpOptionalString(hash<auto> opts, string key, string default_value = "") {
        if (!exists opts{key}) {
            return default_value;
        }
        if (opts{key}.typeCode() != NT_STRING) {
            throw "MLLP-ACK-ERROR", sprintf("ACK option %y must be a string", key);
        }
        return opts{key} ? opts{key} : default_value;
    }
}
