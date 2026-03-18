# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceUpdateWebhookDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Request type for updating a webhook
public class WooCommerceUpdateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Webhook ID",
                "type": IntType,
                "desc": "The unique identifier for the webhook to update",
                "required": True,
                "example_value": 142,
            },
            "topic": {
                "display_name": "Topic",
                "type": StringOrNothingType,
                "desc": "The webhook topic",
                "example_value": "order.updated",
            },
            "delivery_url": {
                "display_name": "Delivery URL",
                "type": StringOrNothingType,
                "desc": "The URL where the webhook payload is delivered",
                "example_value": "https://example.com/webhooks/woocommerce",
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "A friendly name for the webhook",
                "example_value": "Order Updated Webhook",
            },
            "secret": {
                "display_name": "Secret",
                "type": StringOrNothingType,
                "desc": "Secret key for webhook verification",
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Webhook status: `active`, `paused`, `disabled`",
                "allowed_values": WebhookStatusAllowedValues,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a WooCommerce webhook
public class WooCommerceUpdateWebhookDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Updates an existing WooCommerce webhook",
            "type": "WooCommerceUpdateWebhookDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceUpdateWebhookRequestDataType RequestType();

        #! Response type
        static WooCommerceWebhookDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing WooCommerce webhook";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = remove req.id;
        return doPut(sprintf("webhooks/%d", id), {} + req);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
