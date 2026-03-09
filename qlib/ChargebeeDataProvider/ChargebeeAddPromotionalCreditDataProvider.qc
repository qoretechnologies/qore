# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeAddPromotionalCreditDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for adding promotional credits to a customer
public class ChargebeeAddPromotionalCreditRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer to add credits to",
                "desc": "The unique identifier of the customer to add promotional credits to",
                "required": True,
            },
            "amount": {
                "type": IntType,
                "display_name": "Amount",
                "short_desc": "Credit amount in cents",
                "desc": "The amount of promotional credits to add in the smallest currency unit "
                    "(e.g., cents)",
                "required": True,
            },
            "currency_code": {
                "type": StringType,
                "display_name": "Currency Code",
                "short_desc": "Currency for the credits",
                "desc": "The currency code for the promotional credits (e.g., `USD`, `EUR`)",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Description of the credits",
                "desc": "A description explaining why the promotional credits were added",
            },
            "credit_type": {
                "type": StringOrNothingType,
                "display_name": "Credit Type",
                "short_desc": "Type of promotional credit",
                "desc": "The type of promotional credit being added",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "loyalty_credits", "desc": "Loyalty credits"},
                    <AllowedValueInfo>{"value": "referral_rewards", "desc": "Referral rewards"},
                    <AllowedValueInfo>{"value": "general", "desc": "General promotional credits"},
                ),
            },
            "reference": {
                "type": StringOrNothingType,
                "display_name": "Reference",
                "short_desc": "Reference for the credit",
                "desc": "A reference identifier for the promotional credit transaction",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding promotional credits to a customer
public class ChargebeeAddPromotionalCreditDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-promotional-credit",
            "desc": "Add promotional credits to a customer in Chargebee",
            "type": "ChargebeeAddPromotionalCreditDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeAddPromotionalCreditRequestDataType RequestType();

        #! Response type
        static ChargebeeCustomerDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "add-promotional-credit";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add promotional credits to a customer in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string customer_id = req.customer_id;
        hash<auto> body = req - "customer_id";
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding promotional credits for customer %s", customer_id));

        hash<auto> response = doPost("/customers/" + customer_id + "/add_promotional_credits", body);
        return response.customer;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
