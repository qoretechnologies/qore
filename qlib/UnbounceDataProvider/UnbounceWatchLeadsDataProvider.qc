# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceWatchLeadsDataProvider.qc Unbounce watch leads event data provider

/*  UnbounceWatchLeadsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce watch leads event data provider
public class UnbounceWatchLeadsDataProvider inherits UnbounceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_LEAD_ADDED,
            "desc": "Unbounce lead added event provider - polls for new leads on a page",
            "type": "UnbounceWatchLeadsDataProvider",
            "supports_observable": True,
            "constructor_options": LeadWatchConstructorOptions,
        };

        #! Constructor options for the watch provider (extends base with page_id)
        const LeadWatchConstructorOptions = WatchConstructorOptions + {
            "page_id": <DataProviderOptionInfo>{
                "display_name": "Page ID",
                "short_desc": "Page to watch for new leads",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The page ID to watch for new leads. Required for polling",
                "ref_data": "pages",
            },
        };
    }

    private {
        #! The page ID to poll
        *string page_id;
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : UnbounceWatchDataProviderBase(rest, options) {
        page_id = options.page_id;
    }

    #! Returns the provider name
    string getName() {
        return EVENT_LEAD_ADDED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce lead added event provider - polls for new leads on a page";
    }

    #! Called when observers are ready; starts polling
    observersReady() {
        if (!page_id) {
            *hash<auto> ctx = DataProviderDataContextHelper::getHash();
            page_id = ctx.page_id;
        }
        if (!page_id) {
            throw "UNBOUNCE-ERROR", "page_id is required for watching leads";
        }
        AbstractWatchDataProviderBase::observersReady();
    }

    #! Fetches leads created since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(
            "pages/" + page_id + "/leads",
            "leads",
            {
                "from": formatDateTime(since),
                "sort_order": "asc",
            },
        );
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_LEAD_ADDED;
    }

    #! Returns the data type for lead event records
    private AbstractDataProviderType getRecordDataType() {
        return new UnbounceLeadDataType();
    }

    #! Returns fake example data for leads
    private hash<auto> getFakeExampleData() {
        return UnbounceWatchDataProviderBase::getFakeExampleData() + {
            "page_id": page_id ?? "EXAMPLE_PAGE_ID",
            "variant_id": "EXAMPLE_VARIANT_ID",
            "submitter_ip": "192.168.1.1",
            "form_data": {
                "email": "user@example.com",
                "name": "John Doe",
            },
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
