# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseLeadDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close lead record type
public class CloseLeadDataType inherits HashDataType {
    public {
        #! Field definitions for Close leads
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead",
            },
            "display_name": {
                "type": StringOrNothingType,
                "display_name": "Display Name",
                "short_desc": "The display name of the lead",
                "desc": "The display name of the lead, typically the company or organization name",
            },
            "status_id": {
                "type": StringOrNothingType,
                "display_name": "Status ID",
                "short_desc": "The lead status ID",
                "desc": "The unique identifier of the lead's current status",
            },
            "status_label": {
                "type": StringOrNothingType,
                "display_name": "Status Label",
                "short_desc": "The lead status label",
                "desc": "The human-readable label of the lead's current status (e.g., Potential, "
                    "Qualified, Customer)",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The lead description",
                "desc": "A text description of the lead with additional context or notes",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The lead URL",
                "desc": "The website URL associated with the lead",
            },
            "contacts": {
                "type": AutoListOrNothingType,
                "display_name": "Contacts",
                "short_desc": "Contacts associated with the lead",
                "desc": "A list of contact objects associated with this lead",
            },
            "opportunities": {
                "type": AutoListOrNothingType,
                "display_name": "Opportunities",
                "short_desc": "Opportunities associated with the lead",
                "desc": "A list of opportunity objects associated with this lead",
            },
            "tasks": {
                "type": AutoListOrNothingType,
                "display_name": "Tasks",
                "short_desc": "Tasks associated with the lead",
                "desc": "A list of task objects associated with this lead",
            },
            "addresses": {
                "type": new SoftListDataType("Addresses", new CloseAddressEntryDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Addresses associated with the lead",
                "desc": "A list of address objects associated with this lead",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this lead belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user ID who created the lead",
                "desc": "The unique identifier of the user who created this lead",
            },
            "created_by_name": {
                "type": StringOrNothingType,
                "display_name": "Created By Name",
                "short_desc": "The name of the user who created the lead",
                "desc": "The display name of the user who created this lead",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user ID who last updated the lead",
                "desc": "The unique identifier of the user who last updated this lead",
            },
            "updated_by_name": {
                "type": StringOrNothingType,
                "display_name": "Updated By Name",
                "short_desc": "The name of the user who last updated the lead",
                "desc": "The display name of the user who last updated this lead",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the lead was created",
                "desc": "The timestamp when the lead was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the lead was last updated",
                "desc": "The timestamp when the lead was last updated (ISO 8601 format)",
            },
            "html_url": {
                "type": StringOrNothingType,
                "display_name": "HTML URL",
                "short_desc": "The Close web UI URL for this lead",
                "desc": "The URL to view this lead in the Close web application",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
