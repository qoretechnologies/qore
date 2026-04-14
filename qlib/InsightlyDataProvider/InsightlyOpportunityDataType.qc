# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyOpportunityDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

#! Insightly opportunity record type
public class InsightlyOpportunityDataType inherits HashDataType {
    public {
        const Fields = {
            "OPPORTUNITY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Opportunity ID",
                "short_desc": "The unique opportunity identifier",
                "desc": "The unique identifier of the opportunity",
            },
            "OPPORTUNITY_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Opportunity Name",
                "short_desc": "The opportunity name",
                "desc": "The name of the opportunity",
            },
            "OPPORTUNITY_DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Opportunity Details",
                "short_desc": "Description of the opportunity",
                "desc": "Detailed description of the opportunity",
            },
            "PROBABILITY": {
                "type": SoftIntOrNothingType,
                "display_name": "Probability",
                "short_desc": "Win probability percentage",
                "desc": "The probability of winning the opportunity (0-100)",
            },
            "BID_CURRENCY": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Currency",
                "short_desc": "Currency code for the bid",
                "desc": "The ISO 4217 currency code for the bid amount",
            },
            "BID_AMOUNT": {
                "type": SoftFloatOrNothingType,
                "display_name": "Bid Amount",
                "short_desc": "The bid amount",
                "desc": "The monetary value of the opportunity",
            },
            "BID_TYPE": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Type",
                "short_desc": "The bid type",
                "desc": "The type of bid",
            },
            "BID_DURATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Duration",
                "short_desc": "The bid duration",
                "desc": "The duration of the bid",
            },
            "ACTUAL_CLOSE_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Actual Close Date",
                "short_desc": "The actual close date",
                "desc": "The actual date the opportunity was closed",
            },
            "FORECAST_CLOSE_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Forecast Close Date",
                "short_desc": "The forecasted close date",
                "desc": "The expected date the opportunity will close",
            },
            "OPPORTUNITY_STATE": {
                "type": SoftStringOrNothingType,
                "display_name": "Opportunity State",
                "short_desc": "Current state of the opportunity",
                "desc": "The current state of the opportunity",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Open", "display_name": "Open",
                        "desc": "Opportunity is open"},
                    <AllowedValueInfo>{"value": "Abandoned", "display_name": "Abandoned",
                        "desc": "Opportunity has been abandoned"},
                    <AllowedValueInfo>{"value": "Lost", "display_name": "Lost",
                        "desc": "Opportunity was lost"},
                    <AllowedValueInfo>{"value": "Suspended", "display_name": "Suspended",
                        "desc": "Opportunity is suspended"},
                    <AllowedValueInfo>{"value": "Won", "display_name": "Won",
                        "desc": "Opportunity was won"},
                ),
            },
            "OPPORTUNITY_STATE_REASON_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "State Reason ID",
                "short_desc": "The state reason identifier",
                "desc": "The ID of the reason for the current opportunity state",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The opportunity category identifier",
                "desc": "The ID of the opportunity category",
            },
            "PIPELINE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "The pipeline identifier",
                "desc": "The ID of the pipeline this opportunity belongs to",
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
                "short_desc": "Opportunity image URL",
                "desc": "The URL of the opportunity's image",
            },
            "RESPONSIBLE_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Responsible User ID",
                "short_desc": "Responsible user identifier",
                "desc": "The ID of the user responsible for this opportunity",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this opportunity",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this opportunity",
                "allowed_values": VisibleToAllowedValues,
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyOppCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the opportunity",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyOppTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Opportunity tags",
                "desc": "A list of tags applied to the opportunity",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyOppLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the opportunity was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the opportunity was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class InsightlyCreateOpportunityRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "OPPORTUNITY_NAME": {
                "type": SoftStringType,
                "display_name": "Opportunity Name",
                "required": True,
                "short_desc": "The opportunity name",
                "desc": "The name of the opportunity (required)",
                "example_value": "New Enterprise Deal",
            },
            "OPPORTUNITY_DETAILS": {
                "type": SoftStringOrNothingType,
                "display_name": "Opportunity Details",
                "short_desc": "Description of the opportunity",
                "desc": "Detailed description of the opportunity",
                "preselected": True,
            },
            "PROBABILITY": {
                "type": SoftIntOrNothingType,
                "display_name": "Probability",
                "short_desc": "Win probability percentage",
                "desc": "The probability of winning the opportunity (0-100)",
            },
            "BID_CURRENCY": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Currency",
                "short_desc": "Currency code for the bid",
                "desc": "The ISO 4217 currency code for the bid amount (e.g., `USD`)",
            },
            "BID_AMOUNT": {
                "type": SoftFloatOrNothingType,
                "display_name": "Bid Amount",
                "short_desc": "The bid amount",
                "desc": "The monetary value of the opportunity",
                "preselected": True,
            },
            "BID_TYPE": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Type",
                "short_desc": "The bid type",
                "desc": "The type of bid",
            },
            "BID_DURATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Bid Duration",
                "short_desc": "The bid duration",
                "desc": "The duration of the bid",
            },
            "FORECAST_CLOSE_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Forecast Close Date",
                "short_desc": "The forecasted close date",
                "desc": "The expected date the opportunity will close",
                "preselected": True,
            },
            "OPPORTUNITY_STATE": {
                "type": SoftStringOrNothingType,
                "display_name": "Opportunity State",
                "short_desc": "Current state of the opportunity",
                "desc": "The current state of the opportunity",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Open", "display_name": "Open",
                        "desc": "Opportunity is open"},
                    <AllowedValueInfo>{"value": "Abandoned", "display_name": "Abandoned",
                        "desc": "Opportunity has been abandoned"},
                    <AllowedValueInfo>{"value": "Lost", "display_name": "Lost",
                        "desc": "Opportunity was lost"},
                    <AllowedValueInfo>{"value": "Suspended", "display_name": "Suspended",
                        "desc": "Opportunity is suspended"},
                    <AllowedValueInfo>{"value": "Won", "display_name": "Won",
                        "desc": "Opportunity was won"},
                ),
            },
            "OPPORTUNITY_STATE_REASON_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "State Reason ID",
                "short_desc": "The state reason identifier",
                "desc": "The ID of the reason for the current state",
                "ref_data": "opportunity_state_reasons",
            },
            "CATEGORY_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The opportunity category identifier",
                "desc": "The ID of the opportunity category",
                "ref_data": "opportunity_categories",
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
                "desc": "The ID of the user responsible for this opportunity",
                "ref_data": "users",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this opportunity",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this opportunity",
                "allowed_values": VisibleToAllowedValues,
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyOppCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyOppTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Opportunity tags",
                "desc": "A list of tags to apply to the opportunity",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyOppLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class InsightlyUpdateOpportunityRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "OPPORTUNITY_ID": {
                "type": SoftIntType,
                "display_name": "Opportunity ID",
                "required": True,
                "short_desc": "The opportunity ID to update",
                "desc": "The unique identifier of the opportunity to update",
                "ref_data": "opportunities",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateOpportunityRequestDataType::Fields);
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class InsightlyDeleteOpportunityRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "OPPORTUNITY_ID": {
                "type": SoftIntType,
                "display_name": "Opportunity ID",
                "required": True,
                "short_desc": "The opportunity ID to delete",
                "desc": "The unique identifier of the opportunity to delete",
                "ref_data": "opportunities",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
