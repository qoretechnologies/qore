# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitTagSubscriberDataProvider.qc Kit tag subscriber action data provider

/*  KitTagSubscriberDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for tagging a subscriber
public class KitTagSubscriberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for tagging a subscriber
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag to apply to the subscriber",
                "required": True,
                "example_value": "12345",
                "ref_data": "tags",
            },
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The subscriber's email address",
                "desc": "The email address of the subscriber to tag",
                "required": True,
                "example_value": "user@example.com",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit tag subscriber action data provider
public class KitTagSubscriberDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "tag-subscriber",
            "desc": "Tag a subscriber in Kit",
            "type": "KitTagSubscriberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "tag-subscriber";
    }

    *string getDesc() {
        return "Tag a subscriber in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitTagSubscriberRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitSubscriberDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string tag_id = remove req.tag_id;
        hash<auto> response = doPost("tags/" + encode_url(tag_id, True) + "/subscribers", req);
        return response.subscriber;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
