# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io campaign record type
/** Represents a campaign in Customer.io.
*/
public class CustomerIoCampaignDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io campaigns
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "Campaign ID",
                "desc": "The unique identifier for the campaign",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Campaign name",
                "desc": "The name of the campaign",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Campaign type",
                "desc": "The type of the campaign",
            },
            "created": {
                "type": IntOrNothingType,
                "display_name": "Created",
                "short_desc": "Created timestamp",
                "desc": "Unix timestamp when the campaign was created",
            },
            "updated": {
                "type": IntOrNothingType,
                "display_name": "Updated",
                "short_desc": "Updated timestamp",
                "desc": "Unix timestamp when the campaign was last updated",
            },
            "active": {
                "type": BoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Whether active",
                "desc": "Whether the campaign is currently active",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State",
                "desc": "The current state of the campaign",
            },
            "tags": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags",
                "desc": "Tags associated with the campaign",
            },
            "actions": {
                "type": AutoListOrNothingType,
                "display_name": "Actions",
                "short_desc": "Campaign actions",
                "desc": "The actions configured for the campaign",
            },
            "msg_templates": {
                "type": AutoListOrNothingType,
                "display_name": "Message Templates",
                "short_desc": "Message templates",
                "desc": "The message templates used by the campaign",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
