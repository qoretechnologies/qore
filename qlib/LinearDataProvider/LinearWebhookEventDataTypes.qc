# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearWebhookEventDataTypes.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear webhook event type constants
public const LinearEventTypeIssue = "Issue";
public const LinearEventTypeComment = "Comment";
public const LinearEventTypeProject = "Project";
public const LinearEventTypeCycle = "Cycle";

#! Linear webhook action constants
public const LinearActionCreate = "create";
public const LinearActionUpdate = "update";
public const LinearActionRemove = "remove";

#! Actor data type for webhook events
public class LinearActorDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The actor's unique identifier",
                "desc": "The unique identifier of the user, integration, or OAuth client that triggered the event",
                "example_value": "b5ea5f1f-8adc-4f52-b4bd-ab4e84cf51ba",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The actor's name",
                "desc": "The name of the actor",
                "example_value": "Sarah Chen",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The actor type",
                "desc": "The type of actor: user, OauthClient, Integration, or apiKey",
                "example_value": "user",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The actor's email address",
                "desc": "The email address of the actor (only present for user actors)",
                "example_value": "sarah@company.com",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The actor's profile URL",
                "desc": "URL to the actor's profile in Linear",
                "example_value": "https://linear.app/company/profiles/sarah",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Base webhook event data type with common fields
public class LinearWebhookEventBaseDataType inherits HashDataType {
    private {
        const Fields = {
            "action": {
                "type": StringType,
                "display_name": "Action",
                "short_desc": "The action that triggered the webhook",
                "desc": "The action type: create, update, or remove",
                "example_value": "create",
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The type of entity",
                "desc": "The entity type that triggered the event (e.g., Issue, Comment, Project)",
                "example_value": "Issue",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the action occurred",
                "desc": "The timestamp when the action occurred (ISO 8601 format)",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "URL to the entity",
                "desc": "The URL to the entity in Linear",
                "example_value": "https://linear.app/company/issue/ENG-123",
            },
            "organizationId": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization's unique identifier",
                "desc": "The unique identifier of the Linear organization where the event occurred",
                "example_value": "dc844923-f9a4-40a3-825c-dea7747e57d6",
            },
            "webhookTimestamp": {
                "type": IntOrNothingType,
                "display_name": "Webhook Timestamp",
                "short_desc": "UNIX timestamp when webhook was sent",
                "desc": "UNIX timestamp in milliseconds when the webhook was sent",
                "example_value": 1706107938084,
            },
            "webhookId": {
                "type": StringOrNothingType,
                "display_name": "Webhook ID",
                "short_desc": "The webhook event ID",
                "desc": "Unique identifier for this webhook event",
                "example_value": "000042e3-d123-4980-b49f-8e140eef9329",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField({
            "name": "actor",
            "display_name": "Actor",
            "short_desc": "The actor who triggered the event",
            "desc": "Information about the user, integration, or OAuth client that triggered this event",
            "type": new LinearActorDataType(),
        }));
    }
}

#! Issue webhook event data type
public class LinearIssueWebhookEventDataType inherits LinearWebhookEventBaseDataType {
    constructor() : LinearWebhookEventBaseDataType() {
        addField(new QoreDataField({
            "name": "data",
            "display_name": "Issue Data",
            "short_desc": "The issue data",
            "desc": "The complete issue data from the webhook event",
            "type": new LinearIssueDataType(),
        }));
        addField(new QoreDataField({
            "name": "updatedFrom",
            "display_name": "Updated From",
            "short_desc": "Previous values before update",
            "desc": "A hash containing the previous values of fields that were changed (only present for update actions)",
            "type": AutoHashOrNothingType,
        }));
    }
}

#! Comment webhook event data type
public class LinearCommentWebhookEventDataType inherits LinearWebhookEventBaseDataType {
    constructor() : LinearWebhookEventBaseDataType() {
        addField(new QoreDataField({
            "name": "data",
            "display_name": "Comment Data",
            "short_desc": "The comment data",
            "desc": "The complete comment data from the webhook event",
            "type": new LinearCommentDataType(),
        }));
        addField(new QoreDataField({
            "name": "updatedFrom",
            "display_name": "Updated From",
            "short_desc": "Previous values before update",
            "desc": "A hash containing the previous values of fields that were changed (only present for update actions)",
            "type": AutoHashOrNothingType,
        }));
    }
}

#! Project webhook event data type
public class LinearProjectWebhookEventDataType inherits LinearWebhookEventBaseDataType {
    constructor() : LinearWebhookEventBaseDataType() {
        addField(new QoreDataField({
            "name": "data",
            "display_name": "Project Data",
            "short_desc": "The project data",
            "desc": "The complete project data from the webhook event",
            "type": new LinearProjectDataType(),
        }));
        addField(new QoreDataField({
            "name": "updatedFrom",
            "display_name": "Updated From",
            "short_desc": "Previous values before update",
            "desc": "A hash containing the previous values of fields that were changed (only present for update actions)",
            "type": AutoHashOrNothingType,
        }));
    }
}

#! Cycle webhook event data type
public class LinearCycleWebhookEventDataType inherits LinearWebhookEventBaseDataType {
    constructor() : LinearWebhookEventBaseDataType() {
        addField(new QoreDataField({
            "name": "data",
            "display_name": "Cycle Data",
            "short_desc": "The cycle data",
            "desc": "The complete cycle data from the webhook event",
            "type": new LinearCycleDataType(),
        }));
        addField(new QoreDataField({
            "name": "updatedFrom",
            "display_name": "Updated From",
            "short_desc": "Previous values before update",
            "desc": "A hash containing the previous values of fields that were changed (only present for update actions)",
            "type": AutoHashOrNothingType,
        }));
    }
}

} # namespace LinearDataProvider
