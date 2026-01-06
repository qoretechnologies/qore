# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCycleDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear cycle (sprint) record type
public class LinearCycleDataType inherits HashDataType {
    public {
        #! Field definitions for Linear cycles
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the cycle",
                "desc": "The unique identifier of the cycle",
                "type": StringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The cycle name",
                "desc": "The name of the cycle",
                "type": StringOrNothingType,
            },
            "number": {
                "display_name": "Number",
                "short_desc": "The cycle number",
                "desc": "The number of the cycle within the team",
                "type": IntOrNothingType,
            },
            "startsAt": {
                "display_name": "Starts At",
                "short_desc": "When the cycle starts",
                "desc": "The start date of the cycle",
                "type": DateOrNothingType,
            },
            "endsAt": {
                "display_name": "Ends At",
                "short_desc": "When the cycle ends",
                "desc": "The end date of the cycle",
                "type": DateOrNothingType,
            },
            "team": {
                "display_name": "Team",
                "short_desc": "The team",
                "desc": "The team the cycle belongs to",
                "type": AutoHashOrNothingType,
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "When the cycle was created",
                "desc": "The date and time when the cycle was created",
                "type": DateOrNothingType,
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "When the cycle was last updated",
                "desc": "The date and time when the cycle was last updated",
                "type": DateOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

} # namespace LinearDataProvider
