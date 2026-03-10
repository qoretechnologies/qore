# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseUpdateMeetingDataProvider.qc Close update meeting action data provider

/*  CloseUpdateMeetingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a meeting
public class CloseUpdateMeetingRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a meeting
        const Fields = {
            "meeting_id": {
                "type": StringType,
                "display_name": "Meeting ID",
                "short_desc": "The meeting ID to update",
                "desc": "The unique identifier of the meeting to update",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Meeting title",
                "desc": "The new title of the meeting",
            },
            "starts_at": {
                "type": StringOrNothingType,
                "display_name": "Starts At",
                "short_desc": "Meeting start time",
                "desc": "The new start time of the meeting (ISO 8601 format)",
            },
            "ends_at": {
                "type": StringOrNothingType,
                "display_name": "Ends At",
                "short_desc": "Meeting end time",
                "desc": "The new end time of the meeting (ISO 8601 format)",
            },
            "location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Meeting location",
                "desc": "The new location of the meeting",
            },
            "user_note_html": {
                "type": StringOrNothingType,
                "display_name": "Note HTML",
                "short_desc": "Meeting note in HTML",
                "desc": "The new HTML note for the meeting",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close update meeting action data provider
public class CloseUpdateMeetingDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing meeting activity in Close",
            "type": "CloseUpdateMeetingDataProvider",
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing meeting activity in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseUpdateMeetingRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseMeetingDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string meeting_id = remove req.meeting_id;
        return doPut("activity/meeting/" + meeting_id, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
