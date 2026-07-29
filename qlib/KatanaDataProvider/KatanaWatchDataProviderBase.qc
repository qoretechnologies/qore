# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchDataProviderBase.qc Base class for Katana polling watch data providers

/*  KatanaWatchDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {

#! Base class for Katana polling-based watch data providers.
/** Subclasses provide endpoint, event name, record data type, and (optionally) a target
    status to filter on. The base class runs a poll loop, fetches records since the last
    check, dedupes by ID, fires a \c data event per new/updated record, advances
    \c last_check, and prunes \c seen_ids when it grows beyond 10000 entries.
*/
public class KatanaWatchDataProviderBase inherits KatanaDataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Constructor options for watch providers
        const WatchConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll for new records (seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval
                    + "`, default `" + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Start watching from this date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only emit events for records seen on or after this UTC timestamp "
                    "(default: now at construction time)",
            },
        };
    }

    #! Creates the object with an async REST client and options
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaDataProviderBase(rest),
              AbstractTimestampWatchDataProvider(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval),
                  options.start_date) {
    }

    #! Destructor — stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! The upstream ordering is not guaranteed, so the watch orders records itself before delivery
    /** Records are sorted by their timestamp and delivered oldest-first, and the durable cursor
        advances only after a record has been delivered successfully.
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        log(LoggerLevel::ERROR, "Error polling %s: %s: %s", getEventName(), ex.err, ex.desc);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        log(LoggerLevel::INFO, "%s", vsprintf(fmt, argv));
    }

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": sprintf("Fired when a %s event is detected", getEventName()),
                "type": getRecordDataType(),
            },
        };
    }

    #! Returns example event data for documentation/testing
    private auto getExampleEventDataImpl(string event_id) {
        try {
            list<hash<auto>> records = fetchRecords(now_us() - 30D);
            if (records) {
                return records[0];
            }
        } catch () {
        }
        return getFakeExampleData();
    }

    #! Default fetch implementation — list endpoint filtered by \c updated_at_min.
    /** Subclasses may override to use different filters (e.g. \c created_at_min for
        watch-new providers, or to add \c status, \c include_deleted, etc).
    */
    list<hash<auto>> fetchRecords(date since) {
        hash<auto> extra = getExtraFetchParams();
        try {
            return fetchRecordsUpdatedAfter(getEndpoint(), since, extra);
        } catch (hash<ExceptionInfo> ex) {
            error("Error fetching %s for %s: %s: %s", getEndpoint(),
                getEventName(), ex.err, ex.desc);
            return ();
        }
    }

    #! Returns the timestamp field on records used to advance \c last_check.
    /** Defaults to \c updated_at. Subclasses watching for \c created may override.
    */
    *string getTimestampField() {
        return "updated_at";
    }

    #! Returns extra query parameters to send with each fetch (e.g. \c status filter).
    /** Defaults to empty. Subclasses may override.
    */
    hash<auto> getExtraFetchParams() {
        return {};
    }

    #! Returns the ID field name for this resource (defaults to \c id)
    string getIdField() {
        return "id";
    }

    #! Returns the REST endpoint path (without leading slash) for fetching records
    abstract string getEndpoint();

    #! Returns the event name (DPAT_EVENT action_val)
    abstract string getEventName();

    #! Returns the record data type for this event
    abstract AbstractDataProviderType getRecordDataType();

    #! Returns fake example data when the API is unreachable
    abstract hash<auto> getFakeExampleData();
}

} # namespace KatanaDataProvider
