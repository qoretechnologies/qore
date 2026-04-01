# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasWatchNewTicketDataProvider.qc Gorgias watch new tickets data provider

/*  GorgiasWatchNewTicketDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias watch new tickets data provider
/** Polls for new tickets by checking the tickets endpoint sorted by creation time
    and emitting events for newly created tickets.
*/
public class GorgiasWatchNewTicketDataProvider inherits GorgiasWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_TICKET,
            "desc": "Watch for new Gorgias tickets by polling for recently created tickets",
            "type": "GorgiasWatchNewTicketDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : GorgiasWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_TICKET;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new Gorgias tickets by polling for recently created tickets";
    }

    #! Fetches recently created tickets
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "order_by": "created_datetime:desc",
        };
        return fetchAllPages("tickets", params, 50);
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_NEW_TICKET;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() {
        return new GorgiasTicketDataType();
    }

    #! Returns the timestamp field
    private string getTimestampField() {
        return "created_datetime";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
