# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyProjectDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyProjectDataType inherits HashDataType {
    public {
        const Fields = {
            "PROJECT_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Project ID",
                "short_desc": "The unique project identifier",
                "desc": "The unique identifier of the project",
            },
            "PROJECT_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Project Name",
                "short_desc": "The project name",
                "desc": "The name of the project",
            },
            "PROJECT_DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Project Details",
                "short_desc": "Description of the project",
                "desc": "Detailed description of the project",
            },
            "STATUS": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Project status",
                "desc": "The current status of the project",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Not Started", "display_name": "Not Started",
                        "desc": "Not yet started"},
                    <AllowedValueInfo>{"value": "In Progress", "display_name": "In Progress",
                        "desc": "Currently in progress"},
                    <AllowedValueInfo>{"value": "Deferred", "display_name": "Deferred",
                        "desc": "Deferred to a later date"},
                    <AllowedValueInfo>{"value": "Completed", "display_name": "Completed",
                        "desc": "Successfully completed"},
                    <AllowedValueInfo>{"value": "Cancelled", "display_name": "Cancelled",
                        "desc": "Cancelled"},
                ),
            },
            "OPPORTUNITY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Opportunity ID",
                "short_desc": "Linked opportunity identifier",
                "desc": "The ID of the opportunity this project is linked to",
            },
            "STARTED_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Started Date",
                "short_desc": "Project start date",
                "desc": "The date the project was started",
            },
            "COMPLETED_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Completed Date",
                "short_desc": "Project completion date",
                "desc": "The date the project was completed",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The project category identifier",
                "desc": "The ID of the project category",
            },
            "PIPELINE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "The pipeline identifier",
                "desc": "The ID of the pipeline this project belongs to",
            },
            "STAGE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Stage ID",
                "short_desc": "The pipeline stage identifier",
                "desc": "The ID of the current pipeline stage",
            },
            "IMAGE_URL": {
                "type": SoftStringOrNothingType,
                "display_name": "Image URL",
                "short_desc": "Project image URL",
                "desc": "The URL of the project's image",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this project",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this project",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this project",
                "allowed_values": VisibleToAllowedValues,
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyProjCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the project",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyProjTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Project tags",
                "desc": "A list of tags applied to the project",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyProjLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the project was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the project was last updated",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateProjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PROJECT_NAME": {
                "type": SoftStringType,
                "display_name": "Project Name",
                "required": True,
                "short_desc": "The project name",
                "desc": "The name of the project (required)",
                "example_value": "Website Redesign",
            },
            "PROJECT_DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Project Details",
                "short_desc": "Description of the project",
                "desc": "Detailed description of the project",
                "preselected": True,
            },
            "STATUS": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Project status",
                "desc": "The current status of the project",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Not Started", "display_name": "Not Started",
                        "desc": "Not yet started"},
                    <AllowedValueInfo>{"value": "In Progress", "display_name": "In Progress",
                        "desc": "Currently in progress"},
                    <AllowedValueInfo>{"value": "Deferred", "display_name": "Deferred",
                        "desc": "Deferred to a later date"},
                    <AllowedValueInfo>{"value": "Completed", "display_name": "Completed",
                        "desc": "Successfully completed"},
                    <AllowedValueInfo>{"value": "Cancelled", "display_name": "Cancelled",
                        "desc": "Cancelled"},
                ),
            },
            "OPPORTUNITY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Opportunity ID",
                "short_desc": "Linked opportunity identifier",
                "desc": "The ID of the opportunity this project is linked to",
                "ref_data": "opportunities",
            },
            "STARTED_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Started Date",
                "short_desc": "Project start date",
                "desc": "The date the project was started",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The project category identifier",
                "desc": "The ID of the project category",
                "ref_data": "project_categories",
            },
            "PIPELINE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "The pipeline identifier",
                "desc": "The ID of the pipeline",
                "ref_data": "pipelines",
            },
            "STAGE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Stage ID",
                "short_desc": "The pipeline stage identifier",
                "desc": "The ID of the pipeline stage",
                "ref_data": "pipeline_stages",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this project",
                "ref_data": "users",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this project",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this project",
                "allowed_values": VisibleToAllowedValues,
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyProjCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyProjTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Project tags",
                "desc": "A list of tags to apply to the project",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyProjLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyUpdateProjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PROJECT_ID": {
                "type": SoftIntType,
                "display_name": "Project ID",
                "required": True,
                "short_desc": "The project ID to update",
                "desc": "The unique identifier of the project to update",
                "ref_data": "projects",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateProjectRequestDataType::Fields);
    }

    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteProjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PROJECT_ID": {
                "type": SoftIntType,
                "display_name": "Project ID",
                "required": True,
                "short_desc": "The project ID to delete",
                "desc": "The unique identifier of the project to delete",
                "ref_data": "projects",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
