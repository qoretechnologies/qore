# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCampaignSequenceDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly campaign email variant data type
public class InstantlyCampaignVariantDataType inherits HashDataType {
    public {
        const Fields = {
            "subject": {
                "type": StringType,
                "display_name": "Subject",
                "short_desc": "Email subject line",
                "desc": "The email subject line; supports template variables like `{{firstName}}`",
                "required": True,
                "example_value": "Hello {{firstName}}",
            },
            "body": {
                "type": StringType,
                "display_name": "Body",
                "short_desc": "Email body content",
                "desc": "The email body content (HTML or plain text); supports template variables "
                    "like `{{firstName}}`",
                "required": True,
                "example_value": "<p>Hi {{firstName}}, I wanted to reach out...</p>",
            },
            "v_disabled": {
                "type": SoftBoolOrNothingType,
                "display_name": "Variant Disabled",
                "short_desc": "Whether this variant is disabled",
                "desc": "If `True`, this variant will not be used for sending; by default all "
                    "variants are enabled",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Instantly campaign sequence step data type
public class InstantlyCampaignStepDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "Step type",
                "desc": "The type of step; currently only `email` is supported",
                "required": True,
                "example_value": "email",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "email", "display_name": "Email"},
                ),
            },
            "delay": {
                "type": IntType,
                "display_name": "Delay",
                "short_desc": "Delay before next email",
                "desc": "The delay value before sending the next email; the unit is determined by "
                    "the `delay_unit` field (defaults to days)",
                "required": True,
                "example_value": 1,
            },
            "delay_unit": {
                "type": StringOrNothingType,
                "display_name": "Delay Unit",
                "short_desc": "Time unit for delay",
                "desc": "The time unit for the `delay` value; defaults to `days`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "minutes", "display_name": "Minutes"},
                    <AllowedValueInfo>{"value": "hours", "display_name": "Hours"},
                    <AllowedValueInfo>{"value": "days", "display_name": "Days"},
                ),
            },
            "variants": {
                "type": new ListDataType("CampaignVariantList",
                    new InstantlyCampaignVariantDataType(), True),
                "display_name": "Variants",
                "short_desc": "Email content variants",
                "desc": "A list of email content variants for A/B testing; at least one variant "
                    "is required",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Instantly campaign sequence data type
public class InstantlyCampaignSequenceDataType inherits HashDataType {
    public {
        const Fields = {
            "steps": {
                "type": new ListDataType("CampaignStepList",
                    new InstantlyCampaignStepDataType(), True),
                "display_name": "Steps",
                "short_desc": "Email sequence steps",
                "desc": "A list of email sequence steps; each step defines the delay and email "
                    "variants to send",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
