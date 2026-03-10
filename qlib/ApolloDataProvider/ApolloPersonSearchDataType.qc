# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloPersonSearchDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo person search response type
public class ApolloPersonSearchDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo person search responses
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Person ID",
                "desc": "The unique identifier of the person",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the person",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the person",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "Full name",
                "desc": "The full name of the person",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email",
                "desc": "The email address of the person",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the person",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "Company name",
                "desc": "The name of the company the person belongs to",
            },
            "headline": {
                "type": StringOrNothingType,
                "display_name": "Headline",
                "short_desc": "Professional headline",
                "desc": "The professional headline of the person",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "LinkedIn URL",
                "desc": "The LinkedIn profile URL of the person",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city where the person is located",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "The state where the person is located",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "The country where the person is located",
            },
            "seniority": {
                "type": StringOrNothingType,
                "display_name": "Seniority",
                "short_desc": "Seniority level",
                "desc": "The seniority level of the person",
            },
            "departments": {
                "type": AutoListOrNothingType,
                "display_name": "Departments",
                "short_desc": "Departments",
                "desc": "The list of departments the person belongs to",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
