# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditLeadAdsWatchNewLeadDataProvider.qc Reddit Lead Ads watch new lead trigger

/*  RedditLeadAdsWatchNewLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditLeadAdsDataProvider module
public namespace RedditLeadAdsDataProvider {

#! Watch for new lead form submissions from Reddit Lead Generation Ads
public class RedditLeadAdsWatchNewLeadDataProvider inherits RedditLeadAdsDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_LEAD,
            "desc": "Watch for new lead form submissions from Reddit Lead Generation Ads",
            "type": "RedditLeadAdsWatchNewLeadDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };

        #! Constructor options for watch provider
        const WatchConstructorOptions = BaseConstructorOptions + {
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "The Reddit Ads account ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The Reddit Ads account ID to poll for lead form submissions",
            },
            "campaign_id": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "Filter to a specific campaign",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Optional campaign ID to filter lead submissions. If not set, "
                    "watches all campaigns in the account.",
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new leads (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only detect leads submitted after this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only detect leads submitted after this date/time",
            },
        };
    }

    private {
        #! The account ID to poll
        *string account_id;

        #! Optional campaign ID filter
        *string campaign_id;

        #! Last check time
        date last_check;

        #! Set of seen IDs to avoid duplicates
        hash<string, bool> seen_ids;
    }

    #! Creates the object with a REST client (no options — for provider tree traversal)
    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestClientIo rest)
            : RedditLeadAdsDataProviderBase(rest),
              AbstractWatchDataProviderBase(DefaultPollInterval) {
        last_check = now_us();
    }

    #! Creates the object with a REST client and options (for trigger activation)
    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestClientIo rest, hash<auto> options)
            : RedditLeadAdsDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        account_id = options.account_id;
        campaign_id = options.campaign_id;
        last_check = options.start_date ?? now_us();
    }

    #! Destructor - stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-progress poll operation for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Called when observers are ready; gets options from context if needed and starts polling
    observersReady() {
        if (!account_id) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            if (ctx.account_id) {
                account_id = ctx.account_id;
            }
            if (ctx.campaign_id) {
                campaign_id = ctx.campaign_id;
            }
            if (ctx.start_date) {
                last_check = date(ctx.start_date);
            }
        }
        if (!account_id) {
            throw "REDDIT-LEAD-ADS-ERROR", "account_id is required for watching new leads; "
                "provide it as a constructor option or via data context";
        }
        AbstractWatchDataProviderBase::observersReady();
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_LEAD;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new lead form submissions from Reddit Lead Generation Ads";
    }

    #! Polls for new lead submissions
    private pollOnce() {
        if (!account_id) {
            log(LoggerLevel::ERROR, "Cannot poll for leads: account_id is not set");
            return;
        }
        try {
            # Build the API path
            # NOTE: the lead_form_submissions endpoint is not documented in the public Reddit
            # Ads API v3 OpenAPI spec.  Reddit lead gen ads deliver data through the Ads Manager
            # UI (CSV) and Zapier.  If this endpoint does not exist, the provider will log
            # errors but otherwise stay idle.  Verify the path against your API access level
            # and update if Reddit exposes a different route.
            string path = sprintf("accounts/%s/lead_form_submissions", account_id);
            hash<auto> params;
            if (campaign_id) {
                params.campaign_id = campaign_id;
            }

            hash<auto> response = doGet(path, params);

            # Extract leads from response — try common Reddit API envelope keys
            *list<auto> leads = response.data ?? response.lead_form_submissions ?? response.results;
            if (!leads) {
                return;
            }

            date new_last_check = last_check;

            foreach hash<auto> lead in (leads) {
                if (isStopping()) {
                    break;
                }

                string id = string(lead.id);

                # Skip if already seen
                if (seen_ids{id}) {
                    continue;
                }

                # Check timestamp to avoid old records
                *date record_time;
                if (lead.created_at) {
                    record_time = date(lead.created_at);
                }
                if (record_time) {
                    if (record_time <= last_check) {
                        continue;
                    }
                    if (record_time > new_last_check) {
                        new_last_check = record_time;
                    }
                }

                # Mark as seen
                seen_ids{id} = True;

                # Notify observers
                notifyObservers(EVENT_NEW_LEAD, lead);
            }

            last_check = new_last_check;

            # Prune seen_ids if it gets too large — keep the most recent half
            # to avoid re-triggering duplicates for recently-seen IDs
            if (seen_ids.size() > MaxSeenIds) {
                int keep = MaxSeenIds / 2;
                seen_ids = (map {$1: True}, (keys seen_ids)[seen_ids.size() - keep ..]);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling for new leads: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_LEAD: <DataProviderMessageInfo>{
                "desc": "Fired when a new lead form submission is detected",
                "type": new RedditLeadAdsLeadDataType(),
            },
        };
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        # Try real API data first
        try {
            string path = sprintf("accounts/%s/lead_form_submissions", account_id);
            hash<auto> response = doGet(path, {"limit": 1});
            *list<auto> leads = response.data ?? response.lead_form_submissions ?? response.results;
            if (leads) {
                return leads[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Returns fake example data
    private hash<auto> getFakeExampleData() {
        return {
            "id": "lead_12345",
            "created_at": "2026-01-30T10:30:00Z",
            "campaign_id": "t8_abc123",
            "campaign_name": "Q1 Lead Gen Campaign",
            "ad_group_id": "t8_def456",
            "ad_id": "t11_ghi789",
            "form_id": "form_xyz",
            "email": "prospect@example.com",
            "full_name": "Jane Smith",
            "job_title": "Product Manager",
            "company_name": "Acme Corp",
            "phone_number": "+1-555-0123",
            "custom_fields": {
                "preferred_contact_method": "email",
                "interest_area": "Enterprise Solutions",
            },
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditLeadAdsDataProvider
