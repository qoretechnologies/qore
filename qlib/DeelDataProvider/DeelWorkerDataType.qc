# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWorkerDataType.qc Deel worker/people record data type

/*  DeelWorkerDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel worker (people) record type
public class DeelWorkerDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The worker ID",
                "desc": "The unique identifier of the worker",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The worker's first name",
                "desc": "The worker's given (first) name",
                "example_value": "John",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The worker's last name",
                "desc": "The worker's family (last) name",
                "example_value": "Smith",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The worker's email address",
                "desc": "The email address of the worker",
                "example_value": "john.smith@example.com",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The worker's country",
                "desc": "The ISO 3166-1 alpha-2 country code of the worker's location",
                "example_value": "US",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The worker's employment status",
                "desc": "The current employment status of the worker "
                    "(e.g., `active`, `inactive`, `pending`)",
                "example_value": "active",
            },
            "employment_type": {
                "type": StringOrNothingType,
                "display_name": "Employment Type",
                "short_desc": "The type of employment",
                "desc": "Whether the worker is a contractor or employee "
                    "(e.g., `contractor`, `employee`)",
                "example_value": "contractor",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The worker's job title",
                "desc": "The job title of the worker",
                "example_value": "Software Engineer",
            },
            "seniority": {
                "type": StringOrNothingType,
                "display_name": "Seniority",
                "short_desc": "The worker's seniority level",
                "desc": "The seniority level of the worker",
                "example_value": "senior",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "junior", "desc": "Junior level"},
                    <AllowedValueInfo>{"value": "mid", "desc": "Mid level"},
                    <AllowedValueInfo>{"value": "senior", "desc": "Senior level"},
                    <AllowedValueInfo>{"value": "lead", "desc": "Lead level"},
                ),
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the worker was created",
                "desc": "The timestamp when the worker was added to Deel (ISO 8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the worker was last updated",
                "desc": "The timestamp when the worker was last updated (ISO 8601 format)",
            },
            "profile_picture": {
                "type": StringOrNothingType,
                "display_name": "Profile Picture",
                "short_desc": "The worker's profile picture URL",
                "desc": "URL to the worker's profile picture",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DeelDataProvider
