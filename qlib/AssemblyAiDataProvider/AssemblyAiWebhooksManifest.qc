# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiWebhooksManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
#! Curated actions and resource selections for the webhooks API
public class AssemblyAiWebhooksManifest {
    public {
        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "webhooks": <RestSchemaReferenceDataInfo>{"name": "webhooks", "action": "list-webhooks", "records_path": ("subscriptions",), "value_field": "id", "display_field": "url", "desc": "Recent webhooks; values from earlier workflow steps are also accepted."},
        };

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "create-webhook",
                "method": "POST",
                "path": "/v1/webhook-subscriptions",
                "operation_id": "createWebhookSubscription",
                "display_name": "Create Webhook Subscription",
                "short_desc": "Subscribe a URL to voice agent events",
                "desc": "Subscribe a callback URL to selected events. Omit `agent_id` to receive account-wide events. Set a signing secret so the recipient can verify requests.",
                "groups": ("Webhooks",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{"display_name": "Voice Agent", "ref_data": "agents", "supports_custom_values": True},
                    "secret": <RestSchemaFieldOverlayInfo>{"sensitive": True, "display_name": "Signing Secret"},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-webhooks",
                "method": "GET",
                "path": "/v1/webhook-subscriptions",
                "operation_id": "listWebhookSubscriptions",
                "display_name": "List Webhook Subscriptions",
                "short_desc": "Browse voice agent event subscriptions",
                "desc": "List webhook subscriptions. Pass `response_metadata.next_cursor` as `cursor` to retrieve the next page.",
                "groups": ("Webhooks",),
            },
            <RestSchemaActionInfo>{
                "action": "get-webhook",
                "method": "GET",
                "path": "/v1/webhook-subscriptions/{subscription_id}",
                "operation_id": "getWebhookSubscription",
                "display_name": "Get Webhook Subscription",
                "short_desc": "Retrieve an event subscription",
                "desc": "Retrieve the URL, events, scope, and enabled state of a webhook subscription.",
                "groups": ("Webhooks",),
                "fields": {
                    "subscription_id": <RestSchemaFieldOverlayInfo>{"display_name": "Subscription", "ref_data": "webhooks", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-webhook",
                "method": "PATCH",
                "path": "/v1/webhook-subscriptions/{subscription_id}",
                "operation_id": "updateWebhookSubscription",
                "display_name": "Update Webhook Subscription",
                "short_desc": "Change an event subscription or rotate its signing secret",
                "desc": "Update selected subscription fields. Sending `secret` rotates the signing secret and increments its version; `enabled=false` disables delivery.",
                "groups": ("Webhooks",),
                "fields": {
                    "subscription_id": <RestSchemaFieldOverlayInfo>{"display_name": "Subscription", "ref_data": "webhooks", "supports_custom_values": True},
                    "secret": <RestSchemaFieldOverlayInfo>{"sensitive": True, "display_name": "Signing Secret"},
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-webhook",
                "method": "DELETE",
                "path": "/v1/webhook-subscriptions/{subscription_id}",
                "operation_id": "deleteWebhookSubscription",
                "display_name": "Delete Webhook Subscription",
                "short_desc": "Remove an event subscription",
                "desc": "Delete the selected webhook subscription and stop future delivery through it. A successful request returns no body.",
                "groups": ("Webhooks",),
                "fields": {
                    "subscription_id": <RestSchemaFieldOverlayInfo>{"display_name": "Subscription", "ref_data": "webhooks", "supports_custom_values": True},
                },
            },
        );
    }
}
}
