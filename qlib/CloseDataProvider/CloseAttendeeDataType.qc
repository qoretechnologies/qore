# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseAttendeeDataType.qc Close meeting attendee data type

/*  CloseAttendeeDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close meeting attendee data type
public class CloseAttendeeDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The attendee user ID",
                "desc": "The Close user ID of the attendee (for internal users)",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The attendee name",
                "desc": "The display name of the attendee",
                "example_value": "Jane Smith",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The attendee email",
                "desc": "The email address of the attendee",
                "example_value": "jane@example.com",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The attendee RSVP status",
                "desc": "The RSVP status of the attendee: `noreply`, `yes`, `no`, or `maybe`",
                "example_value": "yes",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
