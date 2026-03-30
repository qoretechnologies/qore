# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly campaign record type
public class BitlyCampaignDataType inherits HashDataType {
    public {
        #! Field definitions for Bitly campaigns
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Campaign GUID",
                "short_desc": "The campaign identifier",
                "desc": "The unique identifier (GUID) of the campaign",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The campaign name",
                "desc": "The name of the campaign",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The campaign description",
                "desc": "A description of the campaign",
            },
            "group_guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "Associated group",
                "desc": "The GUID of the group this campaign belongs to",
            },
            "created": {
                "type": StringOrNothingType,
                "display_name": "Created",
                "short_desc": "When the campaign was created",
                "desc": "The ISO 8601 timestamp when the campaign was created",
            },
            "modified": {
                "type": StringOrNothingType,
                "display_name": "Modified",
                "short_desc": "When the campaign was last modified",
                "desc": "The ISO 8601 timestamp when the campaign was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
