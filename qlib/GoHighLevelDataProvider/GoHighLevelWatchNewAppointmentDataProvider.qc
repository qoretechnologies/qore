# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelWatchNewAppointmentDataProvider.qc GoHighLevel watch new appointment trigger provider

/*  GoHighLevelWatchNewAppointmentDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! GoHighLevel appointment record type
public class GoHighLevelAppointmentDataType inherits HashDataType {
    public {
        #! Field definitions for GoHighLevel appointments
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Appointment ID",
                "short_desc": "Unique identifier of the appointment",
                "desc": "Unique identifier of the appointment",
            },
            "calendarId": {
                "type": StringOrNothingType,
                "display_name": "Calendar ID",
                "short_desc": "The calendar this appointment belongs to",
                "desc": "The calendar this appointment belongs to",
            },
            "contactId": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact associated with this appointment",
                "desc": "The contact associated with this appointment",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The appointment title",
                "desc": "The appointment title",
            },
            "startTime": {
                "type": DateOrNothingType,
                "display_name": "Start Time",
                "short_desc": "The appointment start time",
                "desc": "The appointment start time in ISO 8601 format",
            },
            "endTime": {
                "type": DateOrNothingType,
                "display_name": "End Time",
                "short_desc": "The appointment end time",
                "desc": "The appointment end time in ISO 8601 format",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The appointment status",
                "desc": "Appointment status: `new`, `confirmed`, `cancelled`, `showed`, `noshow`, `invalid`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "new", "desc": "New appointment"},
                    <AllowedValueInfo>{"value": "confirmed", "desc": "Confirmed appointment"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled appointment"},
                    <AllowedValueInfo>{"value": "showed", "desc": "Contact showed up"},
                    <AllowedValueInfo>{"value": "noshow", "desc": "Contact did not show up"},
                    <AllowedValueInfo>{"value": "invalid", "desc": "Invalid appointment"},
                ),
            },
            "assignedUserId": {
                "type": StringOrNothingType,
                "display_name": "Assigned User ID",
                "short_desc": "The user assigned to this appointment",
                "desc": "The user assigned to this appointment",
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The appointment location address",
                "desc": "The appointment location address",
            },
            "locationId": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location/sub-account ID",
                "desc": "The location/sub-account ID",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Watch for new appointments created in GoHighLevel
public class GoHighLevelWatchNewAppointmentDataProvider inherits GoHighLevelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_APPOINTMENT,
            "desc": "Watch for new appointments created in GoHighLevel",
            "type": "GoHighLevelWatchNewAppointmentDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest, *hash<auto> options)
            : GoHighLevelWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_APPOINTMENT;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new appointments created in GoHighLevel";
    }

    #! Fetches new appointments since the given date
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "startTime": formatDateTimeGHL(since),
            "endTime": formatDateTimeGHL(now()),
        };
        hash<auto> response = doGet("calendars/events", params);
        return response.events ?? ();
    }

    #! Returns the timestamp from a record for deduplication
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.startTime;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_NEW_APPOINTMENT;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the data type for appointment event records
    private AbstractDataProviderType getRecordDataType() {
        return new GoHighLevelAppointmentDataType();
    }

    #! Returns fake example data for appointments
    private hash<auto> getFakeExampleData() {
        return {
            "id": "EXAMPLE_APPOINTMENT_12345",
            "calendarId": "EXAMPLE_CALENDAR_12345",
            "contactId": "EXAMPLE_CONTACT_12345",
            "title": "Consultation Call",
            "startTime": "2026-02-15T14:00:00.000Z",
            "endTime": "2026-02-15T15:00:00.000Z",
            "status": "new",
            "assignedUserId": "EXAMPLE_USER_12345",
            "address": "123 Main St, Austin, TX 73301",
            "locationId": "EXAMPLE_LOCATION_12345",
        };
    }
}

} # namespace GoHighLevelDataProvider
