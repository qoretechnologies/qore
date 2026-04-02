# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesAppointmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales appointment record type
public class FreshsalesAppointmentDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales appointments
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The appointment ID",
                "desc": "The unique identifier of the appointment",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Appointment title",
                "desc": "The title of the appointment",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Appointment description",
                "desc": "A detailed description of the appointment",
            },
            "from_date": {
                "type": StringOrNothingType,
                "display_name": "Start Date",
                "short_desc": "Start date and time",
                "desc": "The start date and time of the appointment",
            },
            "end_date": {
                "type": StringOrNothingType,
                "display_name": "End Date",
                "short_desc": "End date and time",
                "desc": "The end date and time of the appointment",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "Time zone",
                "desc": "The time zone for the appointment",
            },
            "location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Meeting location",
                "desc": "The location of the appointment",
            },
            "creater_id": {
                "type": IntOrNothingType,
                "display_name": "Creator ID",
                "short_desc": "Creator user ID",
                "desc": "The ID of the user who created the appointment",
            },
            "targetable_id": {
                "type": IntOrNothingType,
                "display_name": "Target ID",
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal associated with this appointment",
            },
            "targetable_type": {
                "type": StringOrNothingType,
                "display_name": "Target Type",
                "short_desc": "Associated record type",
                "desc": "The type of record associated with this appointment",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the appointment was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the appointment was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales appointment request data type for create operations
public class FreshsalesCreateAppointmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an appointment
        const Fields = {
            "title": {
                "type": StringType,
                "display_name": "Title",
                "required": True,
                "short_desc": "Appointment title",
                "desc": "The title of the appointment",
                "example_value": "Client Meeting",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Appointment description",
                "desc": "A detailed description of the appointment",
                "preselected": True,
            },
            "from_date": {
                "type": StringType,
                "display_name": "Start Date",
                "required": True,
                "short_desc": "Start date and time",
                "desc": "The start date and time (format: `YYYY-MM-DDTHH:MM:SSZ`)",
                "example_value": "2026-04-15T10:00:00Z",
            },
            "end_date": {
                "type": StringType,
                "display_name": "End Date",
                "required": True,
                "short_desc": "End date and time",
                "desc": "The end date and time (format: `YYYY-MM-DDTHH:MM:SSZ`)",
                "example_value": "2026-04-15T11:00:00Z",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "Time zone",
                "desc": "The time zone for the appointment (e.g., `America/New_York`)",
            },
            "location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Meeting location",
                "desc": "The location of the appointment",
            },
            "targetable_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Target ID",
                "short_desc": "Associated record ID",
                "desc": "The ID of the contact, account, or deal to associate",
            },
            "targetable_type": {
                "type": StringOrNothingType,
                "display_name": "Target Type",
                "short_desc": "Associated record type",
                "desc": "The type of record to associate: `Contact`, `SalesAccount`, or `Deal`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Contact", "display_name": "Contact"},
                    <AllowedValueInfo>{"value": "SalesAccount", "display_name": "Account"},
                    <AllowedValueInfo>{"value": "Deal", "display_name": "Deal"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales appointment request data type for update operations
public class FreshsalesUpdateAppointmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating an appointment
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Appointment ID",
                "required": True,
                "short_desc": "The appointment ID to update",
                "desc": "The unique identifier of the appointment to update",
            },
        } + (map {$1.key: $1.value + {"required": False}},
            FreshsalesCreateAppointmentRequestDataType::Fields.pairIterator());
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales appointment request data type for delete operations
public class FreshsalesDeleteAppointmentRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting an appointment
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Appointment ID",
                "required": True,
                "short_desc": "The appointment ID to delete",
                "desc": "The unique identifier of the appointment to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
