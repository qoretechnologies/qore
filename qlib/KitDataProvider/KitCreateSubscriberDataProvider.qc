# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitCreateSubscriberDataProvider.qc Kit create subscriber action data provider

/*  KitCreateSubscriberDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a subscriber
public class KitCreateSubscriberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a subscriber
        const Fields = {
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The subscriber's email address",
                "desc": "The email address of the subscriber to create or update (upsert behavior)",
                "required": True,
                "example_value": "user@example.com",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The subscriber's first name",
                "desc": "The first name of the subscriber",
                "preselected": True,
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The subscriber's state",
                "desc": "The state of the subscriber (e.g., `active`, `inactive`)",
                "allowed_values": (<AllowedValueInfo>{"value": "active"}, <AllowedValueInfo>{"value": "inactive"}),
            },
            "fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A hash of custom field values to set on the subscriber",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit create subscriber action data provider
public class KitCreateSubscriberDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new subscriber in Kit (upsert behavior)",
            "type": "KitCreateSubscriberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new subscriber in Kit (upsert behavior)";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitCreateSubscriberRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitSubscriberDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPost("subscribers", req);
        return response.subscriber;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
