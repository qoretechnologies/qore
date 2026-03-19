# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitCreateBroadcastDataProvider.qc Kit create broadcast action data provider

/*  KitCreateBroadcastDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a broadcast
public class KitCreateBroadcastRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a broadcast
        const Fields = {
            "content": {
                "type": StringType,
                "display_name": "Content",
                "short_desc": "The broadcast HTML content",
                "desc": "The HTML content of the broadcast email",
                "required": True,
                "example_value": "<p>Hello {{subscriber.first_name}}</p>",
            },
            "subject": {
                "type": StringType,
                "display_name": "Subject",
                "short_desc": "The broadcast subject line",
                "desc": "The subject line of the broadcast email",
                "required": True,
                "example_value": "Weekly Newsletter",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The broadcast description",
                "desc": "A description of the broadcast for internal reference",
                "preselected": True,
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The sender email address",
                "desc": "The email address used as the sender for the broadcast",
            },
            "email_template_id": {
                "type": StringOrNothingType,
                "display_name": "Email Template ID",
                "short_desc": "The email template ID",
                "desc": "The identifier of the email template to use for the broadcast",
            },
            "public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether the broadcast is public",
                "desc": "Indicates whether the broadcast is publicly accessible",
            },
            "published_at": {
                "type": DateOrNothingType,
                "display_name": "Published At",
                "short_desc": "When to publish the broadcast",
                "desc": "The timestamp when the broadcast should be published",
            },
            "send_at": {
                "type": DateOrNothingType,
                "display_name": "Send At",
                "short_desc": "When to send the broadcast",
                "desc": "The timestamp when the broadcast should be sent; set to `null` for a draft",
                "preselected": True,
            },
            "thumbnail_alt": {
                "type": StringOrNothingType,
                "display_name": "Thumbnail Alt",
                "short_desc": "The thumbnail alt text",
                "desc": "The alternative text for the broadcast thumbnail image",
            },
            "thumbnail_url": {
                "type": StringOrNothingType,
                "display_name": "Thumbnail URL",
                "short_desc": "The thumbnail URL",
                "desc": "The URL of the broadcast thumbnail image",
            },
            "preview_text": {
                "type": StringOrNothingType,
                "display_name": "Preview Text",
                "short_desc": "The preview text",
                "desc": "The preview text shown in email clients before the email is opened",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit create broadcast action data provider
public class KitCreateBroadcastDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new broadcast in Kit",
            "type": "KitCreateBroadcastDataProvider",
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
        return "Create a new broadcast in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitCreateBroadcastRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitBroadcastDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPost("broadcasts", req);
        return response.broadcast;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
