# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactWatchNewContactListsDataProvider.qc Watch for new/updated contact lists

/*  ConstantContactWatchNewContactListsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Watch for new or updated contact lists in Constant Contact via polling
public class ConstantContactWatchNewContactListsDataProvider
        inherits ConstantContactWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT_LIST,
            "desc": "Watch for new or updated contact lists in Constant Contact",
            "type": "ConstantContactWatchNewContactListsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : ConstantContactWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() { return EVENT_NEW_CONTACT_LIST; }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new or updated contact lists in Constant Contact via polling";
    }

    #! Fetches contact lists (CC doesn't support updated_after for lists, so we fetch all)
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("contact_lists", "lists", {"limit": DefaultPageSize});
    }

    #! Returns the record ID
    private string getRecordId(hash<auto> record) {
        return string(record.list_id);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_CONTACT_LIST; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact list records
    private AbstractDataProviderType getRecordDataType() {
        return new ConstantContactContactListDataType();
    }

    #! Returns fake example data for contact lists
    private hash<auto> getFakeExampleData() {
        return {
            "list_id": "cbc05a6e-03c6-4e1a-b5c5-9b123456789a",
            "name": "Newsletter Subscribers",
            "description": "Main newsletter subscriber list",
            "favorite": False,
            "membership_count": 150,
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }
}

} # namespace ConstantContactDataProvider
