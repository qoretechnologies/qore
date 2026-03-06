# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateOpportunityDataProvider.qc Keap create opportunity action data provider

/*  KeapCreateOpportunityDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an opportunity
public class KeapCreateOpportunityRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "opportunity_title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The opportunity title",
                "desc": "The title of the opportunity",
                "required": True,
            },
            "contact": {
                "type": AutoHashOrNothingType,
                "display_name": "Contact",
                "short_desc": "The associated contact",
                "desc": "The contact to associate with this opportunity. Must be a hash with "
                    "an `id` field containing the contact's integer ID",
            },
            "stage": {
                "type": AutoHashOrNothingType,
                "display_name": "Stage",
                "short_desc": "The opportunity stage",
                "desc": "The pipeline stage for this opportunity. Must be a hash with an `id` "
                    "field containing the stage's integer ID",
            },
            "estimated_close_date": {
                "type": DateOrNothingType,
                "display_name": "Estimated Close Date",
                "short_desc": "The estimated close date",
                "desc": "The estimated close date of the opportunity",
            },
            "projected_revenue_high": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue High",
                "short_desc": "High revenue estimate",
                "desc": "The high estimate of projected revenue for the opportunity",
            },
            "projected_revenue_low": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue Low",
                "short_desc": "Low revenue estimate",
                "desc": "The low estimate of projected revenue for the opportunity",
            },
            "opportunity_notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the opportunity",
                "desc": "Notes about the opportunity",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create opportunity action data provider
public class KeapCreateOpportunityDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new opportunity in Keap",
            "type": "KeapCreateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new opportunity in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateOpportunityRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapOpportunityDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("opportunities", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
