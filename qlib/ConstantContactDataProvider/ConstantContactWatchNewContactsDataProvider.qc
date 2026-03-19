# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactWatchNewContactsDataProvider.qc Watch for new/updated contacts

/*  ConstantContactWatchNewContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch for new or updated contacts in Constant Contact via polling
public class ConstantContactWatchNewContactsDataProvider
        inherits ConstantContactWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT,
            "desc": "Watch for new or updated contacts in Constant Contact",
            "type": "ConstantContactWatchNewContactsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest, *hash<auto> options)
            : ConstantContactWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() { return EVENT_NEW_CONTACT; }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new or updated contacts in Constant Contact via polling";
    }

    #! Fetches contacts updated since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        return fetchAllPages("contacts", "contacts",
            {"updated_after": since_str, "limit": DefaultPageSize});
    }

    #! Returns the record ID
    private string getRecordId(hash<auto> record) {
        return string(record.contact_id);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NEW_CONTACT; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact records
    private AbstractDataProviderType getRecordDataType() {
        return new ConstantContactContactDataType();
    }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return {
            "contact_id": "04fe9a97-a579-43c5-bb1a-58ed29bf0a6a",
            "email_address": {
                "address": "john.doe@example.com",
                "permission_to_send": "implicit",
            },
            "first_name": "John",
            "last_name": "Doe",
            "created_at": "2026-01-30T10:30:00Z",
            "updated_at": "2026-01-30T10:30:00Z",
        };
    }
}

} # namespace ConstantContactDataProvider
