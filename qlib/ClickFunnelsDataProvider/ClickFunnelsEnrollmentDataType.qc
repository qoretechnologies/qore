# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsEnrollmentDataType.qc ClickFunnels enrollment data type

/*  ClickFunnelsEnrollmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! ClickFunnels enrollment record type
public class ClickFunnelsEnrollmentDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The enrollment ID",
                "desc": "The unique identifier of the enrollment",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the enrollment",
            },
            "course_id": {
                "type": IntOrNothingType,
                "display_name": "Course ID",
                "short_desc": "The course ID",
                "desc": "The ID of the course for this enrollment",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The ID of the enrolled contact",
            },
            "suspended": {
                "type": SoftBoolOrNothingType,
                "display_name": "Suspended",
                "short_desc": "Whether the enrollment is suspended",
                "desc": "Whether the enrollment is currently suspended",
            },
            "completed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Completed",
                "short_desc": "Whether the enrollment is completed",
                "desc": "Whether the enrollment has been completed",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the enrollment was created",
                "desc": "The timestamp when the enrollment was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the enrollment was last updated",
                "desc": "The timestamp when the enrollment was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
