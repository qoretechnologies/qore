# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapResponseParser.qc IMAP response parser

/*  ImapResponseParser.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ImapClient {
#! Parses raw IMAP responses into typed %Qore values
/** IMAP responses are S-expressions of atoms, numbers, \c NIL, quoted strings, literals, and
    parenthesized lists. Literal payloads may contain arbitrary octets — including CRLF — so the raw
    response is first split into protocol text and an ordered list of literal payloads
    (@ref segment()); the text is then tokenized, and every literal introducer consumes the next payload
    in order.

    Literal payloads are never decoded: they stay binary all the way to the caller, so message and
    attachment content survives byte-exact.

    @since %ImapClient 1.0
*/
public class ImapResponseParser {
    public {
        #! Months used to parse IMAP date formats
        const Months = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
    }

    #! @cond nodoc
    private {
        #! the protocol text being parsed
        string text;

        #! the current parse position in @ref text
        int pos = 0;

        #! the length of @ref text
        int len;

        #! literal payloads in wire order
        list<binary> literals;

        #! the index of the next unconsumed literal
        int lit_idx = 0;
    }
    #! @endcond

    #! Creates the parser for a segmented response
    /** @param raw the segmented raw response from @ref segment()
    */
    constructor(hash<ImapRawResponse> raw) {
        text = raw.text;
        len = text.size();
        literals = raw.literals;
    }

    #! Splits a raw IMAP response into protocol text and literal payloads
    /** Literal introducers (\c "{N}", \c "{N+}", \c "{N-}") are left in the text so the tokenizer knows
        where each payload belongs; the payload octets themselves are removed and returned separately.

        @param raw the raw response as received from the server

        @return the response split into text and literal payloads

        @throw IMAP-PARSE-ERROR a literal introducer announced more octets than the response contains
    */
    static hash<ImapRawResponse> segment(binary raw) {
        string txt = "";
        list<binary> lits = ();
        int pos = 0;
        int size = raw.size();

        while (pos < size) {
            int eol = raw.find(<0d0a>, pos);
            if (eol < 0) {
                # a trailing fragment without CRLF cannot introduce a literal
                txt += raw.substr(pos).toString(ProtocolEncoding);
                break;
            }
            string line = raw.substr(pos, eol - pos + 2).toString(ProtocolEncoding);
            txt += line;
            pos = eol + 2;

            *int count = ImapResponseParser::getLiteralCount(line);
            if (!exists count) {
                continue;
            }
            if (pos + count > size) {
                throw "IMAP-PARSE-ERROR", sprintf("literal announced %d bytes but only %d bytes remain in "
                    "the response", count, size - pos);
            }
            lits += count ? raw.substr(pos, count) : binary();
            pos += count;
        }

        return <ImapRawResponse>{"text": txt, "literals": lits};
    }

    #! Returns the octet count of a literal introduced at the end of a line, if any
    /** @param line the line including its trailing CRLF

        @return the literal octet count, or @ref nothing if the line does not end with a literal
        introducer
    */
    static *int getLiteralCount(string line) {
        *string count = (line =~ x/\{([0-9]+)[+-]?\}\r?\n?$/)[0];
        return count.val() ? count.toInt() : NOTHING;
    }

    #! Parses a complete raw IMAP response
    /** @param raw the raw response as received from the server
        @param tag the command tag used for the request

        @return the parsed response

        @throw IMAP-PARSE-ERROR the response could not be parsed
    */
    static hash<ImapResponse> parse(binary raw, string tag) {
        ImapResponseParser parser(ImapResponseParser::segment(raw));
        return parser.parseResponse(tag);
    }

    #! Parses the response held by this object
    /** @param tag the command tag used for the request

        @return the parsed response

        @throw IMAP-PARSE-ERROR the response could not be parsed
    */
    hash<ImapResponse> parseResponse(string tag) {
        hash<ImapResponse> rv();

        while (pos < len) {
            # a tagged completion line or a continuation request ends the response; both are plain text
            # lines, so they are matched before tokenizing
            *string line = peekLine();
            if (!exists line) {
                break;
            }
            if (line == "" ) {
                skipLine();
                continue;
            }
            if (line[0] == "+") {
                rv.continuation = True;
                string ctext = line.size() > 1 ? line.substr(1) : "";
                trim ctext;
                rv.text = ctext;
                skipLine();
                break;
            }
            if (line.size() > tag.size() && line.substr(0, tag.size()).lwr() == tag.lwr()
                && line[tag.size()] == " ") {
                *list<*string> m = (line =~ x/^\S+\s+(OK|NO|BAD)(?:\s+\[([^\]]*)\])?\s*(.*)$/i);
                if (m) {
                    rv.status = m[0].upr();
                    if (m[1].val()) {
                        rv.code = ImapResponseParser::parseTokens(m[1]);
                    }
                    rv.text = m[2] ?? "";
                } else {
                    rv.status = "OK";
                    rv.text = line;
                }
                skipLine();
                break;
            }

            list<auto> values = parseLine();
            if (values) {
                # push, not +=: "+=" would concatenate the line's values into the outer list
                push rv.untagged, values;
            }
        }

        return rv;
    }

    #! Tokenizes a standalone string into a list of IMAP values
    /** Used for response codes and other fragments that carry no literals.

        @param str the string to tokenize

        @return the parsed values
    */
    static list<auto> parseTokens(string str) {
        ImapResponseParser parser(<ImapRawResponse>{"text": str, "literals": ()});
        return parser.parseLine();
    }

    #! @cond nodoc
    # Returns the current physical line without its CRLF, or NOTHING at end of input
    private *string peekLine() {
        if (pos >= len) {
            return;
        }
        int eol = text.find("\r\n", pos);
        if (eol < 0) {
            return text.substr(pos);
        }
        return text.substr(pos, eol - pos);
    }

    # Advances past the current physical line
    private skipLine() {
        int eol = text.find("\r\n", pos);
        pos = eol < 0 ? len : eol + 2;
    }

    # Returns True if the cursor is at a CRLF
    private bool atCrLf() {
        return pos < len && text[pos] == "\r";
    }

    # Skips spaces and tabs, but not line breaks
    private skipSpaces() {
        while (pos < len && (text[pos] == " " || text[pos] == "\t")) {
            ++pos;
        }
    }

    # Skips spaces, tabs, and line breaks; used inside parenthesized lists, which may span lines when a
    # literal payload has been spliced out of the middle of one
    private skipWhitespace() {
        while (pos < len && (text[pos] == " " || text[pos] == "\t" || text[pos] == "\r"
            || text[pos] == "\n")) {
            ++pos;
        }
    }

    # Parses one logical response line into a list of values
    private list<auto> parseLine() {
        list<auto> values = ();
        while (pos < len) {
            skipSpaces();
            if (pos >= len) {
                break;
            }
            if (atCrLf()) {
                pos += 2;
                break;
            }
            if (text[pos] == "\n") {
                ++pos;
                break;
            }
            # push, not +=: a value that is itself a list would otherwise be flattened
            push values, parseValue();
        }
        return values;
    }

    # Parses a single IMAP value
    private auto parseValue() {
        string c = text[pos];
        if (c == "(") {
            return parseList(")");
        }
        if (c == "[") {
            return parseList("]");
        }
        if (c == "\"") {
            return parseQuoted();
        }
        if (c == "{") {
            return parseLiteral();
        }
        return parseAtom();
    }

    # Parses a parenthesized or bracketed list
    private list<auto> parseList(string close) {
        ++pos;      # consume the opening delimiter
        list<auto> values = ();
        while (pos < len) {
            skipWhitespace();
            if (pos >= len) {
                break;
            }
            if (text[pos] == close) {
                ++pos;
                break;
            }
            # push, not +=: a nested list would otherwise be flattened into this one
            push values, parseValue();
        }
        return values;
    }

    # Parses a double-quoted string, resolving backslash escapes
    private string parseQuoted() {
        ++pos;      # consume the opening quote
        string rv = "";
        while (pos < len) {
            string c = text[pos];
            if (c == "\\" && (pos + 1) < len) {
                rv += text[pos + 1];
                pos += 2;
                continue;
            }
            ++pos;
            if (c == "\"") {
                break;
            }
            rv += c;
        }
        return rv;
    }

    # Consumes a literal introducer and returns the corresponding payload
    private binary parseLiteral() {
        int close = text.find("}", pos);
        pos = close < 0 ? len : close + 1;
        # the introducer is always followed by a CRLF that belonged to the wire format
        if (atCrLf()) {
            pos += 2;
        } else if (pos < len && text[pos] == "\n") {
            ++pos;
        }
        if (lit_idx >= literals.size()) {
            throw "IMAP-PARSE-ERROR", sprintf("response references literal %d but only %d were received",
                lit_idx + 1, literals.size());
        }
        return literals[lit_idx++];
    }

    # Parses an atom, a number, or NIL
    /** Square brackets are part of the atom when they follow at least one character, so section
        specifiers such as "BODY[HEADER.FIELDS (DATE FROM)]" — which contain spaces — are read as a
        single atom.
    */
    private auto parseAtom() {
        int start = pos;
        int depth = 0;
        while (pos < len) {
            string c = text[pos];
            if (c == "[") {
                ++depth;
            } else if (c == "]") {
                if (!depth) {
                    break;
                }
                --depth;
            } else if (!depth && (c == " " || c == "\t" || c == "\r" || c == "\n" || c == "("
                || c == ")")) {
                break;
            }
            ++pos;
        }
        string atom = text.substr(start, pos - start);
        if (atom.upr() == "NIL") {
            return NOTHING;
        }
        if (atom =~ /^[0-9]+$/) {
            return atom.toInt();
        }
        return atom;
    }
    #! @endcond

    #! Parses an IMAP \c INTERNALDATE value
    /** @param str the date string (ex: \c "17-Jul-1996 02:44:25 -0700")

        @return the parsed date, or @ref nothing if the value could not be parsed
    */
    static *date parseInternalDate(*string str) {
        if (!str.val()) {
            return;
        }
        # the day is space-padded when single-digit
        string norm = str;
        norm =~ s/^\s+//;
        *list<*string> m = (norm =~ x/^([0-9]{1,2})-(\w{3})-([0-9]{4})\s+([0-9]{2}):([0-9]{2}):([0-9]{2})\s*([+-][0-9]{4})?$/);
        if (!m) {
            return;
        }
        return ImapResponseParser::makeDate(m[2], m[1], m[0], m[3], m[4], m[5], m[6]);
    }

    #! Parses an RFC 2822 date as returned in an IMAP \c ENVELOPE
    /** @param str the date string (ex: \c "Wed, 17 Jul 1996 02:23:25 -0700")

        @return the parsed date, or @ref nothing if the value could not be parsed
    */
    static *date parseEnvelopeDate(*string str) {
        if (!str.val()) {
            return;
        }
        string norm = str;
        # strip an optional leading day-of-week and any trailing timezone comment
        norm =~ s/^\s*\w{3},\s*//;
        norm =~ s/\s*\(.*\)\s*$//;
        norm =~ s/^\s+//;
        norm =~ s/\s+$//;
        *list<*string> m = (norm =~ x/^([0-9]{1,2})\s+(\w{3})\s+([0-9]{2,4})\s+([0-9]{2}):([0-9]{2})(?::([0-9]{2}))?\s*([+-][0-9]{4})?/);
        if (!m) {
            return;
        }
        return ImapResponseParser::makeDate(m[2], m[1], m[0], m[3], m[4], m[5] ?? "00", m[6]);
    }

    #! @cond nodoc
    # Builds a date from already-validated components and a numeric UTC offset
    private static *date makeDate(string year, string month, string day, string hour, string minute,
            string second, *string offset) {
        int mon = ImapResponseParser::getMonth(month);
        if (!mon) {
            return;
        }
        int yr = year.toInt();
        if (year.size() == 2) {
            # RFC 2822 §4.3 obsolete two-digit years
            yr += yr < 50 ? 2000 : 1900;
        }
        int off = 0;
        if (offset.val()) {
            int hours = offset.substr(1, 2).toInt();
            int mins = offset.substr(3, 2).toInt();
            off = (hours * 3600) + (mins * 60);
            if (offset[0] == "-") {
                off = -off;
            }
        }
        TimeZone tz(off);
        return tz.date(sprintf("%04d%02d%02d%02d%02d%02d", yr, mon, day.toInt(), hour.toInt(),
            minute.toInt(), second.toInt()), "YYYYMMDDHHmmSS");
    }

    # Returns the 1-based month number for a 3-letter English abbreviation, or 0 if unknown
    private static int getMonth(string abbr) {
        string want = abbr.lwr();
        foreach string m in (Months) {
            if (m.lwr() == want) {
                return $# + 1;
            }
        }
        return 0;
    }
    #! @endcond
}
}
