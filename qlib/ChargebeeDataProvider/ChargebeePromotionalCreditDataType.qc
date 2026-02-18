# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeePromotionalCreditDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Chargebee promotional credit record type
/** Represents a promotional credit entry in Chargebee.
*/
public class ChargebeePromotionalCreditDataType inherits HashDataType {
    private {
        #! Field definitions for Chargebee promotional credits
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Promotional Credit ID",
                "short_desc": "Unique promotional credit identifier",
                "desc": "The unique identifier for the promotional credit entry in Chargebee",
                "required": True,
            },
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "Associated customer identifier",
                "desc": "The identifier of the customer this promotional credit belongs to",
                "required": True,
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Credit operation type",
                "desc": "The type of promotional credit operation",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "increment",
                        "display_name": "Increment",
                        "desc": "Credits were added to the customer balance",
                    },
                    <AllowedValueInfo>{
                        "value": "decrement",
                        "display_name": "Decrement",
                        "desc": "Credits were deducted from the customer balance",
                    },
                    <AllowedValueInfo>{
                        "value": "set",
                        "display_name": "Set",
                        "desc": "Customer credit balance was set to a specific amount",
                    },
                ),
            },
            "amount": {
                "type": IntOrNothingType,
                "display_name": "Amount",
                "short_desc": "Credit amount",
                "desc": "The amount of the promotional credit operation (in cents)",
            },
            "currency_code": {
                "type": StringOrNothingType,
                "display_name": "Currency Code",
                "short_desc": "Credit currency",
                "desc": "The ISO 4217 currency code for the promotional credit",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Credit description",
                "desc": "A description of the promotional credit operation",
            },
            "credit_type": {
                "type": StringOrNothingType,
                "display_name": "Credit Type",
                "short_desc": "Category of the credit",
                "desc": "The category of the promotional credit",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "loyalty_credits",
                        "display_name": "Loyalty Credits",
                        "desc": "Credits earned through a loyalty program",
                    },
                    <AllowedValueInfo>{
                        "value": "referral_rewards",
                        "display_name": "Referral Rewards",
                        "desc": "Credits earned through referrals",
                    },
                    <AllowedValueInfo>{
                        "value": "general",
                        "display_name": "General",
                        "desc": "General promotional credits",
                    },
                ),
            },
            "reference": {
                "type": StringOrNothingType,
                "display_name": "Reference",
                "short_desc": "External reference",
                "desc": "An external reference for the promotional credit operation",
            },
            "closing_balance": {
                "type": IntOrNothingType,
                "display_name": "Closing Balance",
                "short_desc": "Balance after operation",
                "desc": "The customer's promotional credit balance after this operation (in cents)",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Unix timestamp (in seconds) when the promotional credit entry was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ChargebeeDataProvider
