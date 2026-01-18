# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearIssueEventDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Event ID for new issues
public const EVENT_ISSUE_CREATED = "issue-created";

#! Event ID for updated issues
public const EVENT_ISSUE_UPDATED = "issue-updated";

#! Linear issue event data provider - watches for new and updated issues via webhooks
public class LinearIssueEventDataProvider inherits LinearDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "LinearIssueEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = LinearDataProviderBase::BaseConstructorOptions + {
            "teamId": <DataProviderOptionInfo>{
                "display_name": "Team",
                "short_desc": "Filter events to a specific team",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only watch for events from this team (optional)",
            },
            "event_type": <DataProviderOptionInfo>{
                "display_name": "Event Type",
                "short_desc": "Which events to watch for",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of events to watch for: 'issue-created' or 'issue-updated'",
                "default_value": EVENT_ISSUE_CREATED,
                "allowed_values": (
                    <AllowedValueInfo>{"value": EVENT_ISSUE_CREATED, "display_name": "Issue Created",
                        "desc": "Watch for newly created issues"},
                    <AllowedValueInfo>{"value": EVENT_ISSUE_UPDATED, "display_name": "Issue Updated",
                        "desc": "Watch for updated issues"},
                ),
            },
        };
    }

    private {
        #! Team ID filter
        *string teamId;

        #! Event type to watch for
        string event_type = EVENT_ISSUE_CREATED;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Linear webhook ID for cleanup
        *string linear_webhook_id;

        #! Issue data type
        static LinearIssueWebhookEventDataType issueEventDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : LinearDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.teamId) {
            teamId = copts.teamId;
        }
        if (copts.event_type) {
            event_type = copts.event_type;
        }
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest, *hash<auto> options) : LinearDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.teamId) {
            teamId = copts.teamId;
        }
        if (copts.event_type) {
            event_type = copts.event_type;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return event_type;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Linear %s event data provider", event_type == EVENT_ISSUE_CREATED ? "issue created" :
            "issue updated");
    }

    #! Called when all observers have been added to the object
    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Determine resource types based on event type
        list<string> resource_types;
        if (event_type == EVENT_ISSUE_CREATED) {
            resource_types = ("Issue",);
        } else {
            # EVENT_ISSUE_UPDATED
            resource_types = ("Issue",);
        }

        # Register webhook with Linear
        hash<auto> input = {
            "url": webhook_info.url,
            "resourceTypes": resource_types,
            "label": sprintf("Qorus %s webhook", event_type),
        };
        if (teamId) {
            input.teamId = teamId;
        } else {
            # Linear requires either teamId or allPublicTeams
            input.allPublicTeams = True;
        }

        try {
            hash<auto> result = doGraphQL(CreateWebhookMutation, {"input": input});
            if (!result.webhookCreate.success) {
                throw "LINEAR-WEBHOOK-ERROR", "Failed to create webhook with Linear";
            }
            linear_webhook_id = result.webhookCreate.webhook.id;
            log(LoggerLevel::INFO, "LinearIssueEventDataProvider: registered webhook %y with Linear (ID: %s)",
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
            event_type: <DataProviderMessageInfo>{
                "desc": event_type == EVENT_ISSUE_CREATED
                    ? "A new issue has been created in Linear"
                    : "An issue has been updated in Linear",
                "type": issueEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> example = {
            "action": event_type == EVENT_ISSUE_CREATED ? LinearActionCreate : LinearActionUpdate,
            "type": LinearEventTypeIssue,
            "createdAt": "2025-01-24T14:32:18.084Z",
            "url": "https://linear.app/company/issue/ENG-123",
            "organizationId": "dc844923-f9a4-40a3-825c-dea7747e57d6",
            "webhookTimestamp": 1706107938084,
            "webhookId": "000042e3-d123-4980-b49f-8e140eef9329",
            "actor": {
                "id": "b5ea5f1f-8adc-4f52-b4bd-ab4e84cf51ba",
                "type": "user",
                "name": "Sarah Chen",
                "email": "sarah@company.com",
                "url": "https://linear.app/company/profiles/sarah",
            },
            "data": generateExampleData(new LinearIssueDataType().getFields()),
        };
        # Add updatedFrom for update events
        if (event_type == EVENT_ISSUE_UPDATED) {
            example.updatedFrom = {
                "stateId": "e3d2c1b0-a987-6f5e-4d3c-2b1a09876543",
                "assigneeId": "f1e2d3c4-b5a6-9870-1234-567890fedcba",
                "updatedAt": "2025-01-24T14:32:18.076Z",
            };
        }
        return example;
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
                log(LoggerLevel::INFO, "LinearIssueEventDataProvider: deleted Linear webhook %s", linear_webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "LinearIssueEventDataProvider: error deleting Linear webhook: %s",
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
        # Check if this is the event type we're watching for
        string action = payload.action;
        string type = payload.type;

        # Filter events
        if (type != "Issue") {
            return;
        }

        # Check action matches our event type
        if (event_type == EVENT_ISSUE_CREATED && action != LinearActionCreate) {
            return;
        }
        if (event_type == EVENT_ISSUE_UPDATED && action != LinearActionUpdate) {
            return;
        }

        # Filter by team if specified
        if (teamId && payload.data.team.id != teamId) {
            return;
        }

        log(LoggerLevel::DEBUG, "handleWebhookEvent: %s for issue %s", action, payload.data.identifier);
        notifyObservers(event_type, payload);
    }
}

} # namespace LinearDataProvider
