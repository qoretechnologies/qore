# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloCreateDealDataProvider.qc Apollo create deal action data provider

/*  ApolloCreateDealDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a deal
public class ApolloCreateDealRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The deal name",
                "desc": "The name of the deal",
                "example_value": "Enterprise License",
                "required": True,
                "preselected": True,
            },
            "amount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "The deal amount",
                "desc": "The monetary amount of the deal",
                "preselected": True,
            },
            "closed_date": {
                "type": StringOrNothingType,
                "display_name": "Closed Date",
                "short_desc": "Expected close date (ISO 8601 format)",
                "desc": "The expected close date of the deal in ISO 8601 format",
            },
            "stage_name": {
                "type": StringOrNothingType,
                "display_name": "Stage Name",
                "short_desc": "The deal stage name",
                "desc": "The name of the current stage of the deal",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The deal status: `open`, `won`, or `lost`",
                "desc": "The status of the deal: `open`, `won`, or `lost`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open"},
                    <AllowedValueInfo>{"value": "won"},
                    <AllowedValueInfo>{"value": "lost"},
                ),
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The owner user ID",
                "desc": "The ID of the user who owns this deal",
            },
            "account_id": {
                "type": StringOrNothingType,
                "display_name": "Account ID",
                "short_desc": "The associated account ID",
                "desc": "The ID of the account associated with this deal",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo create deal action data provider
public class ApolloCreateDealDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new deal in Apollo",
            "type": "ApolloCreateDealDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloDealDataType();
        static *AbstractDataProviderType RequestType = new ApolloCreateDealRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new deal in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloCreateDealRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloDealDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> result = doPost("opportunities", {} + req);
        return result.opportunity ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
