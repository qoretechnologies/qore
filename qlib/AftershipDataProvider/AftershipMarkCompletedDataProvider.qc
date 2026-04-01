# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipMarkCompletedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Request type for marking a tracking as completed
public class AftershipMarkCompletedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tracking ID",
                "short_desc": "The tracking ID to mark as completed",
                "desc": "The Aftership tracking ID to mark as completed",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "reason": {
                "display_name": "Reason",
                "short_desc": "Completion reason",
                "desc": "Reason for marking the tracking as completed",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DELIVERED", "display_name": "Delivered"},
                    <AllowedValueInfo>{"value": "LOST", "display_name": "Lost"},
                    <AllowedValueInfo>{"value": "RETURNED_TO_SENDER", "display_name": "Returned to Sender"},
                ),
            },
            "event_datetime": {
                "display_name": "Event Date/Time",
                "short_desc": "When the event occurred",
                "desc": "Timestamp of when the completion event occurred. If not provided, "
                    "the current time is used",
                "type": AbstractDataProviderType::get(DateOrNothingType),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AftershipMarkCompletedRequestDataType = new AftershipMarkCompletedRequestDataType();

#! Data provider for marking a tracking as completed
public class AftershipMarkCompletedDataProvider inherits AftershipDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-as-completed",
            "type": "AftershipMarkCompletedDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = AftershipMarkCompletedRequestDataType;
        const ResponseType = AftershipTrackingDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AftershipDataProviderBase(rest) {}
    constructor(*hash<auto> options) : AftershipDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.id;
        return doTrackingCommand("POST", "trackings/" + id + "/mark-as-completed",
            req).body.data.tracking;
    }
}
}
