# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasUpdateCustomerDataProvider.qc Gorgias update customer action data provider

/*  GorgiasUpdateCustomerDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Request type for updating a customer
public class GorgiasUpdateCustomerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "customer_id": {
                "type": IntType,
                "display_name": "Customer ID",
                "short_desc": "The customer to update",
                "desc": "The ID of the customer to update",
                "required": True,
                "ref_data": "customers",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Customer email address",
                "desc": "The primary email address of the customer",
                "preselected": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Full name",
                "desc": "The full name of the customer",
                "preselected": True,
            },
            "firstname": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Customer first name",
                "desc": "The first name of the customer",
            },
            "lastname": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Customer last name",
                "desc": "The last name of the customer",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Preferred language",
                "desc": "The preferred language of the customer (ISO 639-1 code)",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "Customer timezone",
                "desc": "The timezone of the customer",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the customer",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Internal note",
                "desc": "Internal notes about the customer",
            },
            "channels": {
                "type": new ListDataType("channels",
                    new GorgiasCustomerChannelDataType(AutoHashOrNothingType), True),
                "display_name": "Channels",
                "short_desc": "Communication channels",
                "desc": "List of communication channels for the customer. "
                    "Each channel has an `address`, `type`, and optional `preferred` flag",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias update customer action data provider
public class GorgiasUpdateCustomerDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing Gorgias customer",
            "type": "GorgiasUpdateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing Gorgias customer";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasUpdateCustomerRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasCustomerDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int customer_id = remove req.customer_id;
        return doPut("customers/" + customer_id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
