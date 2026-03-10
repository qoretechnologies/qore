# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloUpdateDealDataProvider.qc Apollo update deal action data provider

/*  ApolloUpdateDealDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a deal
public class ApolloUpdateDealRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "deal_id": {
                "type": StringType,
                "display_name": "Deal ID",
                "short_desc": "The deal ID to update",
                "desc": "The unique identifier of the deal to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The new deal name",
                "desc": "The new name of the deal",
            },
            "amount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "The new deal amount",
                "desc": "The new monetary amount of the deal",
            },
            "closed_date": {
                "type": StringOrNothingType,
                "display_name": "Closed Date",
                "short_desc": "The new expected close date",
                "desc": "The new expected close date of the deal",
            },
            "stage_name": {
                "type": StringOrNothingType,
                "display_name": "Stage Name",
                "short_desc": "The new stage name",
                "desc": "The new stage name of the deal",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The deal status",
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
                "short_desc": "The new owner user ID",
                "desc": "The new owner user ID for the deal",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo update deal action data provider
public class ApolloUpdateDealDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing deal in Apollo",
            "type": "ApolloUpdateDealDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloDealDataType();
        static *AbstractDataProviderType RequestType = new ApolloUpdateDealRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing deal in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloUpdateDealRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloDealDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        string id = remove body.deal_id;
        hash<auto> result = doPatch("opportunities/" + id, body);
        return result.opportunity ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
