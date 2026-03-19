# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitUnsubscribeDataProvider.qc Kit unsubscribe action data provider

/*  KitUnsubscribeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for unsubscribing a subscriber
public class KitUnsubscribeRequestDataType inherits HashDataType {
    public {
        #! Field definitions for unsubscribing a subscriber
        const Fields = {
            "subscriber_id": {
                "type": StringType,
                "display_name": "Subscriber ID",
                "short_desc": "The subscriber ID to unsubscribe",
                "desc": "The unique identifier of the subscriber to unsubscribe",
                "required": True,
                "example_value": "12345",
                "ref_data": "subscribers",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit unsubscribe action data provider
public class KitUnsubscribeDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "unsubscribe",
            "desc": "Unsubscribe a subscriber in Kit",
            "type": "KitUnsubscribeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "unsubscribe";
    }

    *string getDesc() {
        return "Unsubscribe a subscriber in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitUnsubscribeRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitSubscriberDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.subscriber_id;
        hash<auto> response = doPost("subscribers/" + encode_url(id, True) + "/unsubscribe");
        return response.subscriber;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
