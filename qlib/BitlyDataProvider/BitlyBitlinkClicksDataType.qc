# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyBitlinkClicksDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly click summary response type
public class BitlyBitlinkClicksDataType inherits HashDataType {
    public {
        #! Field definitions for click summary
        const Fields = {
            "total_clicks": {
                "type": SoftIntType,
                "display_name": "Total Clicks",
                "short_desc": "Total number of clicks",
                "desc": "The aggregate number of clicks for the specified time period",
            },
            "units": {
                "type": SoftIntType,
                "display_name": "Units",
                "short_desc": "Number of time units queried",
                "desc": "The number of time units that were queried",
            },
            "unit": {
                "type": StringType,
                "display_name": "Unit",
                "short_desc": "The time unit",
                "desc": "The time unit used for the query",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "minute", "display_name": "Minute"},
                    <AllowedValueInfo>{"value": "hour", "display_name": "Hour"},
                    <AllowedValueInfo>{"value": "day", "display_name": "Day"},
                    <AllowedValueInfo>{"value": "week", "display_name": "Week"},
                    <AllowedValueInfo>{"value": "month", "display_name": "Month"},
                ),
            },
            "unit_reference": {
                "type": StringOrNothingType,
                "display_name": "Unit Reference",
                "short_desc": "The reference timestamp",
                "desc": "The ISO 8601 timestamp used as the reference point for the query",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
