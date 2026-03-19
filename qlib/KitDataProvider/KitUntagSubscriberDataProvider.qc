# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitUntagSubscriberDataProvider.qc Kit untag subscriber action data provider

/*  KitUntagSubscriberDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for untagging a subscriber
public class KitUntagSubscriberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for untagging a subscriber
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag to remove from the subscriber",
                "required": True,
                "example_value": "12345",
                "ref_data": "tags",
            },
            "subscriber_id": {
                "type": StringType,
                "display_name": "Subscriber ID",
                "short_desc": "The subscriber ID",
                "desc": "The unique identifier of the subscriber to untag",
                "required": True,
                "example_value": "67890",
                "ref_data": "subscribers",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit untag subscriber action data provider
public class KitUntagSubscriberDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "untag-subscriber",
            "desc": "Remove a tag from a subscriber in Kit",
            "type": "KitUntagSubscriberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "untag-subscriber";
    }

    *string getDesc() {
        return "Remove a tag from a subscriber in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitUntagSubscriberRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string tag_id = remove req.tag_id;
        string subscriber_id = remove req.subscriber_id;
        return doDelete("tags/" + encode_url(tag_id, True) + "/subscribers/" + encode_url(subscriber_id, True));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
