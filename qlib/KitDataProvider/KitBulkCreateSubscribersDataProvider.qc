# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitBulkCreateSubscribersDataProvider.qc Kit bulk create subscribers action data provider

/*  KitBulkCreateSubscribersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Request type for bulk creating subscribers
public class KitBulkCreateSubscribersRequestDataType inherits HashDataType {
    public {
        #! Field definitions for bulk creating subscribers
        const Fields = {
            "subscribers": {
                "type": new ListDataType("Subscribers", new KitBulkSubscriberDataType(), True),
                "display_name": "Subscribers",
                "short_desc": "List of subscribers to create",
                "desc": "A list of subscribers to create, each containing at least an `email_address` "
                    "field",
                "required": True,
            },
            "callback_url": {
                "type": StringOrNothingType,
                "display_name": "Callback URL",
                "short_desc": "URL to receive completion callback",
                "desc": "A URL that will receive a webhook when the bulk operation completes",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit bulk create subscribers action data provider
public class KitBulkCreateSubscribersDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-create",
            "desc": "Bulk create subscribers in Kit",
            "type": "KitBulkCreateSubscribersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "bulk-create";
    }

    *string getDesc() {
        return "Bulk create subscribers in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitBulkCreateSubscribersRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("bulk/subscribers", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
