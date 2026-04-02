# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesDealDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales deal record type
public class FreshsalesDealDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales deals
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The deal ID",
                "desc": "The unique identifier of the deal",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Deal name",
                "desc": "The name or title of the deal",
            },
            "amount": {
                "type": FloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "Deal value",
                "desc": "The monetary value of the deal",
            },
            "base_currency_amount": {
                "type": FloatOrNothingType,
                "display_name": "Base Currency Amount",
                "short_desc": "Amount in base currency",
                "desc": "The deal amount converted to the base currency",
            },
            "currency_id": {
                "type": IntOrNothingType,
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The ID of the currency for this deal",
            },
            "sales_account_id": {
                "type": IntOrNothingType,
                "display_name": "Account ID",
                "short_desc": "Related account ID",
                "desc": "The ID of the associated sales account",
            },
            "deal_stage_id": {
                "type": IntOrNothingType,
                "display_name": "Deal Stage ID",
                "short_desc": "Pipeline stage",
                "desc": "The ID of the current deal stage in the pipeline",
            },
            "deal_pipeline_id": {
                "type": IntOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "Pipeline identifier",
                "desc": "The ID of the deal pipeline",
            },
            "deal_type_id": {
                "type": IntOrNothingType,
                "display_name": "Deal Type ID",
                "short_desc": "Deal category",
                "desc": "The ID of the deal type/category",
            },
            "deal_reason_id": {
                "type": IntOrNothingType,
                "display_name": "Deal Reason ID",
                "short_desc": "Win/loss reason",
                "desc": "The ID of the reason for winning or losing the deal",
            },
            "owner_id": {
                "type": IntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user who owns this deal",
            },
            "lead_source_id": {
                "type": IntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "Lead source identifier",
                "desc": "The ID of the lead source for this deal",
            },
            "campaign_id": {
                "type": IntOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign identifier",
                "desc": "The ID of the campaign associated with this deal",
            },
            "deal_payment_status_id": {
                "type": IntOrNothingType,
                "display_name": "Payment Status ID",
                "short_desc": "Payment status",
                "desc": "The ID of the deal payment status",
            },
            "territory_id": {
                "type": IntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory for this deal",
            },
            "expected_close": {
                "type": StringOrNothingType,
                "display_name": "Expected Close",
                "short_desc": "Expected close date",
                "desc": "The anticipated closure date of the deal",
            },
            "closed_date": {
                "type": StringOrNothingType,
                "display_name": "Closed Date",
                "short_desc": "Actual close date",
                "desc": "The actual date the deal was closed",
            },
            "probability": {
                "type": IntOrNothingType,
                "display_name": "Probability",
                "short_desc": "Win probability",
                "desc": "The probability of winning the deal (`0` to `100`)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the deal was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the deal was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales deal request data type for create operations
public class FreshsalesCreateDealRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a deal
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "required": True,
                "short_desc": "Deal name",
                "desc": "The name or title of the deal",
                "example_value": "Enterprise License",
            },
            "amount": {
                "type": SoftFloatType,
                "display_name": "Amount",
                "required": True,
                "short_desc": "Deal value",
                "desc": "The monetary value of the deal",
                "example_value": 10000.00,
            },
            "sales_account_id": {
                "type": SoftIntType,
                "display_name": "Account ID",
                "required": True,
                "short_desc": "Related account ID",
                "desc": "The ID of the associated sales account",
                "ref_data": "sales_accounts",
            },
            "currency_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Currency ID",
                "short_desc": "Currency for the deal",
                "desc": "The ID of the currency for this deal. If not specified, the account's "
                    "default currency is used",
                "ref_data": "currencies",
            },
            "deal_stage_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Deal Stage ID",
                "short_desc": "Pipeline stage",
                "desc": "The ID of the deal stage in the pipeline",
                "preselected": True,
                "ref_data": "deal_stages",
            },
            "deal_pipeline_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "Pipeline identifier",
                "desc": "The ID of the deal pipeline",
                "ref_data": "deal_pipelines",
            },
            "deal_type_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Deal Type ID",
                "short_desc": "Deal category",
                "desc": "The ID of the deal type/category",
                "ref_data": "deal_types",
            },
            "deal_reason_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Deal Reason ID",
                "short_desc": "Win/loss reason",
                "desc": "The ID of the reason for winning or losing the deal",
                "ref_data": "deal_reasons",
            },
            "owner_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user to assign as owner",
                "ref_data": "owners",
            },
            "lead_source_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "Lead source identifier",
                "desc": "The ID of the lead source for this deal",
                "ref_data": "lead_sources",
            },
            "campaign_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign identifier",
                "desc": "The ID of the campaign associated with this deal",
                "ref_data": "campaigns",
            },
            "deal_payment_status_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Payment Status ID",
                "short_desc": "Payment status",
                "desc": "The ID of the deal payment status",
                "ref_data": "deal_payment_statuses",
            },
            "territory_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory for this deal",
                "ref_data": "territories",
            },
            "expected_close": {
                "type": StringOrNothingType,
                "display_name": "Expected Close",
                "short_desc": "Expected close date",
                "desc": "The anticipated closure date (format: `YYYY-MM-DD`)",
                "preselected": True,
            },
            "closed_date": {
                "type": StringOrNothingType,
                "display_name": "Closed Date",
                "short_desc": "Actual close date",
                "desc": "The actual date the deal was closed (format: `YYYY-MM-DD`)",
            },
            "probability": {
                "type": SoftIntOrNothingType,
                "display_name": "Probability",
                "short_desc": "Win probability",
                "desc": "The probability of winning the deal (`0` to `100`)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales deal request data type for update operations
public class FreshsalesUpdateDealRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a deal
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Deal ID",
                "required": True,
                "short_desc": "The deal ID to update",
                "desc": "The unique identifier of the deal to update",
            },
        } + (map {$1.key: $1.value + {"required": False}},
            FreshsalesCreateDealRequestDataType::Fields.pairIterator());
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales deal request data type for delete operations
public class FreshsalesDeleteDealRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a deal
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Deal ID",
                "required": True,
                "short_desc": "The deal ID to delete",
                "desc": "The unique identifier of the deal to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
