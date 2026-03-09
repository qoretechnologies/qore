# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapOpportunityDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap opportunity record type
public class KeapOpportunityDataType inherits HashDataType {
    private {
        #! Field definitions for Keap opportunities
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The opportunity ID",
                "desc": "The unique identifier of the opportunity",
            },
            "opportunity_title": {
                "type": StringOrNothingType,
                "display_name": "Opportunity Title",
                "short_desc": "The opportunity title",
                "desc": "The title of the opportunity",
                "example_value": "New Enterprise Deal",
            },
            "contact": {
                "type": new KeapContactRefDataType(AutoHashOrNothingType),
                "display_name": "Contact",
                "short_desc": "The associated contact",
                "desc": "The contact associated with this opportunity",
            },
            "stage": {
                "type": new KeapStageDataType(AutoHashOrNothingType),
                "display_name": "Stage",
                "short_desc": "The opportunity stage",
                "desc": "The current stage of the opportunity in the sales pipeline",
            },
            "estimated_close_date": {
                "type": DateOrNothingType,
                "display_name": "Estimated Close Date",
                "short_desc": "The estimated close date",
                "desc": "The estimated date when the opportunity will close",
            },
            "projected_revenue_high": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue High",
                "short_desc": "High revenue estimate",
                "desc": "The high end of the projected revenue for this opportunity",
                "example_value": 50000.0,
            },
            "projected_revenue_low": {
                "type": FloatOrNothingType,
                "display_name": "Projected Revenue Low",
                "short_desc": "Low revenue estimate",
                "desc": "The low end of the projected revenue for this opportunity",
                "example_value": 25000.0,
            },
            "opportunity_notes": {
                "type": StringOrNothingType,
                "display_name": "Opportunity Notes",
                "short_desc": "Notes about the opportunity",
                "desc": "Free-text notes about the opportunity",
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the opportunity was created",
                "desc": "The timestamp when the opportunity was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the opportunity was last updated",
                "desc": "The timestamp when the opportunity was last updated",
            },
            "next_action_date": {
                "type": DateOrNothingType,
                "display_name": "Next Action Date",
                "short_desc": "Date of the next action",
                "desc": "The date when the next action should be taken on this opportunity",
            },
            "next_action_notes": {
                "type": StringOrNothingType,
                "display_name": "Next Action Notes",
                "short_desc": "Notes for the next action",
                "desc": "Notes describing the next action to be taken on this opportunity",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
