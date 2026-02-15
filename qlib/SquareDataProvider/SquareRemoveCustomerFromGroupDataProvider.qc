# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareRemoveCustomerFromGroupDataProvider.qc Square remove customer from group action

/*  SquareRemoveCustomerFromGroupDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for removing a customer from a group
public class SquareRemoveCustomerFromGroupRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "customer_id": {
                "type": StringType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The unique identifier of the customer to remove from the group",
                "required": True,
            },
            "group_id": {
                "type": StringType,
                "display_name": "Group ID",
                "short_desc": "The group ID",
                "desc": "The unique identifier of the customer group",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square remove customer from group action data provider
public class SquareRemoveCustomerFromGroupDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove-from-group",
            "desc": "Remove a customer from a customer group in Square",
            "type": "SquareRemoveCustomerFromGroupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "remove-from-group";
    }

    *string getDesc() {
        return "Remove a customer from a customer group in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareRemoveCustomerFromGroupRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("customers/%s/groups/%s", req.customer_id, req.group_id));
        return {};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
