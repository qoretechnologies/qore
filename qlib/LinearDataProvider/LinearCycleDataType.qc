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
    private {
        #! Field definitions for Linear cycles
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the cycle",
                "desc": "The unique identifier of the cycle",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The cycle name",
                "desc": "The name of the cycle",
                "example_value": "Sprint 23",
            },
            "number": {
                "type": IntOrNothingType,
                "display_name": "Number",
                "short_desc": "The cycle number",
                "desc": "The number of the cycle within the team",
                "example_value": 23,
            },
            "startsAt": {
                "type": DateOrNothingType,
                "display_name": "Starts At",
                "short_desc": "When the cycle starts",
                "desc": "The start date of the cycle",
            },
            "endsAt": {
                "type": DateOrNothingType,
                "display_name": "Ends At",
                "short_desc": "When the cycle ends",
                "desc": "The end date of the cycle",
            },
            "team": {
                "type": AutoHashOrNothingType,
                "display_name": "Team",
                "short_desc": "The team",
                "desc": "The team the cycle belongs to",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the cycle was created",
                "desc": "The date and time when the cycle was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the cycle was last updated",
                "desc": "The date and time when the cycle was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
