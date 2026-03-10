# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close contact record type
public class CloseContactDataType inherits HashDataType {
    public {
        #! Field definitions for Close contacts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The associated lead ID",
                "desc": "The unique identifier of the lead this contact belongs to",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The contact name",
                "desc": "The full name of the contact",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The contact title",
                "desc": "The job title of the contact",
            },
            "emails": {
                "type": new SoftListDataType("Emails", new CloseEmailEntryDataType(), True),
                "display_name": "Email Addresses",
                "short_desc": "The contact email addresses",
                "desc": "A list of email address objects for the contact, each with `type` and "
                    "`email` fields",
            },
            "phones": {
                "type": new SoftListDataType("Phones", new ClosePhoneEntryDataType(), True),
                "display_name": "Phone Numbers",
                "short_desc": "The contact phone numbers",
                "desc": "A list of phone number objects for the contact, each with `type` and "
                    "`phone` fields",
            },
            "urls": {
                "type": new SoftListDataType("URLs", new CloseUrlEntryDataType(), True),
                "display_name": "URLs",
                "short_desc": "The contact URLs",
                "desc": "A list of URL objects for the contact, each with `type` and `url` fields",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this contact belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user ID who created the contact",
                "desc": "The unique identifier of the user who created this contact",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user ID who last updated the contact",
                "desc": "The unique identifier of the user who last updated this contact",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the contact was created",
                "desc": "The timestamp when the contact was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the contact was last updated",
                "desc": "The timestamp when the contact was last updated (ISO 8601 format)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
