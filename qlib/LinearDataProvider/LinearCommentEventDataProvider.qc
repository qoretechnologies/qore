# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCommentEventDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Event ID for new comments
public const EVENT_COMMENT_CREATED = "comment-created";

#! Linear comment event data provider - watches for new comments via webhooks
public class LinearCommentEventDataProvider inherits LinearDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearCommentEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = LinearDataProviderBase::BaseConstructorOptions;
    }

    private {
        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Linear webhook ID for cleanup
        *string linear_webhook_id;

        #! Comment event data type
        static LinearCommentWebhookEventDataType commentEventDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : LinearDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest, *hash<auto> options) : LinearDataProviderBase(rest) {
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_COMMENT_CREATED;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Linear comment created event data provider";
    }

    #! Called when all observers have been added to the object
    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with Linear for Comment events
        # Linear requires either teamId or allPublicTeams
        hash<auto> input = {
            "url": webhook_info.url,
            "resourceTypes": ("Comment",),
            "label": "Qorus comment-created webhook",
            "allPublicTeams": True,
        };

        try {
            hash<auto> result = doGraphQL(CreateWebhookMutation, {"input": input});
            if (!result.webhookCreate.success) {
                throw "LINEAR-WEBHOOK-ERROR", "Failed to create webhook with Linear";
            }
            linear_webhook_id = result.webhookCreate.webhook.id;
            log(LoggerLevel::INFO, "LinearCommentEventDataProvider: registered webhook %y with Linear (ID: %s)",
                webhook_info.url, linear_webhook_id);
        } catch (hash<ExceptionInfo> ex) {
            # Clean up local webhook on failure
            if (webhook_info) {
                DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
                remove webhook_info;
            }
            rethrow;
        }
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_COMMENT_CREATED: <DataProviderMessageInfo>{
                "desc": "A new comment has been created on an issue in Linear",
                "type": commentEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        return {
            "action": LinearActionCreate,
            "type": LinearEventTypeComment,
            "createdAt": "2025-01-24T17:20:45.156Z",
            "url": "https://linear.app/company/issue/ENG-123#comment-abc123",
            "organizationId": "dc844923-f9a4-40a3-825c-dea7747e57d6",
            "webhookTimestamp": 1706118045156,
            "webhookId": "222264g5-f345-6102-d61h-0g362ggh1551",
            "actor": {
                "id": "d7gc8f1h-0cef-6h74-cd6g-hg6f06eh73dc",
                "type": "user",
                "name": "Emma Davis",
                "email": "emma@company.com",
                "url": "https://linear.app/company/profiles/emma",
            },
            "data": generateExampleData(new LinearCommentDataType().getFields()),
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from Linear first
        if (linear_webhook_id) {
            try {
                doGraphQL(DeleteWebhookMutation, {"id": linear_webhook_id});
                log(LoggerLevel::INFO, "LinearCommentEventDataProvider: deleted Linear webhook %s", linear_webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "LinearCommentEventDataProvider: error deleting Linear webhook: %s",
                    get_exception_string(ex));
            }
            remove linear_webhook_id;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from Linear
    private handleWebhookEvent(hash<auto> payload) {
        # Check if this is a comment event
        string action = payload.action;
        string type = payload.type;

        # Filter events - only handle Comment creates
        if (type != "Comment") {
            return;
        }
        if (action != LinearActionCreate) {
            return;
        }

        log(LoggerLevel::DEBUG, "handleWebhookEvent: comment created on issue %s", payload.data.issue.identifier);
        notifyObservers(EVENT_COMMENT_CREATED, payload);
    }
}

} # namespace LinearDataProvider
