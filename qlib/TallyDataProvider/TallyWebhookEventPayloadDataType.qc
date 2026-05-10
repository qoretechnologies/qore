# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyWebhookEventPayloadDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! A single choice option entry inside a form response webhook payload field
public class TallyWebhookEventPayloadFieldOptionDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Option ID",
                "short_desc": "Choice option identifier",
                "desc": "The unique identifier of the choice option",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Option Text",
                "short_desc": "Choice option label",
                "desc": "The display text of the choice option",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! A single field in a form response webhook payload
public class TallyWebhookEventPayloadFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "key": {
                "display_name": "Key",
                "short_desc": "Field key",
                "desc": "The unique key identifying the field within the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "label": {
                "display_name": "Label",
                "short_desc": "Field label",
                "desc": "The human-readable label of the field as shown to respondents",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Block/field type",
                "desc": "The Tally block type for this field (for example `INPUT_TEXT`, "
                    "`MULTIPLE_CHOICE`, `FILE_UPLOAD`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Submitted value",
                "desc": "The value submitted for this field; the runtime type depends on the field "
                    "`type` (string, number, boolean, list, or hash)",
                "type": AbstractDataProviderType::get(AbstractDataProviderType::anyType),
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Available choice options",
                "desc": "List of available options for choice-style fields, each with an `id` and "
                    "`text`",
                "type": new ListDataType("TallyWebhookEventPayloadFieldOptionList",
                    new TallyWebhookEventPayloadFieldOptionDataType(AutoHashOrNothingType), True),
            },
        };
    }

    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! The `data` object inside a FORM_RESPONSE webhook payload
public class TallyWebhookEventPayloadFormResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "responseId": {
                "display_name": "Response ID",
                "short_desc": "Response identifier",
                "desc": "The unique identifier of the response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "submissionId": {
                "display_name": "Submission ID",
                "short_desc": "Submission identifier",
                "desc": "The unique identifier of the submission",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "respondentId": {
                "display_name": "Respondent ID",
                "short_desc": "Respondent identifier",
                "desc": "The unique identifier of the respondent",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "formId": {
                "display_name": "Form ID",
                "short_desc": "Form identifier",
                "desc": "The ID of the form the response was submitted to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "formName": {
                "display_name": "Form Name",
                "short_desc": "Form name",
                "desc": "The display name of the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Submission timestamp",
                "desc": "Timestamp when the submission was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "fields": {
                "display_name": "Fields",
                "short_desc": "Submitted fields",
                "desc": "List of submitted fields with their `key`, `label`, `type`, `value`, and "
                    "(for choice fields) `options`",
                "type": new ListDataType("TallyWebhookEventPayloadFieldList",
                    new TallyWebhookEventPayloadFieldDataType(AutoHashOrNothingType), True),
            },
        };
    }

    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! The full webhook event payload delivered to subscribers
public class TallyWebhookEventPayloadDataType inherits HashDataType {
    public {
        const Fields = {
            "eventId": {
                "display_name": "Event ID",
                "short_desc": "Webhook event identifier",
                "desc": "The unique identifier of the webhook event",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "eventType": {
                "display_name": "Event Type",
                "short_desc": "Event type",
                "desc": "The webhook event type; the only valid value is `FORM_RESPONSE`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FORM_RESPONSE", "display_name": "Form Response"},
                ),
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Event timestamp",
                "desc": "Timestamp when the webhook event was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "Form response data",
                "desc": "The structured form response data including respondent info and submitted "
                    "fields",
                "type": new TallyWebhookEventPayloadFormResponseDataType(AutoHashOrNothingType),
            },
        };
    }

    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyWebhookEventPayloadDataType = new TallyWebhookEventPayloadDataType();
}
