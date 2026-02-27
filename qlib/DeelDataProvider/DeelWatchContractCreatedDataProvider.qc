# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWatchContractCreatedDataProvider.qc Deel watch for new contracts data provider

/*  DeelWatchContractCreatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Watch for new contracts in Deel
public class DeelWatchContractCreatedDataProvider inherits DeelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTRACT_CREATED,
            "desc": "Watch for newly created contracts in Deel",
            "type": "DeelWatchContractCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest, *hash<auto> options)
            : DeelWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_CONTRACT_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for newly created contracts in Deel";
    }

    #! Fetches new contracts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("contracts", "contracts",
            {"dateFrom": formatDate(since), "limit": DefaultPageSize});
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CONTRACT_CREATED;
    }

    #! Returns the data type for contract event records
    private AbstractDataProviderType getRecordDataType() {
        return new DeelContractDataType();
    }

    #! Returns fake example data for contracts
    private hash<auto> getFakeExampleData() {
        return DeelWatchDataProviderBase::getFakeExampleData() + {
            "title": "Software Engineer - Backend",
            "type": "fixed",
            "status": "pending",
            "currency": "USD",
            "amount": 5000.00,
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
