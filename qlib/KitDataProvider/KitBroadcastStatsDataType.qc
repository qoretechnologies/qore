# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitBroadcastStatsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Kit broadcast stats record type
public class KitBroadcastStatsDataType inherits HashDataType {
    public {
        #! Field definitions for Kit broadcast stats
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The broadcast ID",
                "desc": "The unique identifier of the broadcast",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "The broadcast subject",
                "desc": "The subject line of the broadcast email",
            },
            "recipients": {
                "type": SoftIntOrNothingType,
                "display_name": "Recipients",
                "short_desc": "The number of recipients",
                "desc": "The total number of recipients the broadcast was sent to",
            },
            "open_rate": {
                "type": FloatOrNothingType,
                "display_name": "Open Rate",
                "short_desc": "The open rate",
                "desc": "The percentage of recipients who opened the broadcast",
            },
            "click_rate": {
                "type": FloatOrNothingType,
                "display_name": "Click Rate",
                "short_desc": "The click rate",
                "desc": "The percentage of recipients who clicked a link in the broadcast",
            },
            "unsubscribe_count": {
                "type": SoftIntOrNothingType,
                "display_name": "Unsubscribe Count",
                "short_desc": "The number of unsubscribes",
                "desc": "The number of recipients who unsubscribed after receiving the broadcast",
            },
            "total_clicks": {
                "type": SoftIntOrNothingType,
                "display_name": "Total Clicks",
                "short_desc": "The total number of clicks",
                "desc": "The total number of link clicks across all recipients",
            },
            "show_total_clicks": {
                "type": SoftBoolOrNothingType,
                "display_name": "Show Total Clicks",
                "short_desc": "Whether to show total clicks",
                "desc": "Indicates whether total click count is displayed",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The broadcast status",
                "desc": "The current status of the broadcast",
            },
            "progress": {
                "type": FloatOrNothingType,
                "display_name": "Progress",
                "short_desc": "The send progress",
                "desc": "The progress of the broadcast send operation as a percentage",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
