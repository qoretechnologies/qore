# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloWatchDataProviders.qc Apollo polling watch data providers

/*  ApolloWatchDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ApolloDataProvider {

#! Watch for new contacts created in Apollo
public class ApolloWatchContactsCreatedDataProvider inherits ApolloWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_CREATED,
            "desc": "Watch for new contacts created in Apollo",
            "type": "ApolloWatchContactsCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest, *hash<auto> options)
            : ApolloWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new contacts created in Apollo"; }

    #! Fetches new contacts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        list<hash<auto>> all_results = ();
        int page = 1;
        while (True) {
            hash<auto> body = {
                "sort_by_field": "contact_created_at",
                "sort_ascending": False,
                "page": page,
                "per_page": DefaultPageSize,
            };
            hash<auto> response = doRestRequest("POST", "contacts/search", body);
            list<hash<auto>> records = response.contacts ?? ();
            if (!records) {
                break;
            }
            # Filter to records created since the given date
            list<hash<auto>> filtered = select records,
                ($1.created_at && date($1.created_at) > since);
            all_results += filtered;
            # If we got fewer matches than the page size, no more pages
            # or if some records are older than since (sorted desc), we're done
            if (records.size() < DefaultPageSize || filtered.size() < records.size()) {
                break;
            }
            ++page;
        }
        return all_results;
    }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() { return new ApolloContactDataType(); }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return ApolloWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "John",
            "last_name": "Smith",
            "email": "john@example.com",
        };
    }
}

#! Watch for updated contacts in Apollo
public class ApolloWatchContactsUpdatedDataProvider inherits ApolloWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_UPDATED,
            "desc": "Watch for updated contacts in Apollo",
            "type": "ApolloWatchContactsUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest, *hash<auto> options)
            : ApolloWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for updated contacts in Apollo"; }

    #! Fetches updated contacts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        list<hash<auto>> all_results = ();
        int page = 1;
        while (True) {
            hash<auto> body = {
                "sort_by_field": "contact_updated_at",
                "sort_ascending": False,
                "page": page,
                "per_page": DefaultPageSize,
            };
            hash<auto> response = doRestRequest("POST", "contacts/search", body);
            list<hash<auto>> records = response.contacts ?? ();
            if (!records) {
                break;
            }
            # Filter to records updated since the given date
            list<hash<auto>> filtered = select records,
                ($1.updated_at && date($1.updated_at) > since);
            all_results += filtered;
            if (records.size() < DefaultPageSize || filtered.size() < records.size()) {
                break;
            }
            ++page;
        }
        return all_results;
    }

    #! Returns the record timestamp (use updated_at for updated contacts)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at ?? record.created_at;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() { return new ApolloContactDataType(); }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return ApolloWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "John",
            "last_name": "Smith",
            "email": "john@example.com",
        };
    }
}

#! Watch for new accounts created in Apollo
public class ApolloWatchAccountsCreatedDataProvider inherits ApolloWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ACCOUNT_CREATED,
            "desc": "Watch for new accounts created in Apollo",
            "type": "ApolloWatchAccountsCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest, *hash<auto> options)
            : ApolloWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_ACCOUNT_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new accounts created in Apollo"; }

    #! Fetches new accounts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        list<hash<auto>> all_results = ();
        int page = 1;
        while (True) {
            hash<auto> body = {
                "sort_by_field": "account_created_at",
                "sort_ascending": False,
                "page": page,
                "per_page": DefaultPageSize,
            };
            hash<auto> response = doRestRequest("POST", "accounts/search", body);
            list<hash<auto>> records = response.accounts ?? ();
            if (!records) {
                break;
            }
            # Filter to records created since the given date
            list<hash<auto>> filtered = select records,
                ($1.created_at && date($1.created_at) > since);
            all_results += filtered;
            if (records.size() < DefaultPageSize || filtered.size() < records.size()) {
                break;
            }
            ++page;
        }
        return all_results;
    }

    #! Returns the event name
    private string getEventName() { return EVENT_ACCOUNT_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for account event records
    private AbstractDataProviderType getRecordDataType() { return new ApolloAccountDataType(); }

    #! Returns fake example data for accounts
    private hash<auto> getFakeExampleData() {
        return ApolloWatchDataProviderBase::getFakeExampleData() + {
            "name": "Acme Corp",
            "domain": "acme.com",
            "website_url": "https://www.acme.com",
            "phone": "+1-555-123-4567",
            "industry": "Technology",
        };
    }
}

#! Watch for new deals created in Apollo
public class ApolloWatchDealsCreatedDataProvider inherits ApolloWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DEAL_CREATED,
            "desc": "Watch for new deals created in Apollo",
            "type": "ApolloWatchDealsCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest, *hash<auto> options)
            : ApolloWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_DEAL_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new deals created in Apollo"; }

    #! Fetches new deals since the given date
    private list<hash<auto>> fetchRecords(date since) {
        list<hash<auto>> all_results = ();
        int page = 1;
        while (True) {
            hash<auto> response = doGet("opportunities/search", {
                "page": page,
                "per_page": DefaultPageSize,
            });
            list<hash<auto>> records = response.opportunities ?? ();
            if (!records) {
                break;
            }
            list<hash<auto>> filtered = select records,
                ($1.created_at && date($1.created_at) > since);
            all_results += filtered;
            if (records.size() < DefaultPageSize || filtered.size() < records.size()) {
                break;
            }
            ++page;
        }
        return all_results;
    }

    #! Returns the event name
    private string getEventName() { return EVENT_DEAL_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for deal event records
    private AbstractDataProviderType getRecordDataType() { return new ApolloDealDataType(); }

    #! Returns fake example data for deals
    private hash<auto> getFakeExampleData() {
        return ApolloWatchDataProviderBase::getFakeExampleData() + {
            "name": "Enterprise License",
            "amount": 50000.0,
            "status": "open",
            "stage_name": "Negotiation",
            "owner_id": "abc123def456",
        };
    }
}

} # namespace ApolloDataProvider
