# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateMeetingDataProvider.qc Close create meeting action data provider

/*  CloseCreateMeetingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Request type for creating a meeting
public class CloseCreateMeetingRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a meeting
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead to associate with this meeting",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Meeting title",
                "desc": "The title of the meeting",
                "preselected": True,
            },
            "starts_at": {
                "type": StringOrNothingType,
                "display_name": "Starts At",
                "short_desc": "Meeting start time",
                "desc": "The start time of the meeting (ISO 8601 format)",
            },
            "ends_at": {
                "type": StringOrNothingType,
                "display_name": "Ends At",
                "short_desc": "Meeting end time",
                "desc": "The end time of the meeting (ISO 8601 format)",
            },
            "attendees": {
                "type": new SoftListDataType("Attendees", new CloseAttendeeDataType(), True),
                "display_name": "Attendees",
                "short_desc": "Meeting attendees",
                "desc": "A list of attendee objects for this meeting",
            },
            "location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Meeting location",
                "desc": "The location of the meeting",
            },
            "user_note_html": {
                "type": StringOrNothingType,
                "display_name": "Note HTML",
                "short_desc": "Meeting note in HTML",
                "desc": "An HTML note to associate with the meeting",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The unique identifier of the contact associated with this meeting",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The meeting organizer",
                "desc": "The unique identifier of the user who organized this meeting",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create meeting action data provider
public class CloseCreateMeetingDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new meeting activity in Close",
            "type": "CloseCreateMeetingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new meeting activity in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateMeetingRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseMeetingDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activity/meeting", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
