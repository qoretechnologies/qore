# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapUpdateOpportunityDataProvider.qc Keap update opportunity action data provider

/*  KeapUpdateOpportunityDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for updating an opportunity
public class KeapUpdateOpportunityRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "opportunity_id": {
                "type": StringType,
                "display_name": "Opportunity ID",
                "short_desc": "The opportunity ID to update",
                "desc": "The unique identifier of the opportunity to update",
                "required": True,
            },
            "opportunity_title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The opportunity title",
                "desc": "The new title of the opportunity",
            },
            "stage": {
                "type": AutoHashOrNothingType,
                "display_name": "Stage",
                "short_desc": "The opportunity stage",
                "desc": "The new pipeline stage for this opportunity. Must be a hash with an "
                    "`id` field containing the stage's integer ID",
            },
            "estimated_close_date": {
                "type": DateOrNothingType,
                "display_name": "Estimated Close Date",
                "short_desc": "The estimated close date",
                "desc": "The new estimated close date of the opportunity",
            },
            "projected_revenue_high": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue High",
                "short_desc": "High revenue estimate",
                "desc": "The new high estimate of projected revenue for the opportunity",
            },
            "projected_revenue_low": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue Low",
                "short_desc": "Low revenue estimate",
                "desc": "The new low estimate of projected revenue for the opportunity",
            },
            "opportunity_notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the opportunity",
                "desc": "The new notes about the opportunity",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap update opportunity action data provider
public class KeapUpdateOpportunityDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing opportunity in Keap",
            "type": "KeapUpdateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing opportunity in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapUpdateOpportunityRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapOpportunityDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.opportunity_id;
        return doPatch("opportunities/" + id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
