# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Base event data type for all Lemlist webhook events
public class LemlistBaseEventDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The type of event that occurred",
                "desc": "The Lemlist event type (e.g., `emailsSent`, `emailsOpened`)",
            },
            "campaignId": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign where the event occurred",
                "desc": "The identifier of the campaign where this event occurred",
            },
            "campaignName": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "Name of the campaign",
                "desc": "The name of the campaign where this event occurred",
            },
            "leadId": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "Lead involved in the event",
                "desc": "The identifier of the lead involved in this event",
            },
            "leadEmail": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "Email address of the lead",
                "desc": "The email address of the lead involved in this event",
            },
            "sequenceStepId": {
                "type": StringOrNothingType,
                "display_name": "Sequence Step ID",
                "short_desc": "Sequence step that triggered the event",
                "desc": "The identifier of the sequence step that triggered this event",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Event timestamp",
                "desc": "ISO 8601 timestamp when the event occurred",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Email sent event data type
public class LemlistEmailSentEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Email opened event data type
public class LemlistEmailOpenedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Email clicked event data type
public class LemlistEmailClickedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Email replied event data type
public class LemlistEmailRepliedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Email bounced event data type
public class LemlistEmailBouncedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Lead interested event data type
public class LemlistLeadInterestedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

#! Lead not interested event data type
public class LemlistLeadNotInterestedEventDataType inherits LemlistBaseEventDataType {
    constructor() : LemlistBaseEventDataType() {
    }
}

} # namespace LemlistDataProvider
