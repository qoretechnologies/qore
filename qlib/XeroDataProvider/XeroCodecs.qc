# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroCodecs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the XeroDataProvider module
public namespace XeroDataProvider {
#! The wire codecs the Xero application registers
/** Xero declares its date-valued query arguments as plain strings — \c DateFrom and \c DateTo on the
    purchase order listing are \c {"type": "string"} with nothing to say they are dates — and expects them
    written as \c YYYY-MM-DD.  Anything else is either rejected or, worse, parsed as a different day.

    That is exactly the case a codec is for: the schema is not lying about the *shape* of the value, it is
    silent about its *meaning*, so the action presents a %Qore @ref date "date" and the codec writes the
    format Xero reads.  The application this module replaces carried the same conversion as a client-wide
    \c query_date_format option, which applied it to every query argument of every call rather than to the
    ones that are dates.

    @note The date is formatted in the local zone rather than converted to UTC.  Xero's date filters are
    calendar dates in the organisation's own terms — "invoices from the first of the month" — and
    converting to UTC first would move the boundary by a day for anyone east or west of Greenwich.
*/
public class XeroCodecs {
    public {
        #! Codec name: a calendar date written as \c YYYY-MM-DD
        const QueryDate = "xero-query-date";

        #! The codecs this application registers
        const Codecs = {
            QueryDate: <RestSchemaWireCodecInfo>{
                "name": QueryDate,
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "sent as a calendar date written \"YYYY-MM-DD\"",
                "to_wire": auto sub (auto v) {
                    return exists v ? date(v).format("YYYY-MM-DD") : v;
                },
                "from_wire": auto sub (auto v) {
                    return exists v ? date(v) : v;
                },
            },
        };
    }

    #! Registers this application's codecs
    /** Called before the action sets are built, because a codec named by an overlay is resolved from a
        process-wide registry at that point.
    */
    static register() {
        map RestSchemaActionCodecs::registerCodec($1), Codecs.iterator();
    }
}
}
