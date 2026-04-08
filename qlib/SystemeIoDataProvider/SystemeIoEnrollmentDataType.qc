# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoEnrollmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io enrollment record type
public class SystemeIoEnrollmentDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The enrollment ID",
                "desc": "The unique identifier of the enrollment",
            },
            "courseId": {
                "type": IntOrNothingType,
                "display_name": "Course ID",
                "short_desc": "The course ID",
                "desc": "The unique identifier of the course",
            },
            "contactId": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the enrolled contact",
            },
            "accessType": {
                "type": StringOrNothingType,
                "display_name": "Access Type",
                "short_desc": "Type of course access",
                "desc": "The type of access granted to the contact",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the enrollment was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io create enrollment request data type
public class SystemeIoCreateEnrollmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "courseId": {
                "type": IntType,
                "display_name": "Course ID",
                "short_desc": "The course to enroll in",
                "desc": "The unique identifier of the course to enroll the contact in",
                "required": True,
                "ref_data": "courses",
                "example_value": 100,
            },
            "contactId": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact to enroll",
                "desc": "The unique identifier of the contact to enroll",
                "required": True,
                "ref_data": "contacts",
                "example_value": 12345,
            },
            "accessType": {
                "type": StringType,
                "display_name": "Access Type",
                "short_desc": "Type of course access",
                "desc": "The type of access to grant the contact",
                "required": True,
                "example_value": "full_access",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "full_access", "display_name": "Full Access"},
                    <AllowedValueInfo>{"value": "partial_access",
                        "display_name": "Partial Access"},
                    <AllowedValueInfo>{"value": "dripping_content",
                        "display_name": "Dripping Content"},
                ),
            },
            "modules": {
                "type": new ListDataType("Modules", IntOrNothingType, True),
                "display_name": "Modules",
                "short_desc": "Module IDs for partial access",
                "desc": "List of module IDs to grant access to. Required when `accessType` "
                    "is `partial_access`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io delete enrollment request data type
public class SystemeIoDeleteEnrollmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Enrollment ID",
                "short_desc": "The enrollment ID to delete",
                "desc": "The unique identifier of the enrollment to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
