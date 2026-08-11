# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! The data carried by a Paddle webhook event
/** Every Paddle event has the same envelope; what differs is the entity inside \c data.
*/
public class PaddleEventDataTypeBase inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "The unique ID of the event",
                "type": StringType,
                "desc": "The unique ID of the event (`evt_…`).\n\n"
                    "Paddle can deliver the same event more than once, so record this and ignore an ID "
                    "already processed; that is what makes handling **idempotent**.",
            },
            "notification_id": {
                "display_name": "Notification ID",
                "short_desc": "The unique ID of this delivery",
                "type": StringOrNothingType,
                "desc": "The unique ID of this **delivery** (`ntf_…`), as opposed to the event itself.\n\n"
                    "A redelivery of the same event carries the same `event_id` but a new "
                    "`notification_id`, which is how the two are told apart when investigating.",
            },
            "event_type": {
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "type": StringType,
                "desc": "The type of event, such as `transaction.completed` or `subscription.canceled`",
            },
            "occurred_at": {
                "display_name": "Occurred At",
                "short_desc": "When the event occurred",
                "type": StringOrNothingType,
                "desc": "When the event occurred, as an RFC 3339 timestamp.\n\n"
                    "This is when it happened in Paddle, not when it was delivered; a redelivery carries "
                    "the original time.",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "The entity the event is about",
                "type": AutoHashOrNothingType,
                "desc": "The entity the event is about - a customer, product, price, subscription, "
                    "transaction or report, in the same shape the corresponding action returns.\n\n"
                    "**Paddle sends the entity in full, not a diff**, so an update event does not say "
                    "which field changed; compare against what you hold if that matters.",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, NOTHING, NOTHING, NOTHING)
            .setAttributes({"display_name": $1.value.display_name, "short_desc": $1.value.short_desc})),
            Fields.pairIterator();
    }
}

#! The data carried by a Paddle webhook event
public const PaddleEventDataType = new PaddleEventDataTypeBase();
}
