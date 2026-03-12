# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateOpportunityDataProvider.qc Close create opportunity action data provider

/*  CloseCreateOpportunityDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an opportunity
public class CloseCreateOpportunityRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an opportunity
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead to associate the opportunity with",
                "desc": "The unique identifier of the lead to associate this opportunity with",
                "required": True,
            },
            "status_id": {
                "type": StringType,
                "display_name": "Status ID",
                "short_desc": "The opportunity status ID",
                "desc": "The unique identifier of the opportunity status to assign",
                "required": True,
            },
            "value": {
                "type": IntOrNothingType,
                "display_name": "Value",
                "short_desc": "The opportunity value in cents",
                "desc": "The monetary value of the opportunity in cents (e.g., 10000 = $100.00)",
            },
            "value_period": {
                "type": StringOrNothingType,
                "display_name": "Value Period",
                "short_desc": "The value period",
                "desc": "The billing period for the opportunity value: `one_time`, `monthly`, or "
                    "`annual`",
                "example_value": "one_time",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "One Time",
                        "short_desc": "One-time payment",
                        "desc": "One-time billing period",
                        "value": "one_time",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Monthly",
                        "short_desc": "Monthly recurring",
                        "desc": "Monthly billing period",
                        "value": "monthly",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Annual",
                        "short_desc": "Annual recurring",
                        "desc": "Annual billing period",
                        "value": "annual",
                    },
                ),
            },
            "confidence": {
                "type": IntOrNothingType,
                "display_name": "Confidence",
                "short_desc": "The confidence percentage",
                "desc": "The confidence percentage of winning this opportunity, from 0 to 100",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes about the opportunity",
                "desc": "A text note with additional context about the opportunity",
                "example_value": "Initial sales conversation went well",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The unique identifier of the primary contact for this opportunity",
            },
            "value_currency": {
                "type": StringOrNothingType,
                "display_name": "Value Currency",
                "short_desc": "The currency code",
                "desc": "ISO 4217 currency code for the opportunity value (e.g., `USD`, `EUR`, `GBP`)",
                "example_value": "USD",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The assigned user",
                "desc": "The unique identifier of the user to assign this opportunity to",
            },
            "date_won": {
                "type": StringOrNothingType,
                "display_name": "Date Won",
                "short_desc": "The date the opportunity was won",
                "desc": "The date the opportunity was won in YYYY-MM-DD format",
                "example_value": "2026-03-15",
            },
            "date_lost": {
                "type": StringOrNothingType,
                "display_name": "Date Lost",
                "short_desc": "The date the opportunity was lost",
                "desc": "The date the opportunity was lost in YYYY-MM-DD format",
                "example_value": "2026-03-15",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create opportunity action data provider
public class CloseCreateOpportunityDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new opportunity in Close",
            "type": "CloseCreateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new opportunity in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateOpportunityRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseOpportunityDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("opportunity", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
