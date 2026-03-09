# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelWatchNewContactDataProvider.qc GoHighLevel watch new contact trigger provider

/*  GoHighLevelWatchNewContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for new contacts created in GoHighLevel
public class GoHighLevelWatchNewContactDataProvider inherits GoHighLevelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT,
            "desc": "Watch for new contacts created in GoHighLevel",
            "type": "GoHighLevelWatchNewContactDataProvider",
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
        return EVENT_NEW_CONTACT;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new contacts created in GoHighLevel";
    }

    #! Fetches new contacts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> body = {
            "query": "",
            "pageLimit": DefaultPageSize,
        };
        list<hash<auto>> all_contacts = fetchAllPagesPost("contacts/search", "contacts", body, 1000);

        # Client-side filtering by dateAdded
        list<hash<auto>> new_contacts = ();
        foreach hash<auto> contact in (all_contacts) {
            if (contact.dateAdded) {
                date added = date(contact.dateAdded);
                if (added >= since) {
                    new_contacts += contact;
                }
            }
        }
        return new_contacts;
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_NEW_CONTACT;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() {
        return new GoHighLevelContactDataType();
    }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return {
            "id": "EXAMPLE_CONTACT_12345",
            "firstName": "John",
            "lastName": "Smith",
            "name": "John Smith",
            "email": "john.smith@example.com",
            "phone": "+1-555-123-4567",
            "companyName": "Acme Corporation",
            "address1": "123 Main St",
            "city": "Austin",
            "state": "TX",
            "country": "US",
            "postalCode": "73301",
            "tags": ("customer", "vip"),
            "source": "website",
            "dateAdded": "2026-01-30T10:30:00.000Z",
            "dateUpdated": "2026-01-30T10:30:00.000Z",
            "locationId": "EXAMPLE_LOCATION_12345",
        };
    }
}

} # namespace GoHighLevelDataProvider
