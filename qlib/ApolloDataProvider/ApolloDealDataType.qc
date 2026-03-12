# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloDealDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo deal (opportunity) record type
public class ApolloDealDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo deals
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "Deal ID",
                "desc": "The unique identifier of the deal",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Deal name",
                "desc": "The name of the deal",
                "example_value": "Enterprise License",
            },
            "amount": {
                "type": FloatOrNothingType,
                "display_name": "Amount",
                "short_desc": "Deal amount",
                "desc": "The monetary amount of the deal",
            },
            "closed_date": {
                "type": DateOrNothingType,
                "display_name": "Closed Date",
                "short_desc": "Expected close date",
                "desc": "The expected close date of the deal",
            },
            "stage_name": {
                "type": StringOrNothingType,
                "display_name": "Stage Name",
                "short_desc": "Pipeline stage name",
                "desc": "The name of the pipeline stage for this deal",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Deal status (open/won/lost)",
                "desc": "The current status of the deal",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "desc": "Open deal"},
                    <AllowedValueInfo>{"value": "won", "desc": "Won deal"},
                    <AllowedValueInfo>{"value": "lost", "desc": "Lost deal"},
                ),
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Owner user ID",
                "desc": "The unique identifier of the user who owns this deal",
            },
            "account_id": {
                "type": StringOrNothingType,
                "display_name": "Account ID",
                "short_desc": "Account ID",
                "desc": "The unique identifier of the associated account",
            },
            "contact_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Contact IDs",
                "short_desc": "Associated contact IDs",
                "desc": "The list of contact IDs associated with this deal",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Created timestamp",
                "desc": "The timestamp when the deal was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Updated timestamp",
                "desc": "The timestamp when the deal was last updated",
            },
            "typed_custom_fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom fields",
                "desc": "Custom fields associated with the deal",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
