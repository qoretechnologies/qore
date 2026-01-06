# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearTeamDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear team record type
public class LinearTeamDataType inherits HashDataType {
    public {
        #! Field definitions for Linear teams
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the team",
                "desc": "The unique identifier of the team",
                "type": StringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The team name",
                "desc": "The name of the team",
                "type": StringType,
            },
            "key": {
                "display_name": "Key",
                "short_desc": "The team key",
                "desc": "The unique key of the team used in issue identifiers",
                "type": StringType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The team description",
                "desc": "The description of the team",
                "type": StringOrNothingType,
            },
            "private": {
                "display_name": "Private",
                "short_desc": "Whether the team is private",
                "desc": "Whether the team is private (only visible to members)",
                "type": BoolOrNothingType,
            },
            "timezone": {
                "display_name": "Timezone",
                "short_desc": "The team timezone",
                "desc": "The timezone of the team",
                "type": StringOrNothingType,
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "When the team was created",
                "desc": "The date and time when the team was created",
                "type": DateOrNothingType,
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "When the team was last updated",
                "desc": "The date and time when the team was last updated",
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
