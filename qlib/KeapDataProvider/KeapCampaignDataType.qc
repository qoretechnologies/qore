# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapCampaignDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap campaign record type
public class KeapCampaignDataType inherits HashDataType {
    private {
        #! Field definitions for Keap campaigns
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The campaign ID",
                "desc": "The unique identifier of the campaign",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The campaign name",
                "desc": "The name of the campaign",
                "example_value": "Spring Promotion 2026",
            },
            "goals": {
                "type": new ListDataType(new KeapGoalDataType(AutoHashOrNothingType), True),
                "display_name": "Goals",
                "short_desc": "The campaign goals",
                "desc": "A list of goals defined for this campaign",
            },
            "published_status": {
                "type": BoolOrNothingType,
                "display_name": "Published Status",
                "short_desc": "Whether the campaign is published",
                "desc": "Indicates whether the campaign is published (`True` or `False`)",
            },
            "published_date": {
                "type": DateOrNothingType,
                "display_name": "Published Date",
                "short_desc": "When the campaign was published",
                "desc": "The timestamp when the campaign was published",
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the campaign was created",
                "desc": "The timestamp when the campaign was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the campaign was last updated",
                "desc": "The timestamp when the campaign was last updated",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The campaign time zone",
                "desc": "The time zone used for the campaign",
                "example_value": "US/Eastern",
            },
            "error_message": {
                "type": StringOrNothingType,
                "display_name": "Error Message",
                "short_desc": "The campaign error message",
                "desc": "An error message if the campaign encountered an issue",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
