# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreSaleTrackingWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Watch provider for sale tracking changes
public class Cin7CoreSaleTrackingWatchDataProvider inherits Cin7CoreWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = BaseWatchProviderInfo + <DataProviderInfo>{
            "name": EVENT_SALE_TRACKING_CHANGED,
            "desc": "Triggers when sale tracking/shipment information changes",
            "type": "Cin7CoreSaleTrackingWatchDataProvider",
        };
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn, *hash<auto> options)
            : Cin7CoreWatchDataProviderBase(conn, options) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreWatchDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_SALE_TRACKING_CHANGED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when sale tracking/shipment information changes";
    }

    #! Poll for tracking changes
    list<hash<auto>> pollOnce() {
        hash<auto> params = {
            "ModifiedSince": since,
            "Page": 1,
            "Limit": MaxPageSize,
        };

        @debug(log(LoggerLevel::DEBUG, "pollOnce: polling for tracking changes since %y", since));
        hash<auto> response = doGet("/SaleFulfilment", params);
        return response.FulfilmentList ?? ();
    }

    #! Get the unique ID from a fulfilment record
    string getRecordId(hash<auto> record) {
        return record.TaskID ?? record.ID ?? "";
    }

    #! Get the event name
    string getEventName() {
        return EVENT_SALE_TRACKING_CHANGED;
    }

    #! Get the event data type
    AbstractDataProviderType getEventDataType() {
        return new Cin7CoreFulfilmentDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace Cin7CoreDataProvider
