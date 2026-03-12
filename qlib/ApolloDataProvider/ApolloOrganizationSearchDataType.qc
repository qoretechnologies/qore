# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloOrganizationSearchDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo organization search response type
public class ApolloOrganizationSearchDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo organization search responses
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Organization ID",
                "desc": "The unique identifier of the organization",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Company name",
                "desc": "The name of the company",
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "Website",
                "desc": "The website URL of the organization",
            },
            "industry": {
                "type": StringOrNothingType,
                "display_name": "Industry",
                "short_desc": "Industry",
                "desc": "The industry of the organization",
            },
            "estimated_num_employees": {
                "type": IntOrNothingType,
                "display_name": "Estimated Employees",
                "short_desc": "Employee count",
                "desc": "The estimated number of employees in the organization",
            },
            "founded_year": {
                "type": IntOrNothingType,
                "display_name": "Founded Year",
                "short_desc": "Founded year",
                "desc": "The year the organization was founded",
            },
            "annual_revenue": {
                "type": FloatOrNothingType,
                "display_name": "Annual Revenue",
                "short_desc": "Annual revenue",
                "desc": "The annual revenue of the organization",
            },
            "total_funding": {
                "type": FloatOrNothingType,
                "display_name": "Total Funding",
                "short_desc": "Total funding",
                "desc": "The total funding received by the organization",
            },
            "latest_funding_stage": {
                "type": StringOrNothingType,
                "display_name": "Latest Funding Stage",
                "short_desc": "Latest funding stage",
                "desc": "The latest funding stage of the organization",
            },
            "latest_funding_round_date": {
                "type": DateOrNothingType,
                "display_name": "Latest Funding Round Date",
                "short_desc": "Last funding date",
                "desc": "The date of the latest funding round",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city where the organization is located",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "The state where the organization is located",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country",
                "desc": "The country where the organization is located",
            },
            "technology_names": {
                "type": AutoListOrNothingType,
                "display_name": "Technology Names",
                "short_desc": "Technologies used",
                "desc": "The list of technologies used by the organization",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
