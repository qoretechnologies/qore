# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyOrganisationDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly organisation record type
public class InsightlyOrganisationDataType inherits HashDataType {
    public {
        const Fields = {
            "ORGANISATION_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Organisation ID",
                "short_desc": "The unique organisation identifier",
                "desc": "The unique identifier of the organisation",
            },
            "ORGANISATION_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Organisation Name",
                "short_desc": "The organisation name",
                "desc": "The name of the organisation",
            },
            "BACKGROUND": {
                "type": SoftStringOrNothingType,
                "display_name": "Background",
                "short_desc": "Background information",
                "desc": "Background information about the organisation",
            },
            "IMAGE_URL": {
                "type": SoftStringOrNothingType,
                "display_name": "Image URL",
                "short_desc": "Organisation image URL",
                "desc": "The URL of the organisation's image",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this organisation",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this organisation",
                "allowed_values": VisibleToAllowedValues,
            },
            "VISIBLE_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Visible Team ID",
                "short_desc": "Team visibility identifier",
                "desc": "The ID of the team that can see this organisation when `VISIBLE_TO` "
                    "is `TEAM`",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyOrgRespCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the organisation",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyOrgRespAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Organisation addresses",
                "desc": "A list of addresses for the organisation",
            },
            "CONTACTINFOS": {
                "type": new ListDataType("InsightlyOrgRespContactInfoList",
                    new InsightlyContactInfoDataType(), True),
                "display_name": "Contact Infos",
                "short_desc": "Additional contact information",
                "desc": "A list of additional contact information entries",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyOrgRespLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyOrgRespTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Organisation tags",
                "desc": "A list of tags applied to the organisation",
            },
            "DATES": {
                "type": new ListDataType("InsightlyOrgRespDateList",
                    new InsightlyDateDataType(), True),
                "display_name": "Dates",
                "short_desc": "Important dates",
                "desc": "A list of important dates for the organisation",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the organisation was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the organisation was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly organisation request data type for create operations
public class InsightlyCreateOrganisationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "ORGANISATION_NAME": {
                "type": SoftStringType,
                "display_name": "Organisation Name",
                "required": True,
                "short_desc": "The organisation name",
                "desc": "The name of the organisation (required)",
                "example_value": "Acme Corp",
            },
            "BACKGROUND": {
                "type": SoftStringOrNothingType,
                "display_name": "Background",
                "short_desc": "Background information",
                "desc": "Background information about the organisation",
                "preselected": True,
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this organisation",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this organisation",
                "allowed_values": VisibleToAllowedValues,
            },
            "VISIBLE_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Visible Team ID",
                "short_desc": "Team visibility identifier",
                "desc": "The ID of the team that can see this organisation when `VISIBLE_TO` "
                    "is `TEAM`",
                "ref_data": "teams",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyOrgCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyOrgAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Organisation addresses",
                "desc": "A list of addresses for the organisation",
            },
            "CONTACTINFOS": {
                "type": new ListDataType("InsightlyOrgContactInfoList",
                    new InsightlyContactInfoDataType(), True),
                "display_name": "Contact Infos",
                "short_desc": "Additional contact information",
                "desc": "A list of additional contact information entries",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyOrgLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyOrgTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Organisation tags",
                "desc": "A list of tags to apply to the organisation",
            },
            "DATES": {
                "type": new ListDataType("InsightlyOrgDateList",
                    new InsightlyDateDataType(), True),
                "display_name": "Dates",
                "short_desc": "Important dates",
                "desc": "A list of important dates for the organisation",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly organisation request data type for update operations
public class InsightlyUpdateOrganisationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "ORGANISATION_ID": {
                "type": SoftIntType,
                "display_name": "Organisation ID",
                "required": True,
                "short_desc": "The organisation ID to update",
                "desc": "The unique identifier of the organisation to update",
                "ref_data": "organisations",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateOrganisationRequestDataType::Fields);
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly organisation request data type for delete operations
public class InsightlyDeleteOrganisationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "ORGANISATION_ID": {
                "type": SoftIntType,
                "display_name": "Organisation ID",
                "required": True,
                "short_desc": "The organisation ID to delete",
                "desc": "The unique identifier of the organisation to delete",
                "ref_data": "organisations",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
