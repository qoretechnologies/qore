# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoHighLevelDataProvider module definition

/*  GoHighLevelContactDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! GoHighLevel contact record type
public class GoHighLevelContactDataType inherits HashDataType {
    public {
        #! Field definitions for GoHighLevel contacts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "Unique identifier of the contact",
                "desc": "Unique identifier of the contact",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The contact's first name",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The contact's last name",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "The contact's full name",
                "desc": "The contact's full name",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The contact's email address",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The contact's phone number",
                "desc": "The contact's phone number",
            },
            "companyName": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The contact's company name",
                "desc": "The contact's company name",
            },
            "address1": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The contact's street address",
                "desc": "The contact's street address",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The contact's city",
                "desc": "The contact's city",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The contact's state or province",
                "desc": "The contact's state or province",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The contact's country (2-letter code)",
                "desc": "The contact's country (2-letter code)",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The contact's postal/zip code",
                "desc": "The contact's postal/zip code",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The contact's website URL",
                "desc": "The contact's website URL",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "The contact's timezone",
                "desc": "The contact's timezone",
            },
            "tags": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags assigned to the contact",
                "desc": "Tags assigned to the contact",
            },
            "source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "The source of the contact",
                "desc": "The source of the contact",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID the contact is assigned to",
                "desc": "User ID the contact is assigned to",
            },
            "dateAdded": {
                "type": StringOrNothingType,
                "display_name": "Date Added",
                "short_desc": "Timestamp when the contact was created",
                "desc": "Timestamp when the contact was created",
            },
            "dateUpdated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Timestamp when the contact was last updated",
                "desc": "Timestamp when the contact was last updated",
            },
            "dnd": {
                "type": SoftBoolOrNothingType,
                "display_name": "Do Not Disturb",
                "short_desc": "Whether the contact has do-not-disturb enabled",
                "desc": "Whether the contact has do-not-disturb enabled",
            },
            "locationId": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location/sub-account ID",
                "desc": "The location/sub-account ID",
            },
            "customFields": {
                "type": new SoftListDataType(new GoHighLevelCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values for the contact",
                "desc": "Custom field values for the contact; each entry has an `id` "
                    "(custom field ID) and `field_value`",
            },
            "businessId": {
                "type": StringOrNothingType,
                "display_name": "Business ID",
                "short_desc": "The business ID associated with the contact",
                "desc": "The business ID associated with the contact",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace GoHighLevelDataProvider
