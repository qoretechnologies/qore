# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapGoalDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap campaign goal data type
public class KeapGoalDataType inherits HashDataType {
    public {
        #! Field definitions for campaign goals
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Goal ID",
                "short_desc": "The goal ID",
                "desc": "The unique identifier of the campaign goal",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Goal Name",
                "short_desc": "The goal name",
                "desc": "The name of the campaign goal",
                "example_value": "Newsletter Signup",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Goal Type",
                "short_desc": "The type of goal",
                "desc": "The type of the campaign goal",
            },
            "historical_contact_counts": {
                "type": AutoHashOrNothingType,
                "display_name": "Historical Counts",
                "short_desc": "Historical contact counts",
                "desc": "Historical contact count data for this goal",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapGoalDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapGoalDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
