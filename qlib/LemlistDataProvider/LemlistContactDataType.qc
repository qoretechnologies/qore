# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist contact data type
public class LemlistContactDataType inherits HashDataType {
    public {
        const Fields = {
            "_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "Unique contact identifier",
                "desc": "The unique identifier for the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Contact email address",
                "desc": "The email address of the contact",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact first name",
                "desc": "The first name of the contact",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact last name",
                "desc": "The last name of the contact",
            },
            "companyName": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Contact company name",
                "desc": "The name of the company the contact works for",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Contact phone number",
                "desc": "The phone number of the contact",
            },
            "linkedinUrl": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "Contact LinkedIn profile URL",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Contact or company website",
                "desc": "The website URL associated with the contact",
            },
            "isUnsubscribed": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Unsubscribed",
                "short_desc": "Whether the contact has unsubscribed",
                "desc": "`True` if the contact has unsubscribed from outreach",
            },
            "campaigns": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Campaigns",
                "short_desc": "Campaigns this contact is in",
                "desc": "List of campaign identifiers this contact is enrolled in",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Contact creation timestamp",
                "desc": "ISO 8601 timestamp when the contact was created",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Contact last update timestamp",
                "desc": "ISO 8601 timestamp when the contact was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LemlistDataProvider
