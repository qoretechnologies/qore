# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SseEventFormat.qc Server-sent event field formatting

/** SseEventFormat.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpServerAsyncIo {
#! Formats the fields of server-sent events in the event stream format
/** In an event stream, each field of an event is one line, and a line ends with a CR, an LF or a CRLF pair; a blank
    line dispatches the event.  So a field value can never contain a line break: a line break in an event type or an
    ID would end the field early and start another field, or another event, chosen by whoever supplied the value.

    - an event type or an ID with a CR or LF character cannot be written, and neither can one with a NUL character: a
      client ignores an ID with a NUL character, so the ID would be lost, and an event type with one names no event
      that a client listens for; these raise \c SSE-EVENT-ERROR
    - a retry value must be a non-negative integer; anything else raises \c SSE-EVENT-ERROR
    - data and comments are written as one line for each of their lines, split at CRLF, CR and LF, so a client reads
      the data back with its lines joined by LF; a NUL character in data or a comment is kept

    @par Example:
    @code{.py}
# "event: price\ndata: 1.25\ndata: EUR\n\n"
string frame = SseEventFormat::formatEvent({"type": "price", "data": "1.25\rEUR"});
    @endcode

    @since %HttpServerAsyncIo 1.0
*/
public class SseEventFormat {
    public {
        #! The error raised for an event field value that an event stream cannot carry
        const SseEventError = "SSE-EVENT-ERROR";
    }

    #! Returns an SSE event frame for an event hash, terminated by the blank line that dispatches it
    /** @param event a hash with the following optional keys:
        - \c type: the event type
        - \c data: the event data; split into one \c data line for each line
        - \c id: the event ID
        - \c retry: the reconnection time in milliseconds

        @return the event frame

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer
    */
    static string formatEvent(hash<auto> event) {
        string msg = "";
        if (event.id) {
            msg += SseEventFormat::formatLineField("id", event.id);
        }
        if (event.type) {
            msg += SseEventFormat::formatLineField("event", event.type);
        }
        if (event.retry) {
            msg += SseEventFormat::formatRetry(event.retry);
        }
        if (event.data) {
            msg += SseEventFormat::formatMultiLineField("data", event.data);
        }
        # an empty line terminates the event
        return msg + "\n";
    }

    #! Returns one field line for a value that cannot contain a line break, such as an event type or an ID
    /** @param name the field name: \c "event" or \c "id"
        @param value the field value; converted to a string

        @return the field line, ex: \c "id: 42\n"

        @throw SSE-EVENT-ERROR the value contains a NUL, CR or LF character
    */
    static string formatLineField(string name, softstring value) {
        string what = name == "event" ? "type" : (name == "id" ? "ID" : name);
        check_line_text(value, "server-sent event " + what, SseEventError);
        return name + ": " + value + "\n";
    }

    #! Returns the lines of a field whose value can have several lines, such as data or a comment
    /** @param name the field name, ex: \c "data"; an empty name gives comment lines, which start with a colon
        @param value the field value; split at CRLF, CR and LF, as a client splits the stream into lines

        @return one field line for each line of the value, so that a client joins them back into the value with LF
        between the lines; an empty value is one empty field line, and a value that ends with a line break ends
        with an empty field line
    */
    static string formatMultiLineField(string name, softstring value) {
        string str = Qore::replace(Qore::replace(value, "\r\n", "\n"), "\r", "\n");
        list<string> lines = str.split("\n");
        # split() returns no element after a final line break, and none for an empty string
        if (str.empty() || str[str.size() - 1] == "\n") {
            push lines, "";
        }
        string msg = "";
        foreach string line in (lines) {
            msg += name + ": " + line + "\n";
        }
        return msg;
    }

    #! Returns the retry field line
    /** @param retry the reconnection time in milliseconds, as an integer or a string of decimal digits

        @return the field line, ex: \c "retry: 5000\n"

        @throw SSE-EVENT-ERROR the value is not a non-negative integer
    */
    static string formatRetry(auto retry) {
        switch (retry.typeCode()) {
            case NT_INT:
                if (retry >= 0) {
                    return sprintf("retry: %d\n", retry);
                }
                break;
            case NT_STRING:
                if (retry =~ /^[0-9]+$/ && !retry.contains("\n")) {
                    return "retry: " + retry + "\n";
                }
                break;
        }
        throw SseEventError, sprintf("server-sent event retry value %y is not a non-negative integer number of "
            "milliseconds", retry);
    }
}
}
