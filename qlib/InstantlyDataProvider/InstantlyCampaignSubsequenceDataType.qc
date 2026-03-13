# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCampaignSubsequenceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly campaign subsequence data type
public class InstantlyCampaignSubsequenceDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Subsequence ID",
                "short_desc": "Subsequence UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the subsequence",
            },
            "campaign_id": {
                "display_name": "Campaign ID",
                "short_desc": "Parent campaign ID",
                "type": StringType,
                "desc": "The UUID of the parent campaign this subsequence belongs to",
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Subsequence name",
                "type": StringOrNothingType,
                "desc": "The name of the subsequence",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Subsequence status",
                "type": IntOrNothingType,
                "desc": "The status of the subsequence",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the subsequence was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
