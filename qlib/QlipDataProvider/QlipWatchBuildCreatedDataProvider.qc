# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipWatchBuildCreatedDataProvider.qc Qlip watch for new builds

/*  QlipWatchBuildCreatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! Watch for newly-created builds in Qlip
public class QlipWatchBuildCreatedDataProvider inherits QlipWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_BUILD_CREATED,
            "desc": "Watch for newly-created builds in Qlip",
            "type": "QlipWatchBuildCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(QlipRestClient::QlipRestClient rest, *hash<auto> options)
            : QlipWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_BUILD_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for newly-created builds in Qlip";
    }

    #! Fetches builds created since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("builds", "builds",
            {"startDate": QlipDataProviderBase::formatDateTime(since)}, DefaultPageSize);
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_BUILD_CREATED;
    }

    #! Returns the data type for build event records
    private AbstractDataProviderType getRecordDataType() {
        return new QlipBuildDataType();
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace QlipDataProvider
