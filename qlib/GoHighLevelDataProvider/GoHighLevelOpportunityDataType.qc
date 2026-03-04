# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoHighLevelDataProvider module definition

/*  GoHighLevelOpportunityDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! GoHighLevel opportunity record type
public class GoHighLevelOpportunityDataType inherits HashDataType {
    public {
        #! Field definitions for GoHighLevel opportunities
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Opportunity ID",
                "short_desc": "Unique identifier of the opportunity",
                "desc": "Unique identifier of the opportunity",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The opportunity name",
                "desc": "The opportunity name",
            },
            "monetaryValue": {
                "type": SoftFloatOrNothingType,
                "display_name": "Monetary Value",
                "short_desc": "The monetary value of the opportunity",
                "desc": "The monetary value of the opportunity",
            },
            "pipelineId": {
                "type": StringOrNothingType,
                "display_name": "Pipeline ID",
                "short_desc": "The pipeline this opportunity belongs to",
                "desc": "The pipeline this opportunity belongs to",
            },
            "pipelineStageId": {
                "type": StringOrNothingType,
                "display_name": "Pipeline Stage ID",
                "short_desc": "The current stage in the pipeline",
                "desc": "The current stage in the pipeline",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID the opportunity is assigned to",
                "desc": "User ID the opportunity is assigned to",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The opportunity status",
                "desc": "The opportunity status (`open`, `won`, `lost`, or `abandoned`)",
            },
            "source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "The source of the opportunity",
                "desc": "The source of the opportunity",
            },
            "contactId": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact associated with this opportunity",
                "desc": "The contact associated with this opportunity",
            },
            "locationId": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location/sub-account ID",
                "desc": "The location/sub-account ID",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Timestamp when the opportunity was created",
                "desc": "Timestamp when the opportunity was created",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Timestamp when the opportunity was last updated",
                "desc": "Timestamp when the opportunity was last updated",
            },
            "lastStatusChangeAt": {
                "type": StringOrNothingType,
                "display_name": "Last Status Change",
                "short_desc": "Timestamp of the last status change",
                "desc": "Timestamp of the last status change",
            },
            "lastStageChangeAt": {
                "type": StringOrNothingType,
                "display_name": "Last Stage Change",
                "short_desc": "Timestamp of the last stage change",
                "desc": "Timestamp of the last stage change",
            },
            "customFields": {
                "type": new SoftListDataType(new GoHighLevelCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values for the opportunity",
                "desc": "Custom field values for the opportunity; each entry has an `id` "
                    "(custom field ID) and `field_value`",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace GoHighLevelDataProvider
