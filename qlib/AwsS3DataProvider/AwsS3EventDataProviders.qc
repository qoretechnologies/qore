# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3EventDataProviders.qc event providers for the AwsS3DataProvider module

/*  AwsS3EventDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AwsS3DataProvider module
public namespace AwsS3DataProvider {
#! Common base class for the Amazon S3 polling watch providers
/** Amazon S3 offers no change feed of its own: bucket notifications have to be configured on the bucket, and
    deliver to SNS, SQS, or Lambda rather than to a client.  A watch on S3 is therefore a poll, and this class
    supplies the shape that both S3 watches share.

    Both feeds are ordered by a timestamp -- a bucket by its creation date, an object by its last-modified date
    -- so both use @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider",
    which sorts each cycle's candidates, discards those the durable cursor already covers, dispatches oldest
    first, and advances the cursor only after a successful dispatch.  Delivery is therefore at-least-once, and
    an event whose consumer fails is delivered again rather than lost.
*/
public class AwsS3WatchDataProviderBase inherits AwsS3DataProviderBase,
        DataProvider::AbstractTimestampWatchDataProvider {
    public {
        #! Options shared by every S3 watch provider
        const CommonWatchOptions = ConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for changes, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to check for changes, in seconds (minimum `" + MinPollInterval + "`, "
                    "default `" + DefaultPollInterval + "`).\n\n"
                    "Amazon S3 charges per request, so a short interval on a large bucket costs money as well "
                    "as time",
                "default_value": DefaultPollInterval,
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Do not report anything older than this",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Do not report anything older than this date.\n\n"
                    "If not set, the watch reports only what appears after it starts, so an existing bucket "
                    "does not produce a flood of events the first time it runs",
            },
        };
    }

    #! Creates the object from an AwsRestClient
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True, *hash<auto> options)
            : AwsS3DataProviderBase(rest, path_style),
              AbstractTimestampWatchDataProvider(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval), options.start_date) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<string, hash<DataProviderOptionInfo>> con_opts = CommonWatchOptions)
            : AwsS3DataProviderBase(options, con_opts),
              AbstractTimestampWatchDataProvider(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval), options.start_date) {
    }

    #! Cancels an in-flight request so that a stop does not wait out the poll
    private cancelPollOperation() {
        rest.disconnect();
    }

    #! The upstream ordering is not relied on
    /** A listing is returned in key order rather than in timestamp order, so the shared template sorts each
        cycle's candidates by timestamp before dispatching them
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        # NOTE: "LoggerWrapper::error()", not a bare "error()".  AbstractDataProvider declares
        # "error(string err, string fmt, ...)", which *throws* rather than logging, and it wins overload
        # resolution here -- so a bare call would turn a logged poll error into an exception raised from the
        # handler that exists to report it, and this one escapes the polling thread's own catch and stops the
        # watch permanently
        LoggerWrapper::error(sprintf("Error polling for %s events: %s: %s", getEventName(), ex.err, ex.desc));
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        info("%s", vsprintf(fmt, argv));
    }

    #! Returns the record field holding the record's stable identity
    private string getIdField() {
        return "id";
    }

    #! Returns example event data
    /** Real data is used when the account has any, since an example built from the account under test shows
        the caller the field values they will actually receive.

        @note A configuration error is raised rather than swallowed.  Only an API failure -- expired
        credentials, a policy that does not allow the listing, no connectivity -- falls back to fabricated
        data; a trigger that names no bucket would otherwise show a plausible example and give no hint that it
        could never fire, with the missing option surfacing much later as an error in the poll log
    */
    private auto getExampleEventDataImpl(string event_id) {
        checkWatchConfig();
        try {
            list<hash<auto>> records = fetchRecords(1970-01-01Z);
            if (records) {
                return records[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            debug("getExampleEventDataImpl(): %s: %s", ex.err, ex.desc);
        }
        return getFakeExampleData();
    }

    #! Verifies that the watch has everything it needs to poll
    /** The default implementation does nothing; a watch that has to be scoped overrides this and raises an
        error naming the option that is missing.

        @throw S3-WATCH-ERROR the watch is not configured well enough to poll
    */
    private checkWatchConfig() {
    }

    #! Returns the event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getEventDesc(),
                "type": getRecordDataType(),
            },
        };
    }

    #! Returns example data used when the account holds nothing to build an example from
    abstract private hash<auto> getFakeExampleData();

    #! Returns the description of the event this provider raises
    abstract private string getEventDesc();

    #! Returns the data type of the event this provider raises
    abstract private AbstractDataProviderType getRecordDataType();
}

#! Reports each bucket created in the account
/** The bucket list is small and cheap to poll -- one request covers the whole account -- so this watch costs
    one request per interval whatever the account holds.
*/
public class AwsS3WatchNewBucketDataProvider inherits AwsS3WatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_BUCKET,
            "type": "AwsS3WatchNewBucketDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True, *hash<auto> options)
            : AwsS3WatchDataProviderBase(rest, path_style, options) {
    }
    constructor(*hash<auto> options) : AwsS3WatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the buckets created at or after the given timestamp
    /** Every bucket is returned by one request and filtered here; S3 has no way to ask for only the recent
        ones, and there is no pagination on this endpoint

        @param since the cursor position

        @return the candidate records
    */
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> resp = doS3Request("GET", NOTHING, NOTHING);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListAllMyBucketsResult;
        list<hash<auto>> rv = ();
        foreach hash<auto> b in (AwsS3DataProviderBase::forceList(doc.Buckets.Bucket)) {
            if (!b.Name.val() || !b.CreationDate.val()) {
                continue;
            }
            date created = date(b.CreationDate);
            if (created < since) {
                continue;
            }
            rv += {
                "id": b.Name,
                "name": b.Name,
                "creation_date": created,
                "owner_id": doc.Owner.ID,
                "owner_name": doc.Owner.DisplayName,
            };
        }
        return rv;
    }

    private string getEventName() { return EVENT_NEW_BUCKET; }
    private string getTimestampField() { return "creation_date"; }

    private string getEventDesc() {
        return "Fired when a bucket is created in the account.\n\n"
            "Amazon S3 has no change feed, so this is a poll of the account's bucket list; a bucket created "
            "and removed between two polls is not reported.";
    }

    private AbstractDataProviderType getRecordDataType() {
        return AwsS3NewBucketEventDataType;
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": "example-bucket",
            "name": "example-bucket",
            "creation_date": 2026-04-09T12:00:00Z,
            "owner_id": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            "owner_name": "example-owner",
        };
    }
}

#! Reports each object created or updated in a bucket
/** Amazon S3 has no server-side "modified since" filter on a listing, so this lists keys and compares their
    timestamps itself.  Two things follow, and both are worth knowing before pointing this at a production
    bucket:

    - Set \c prefix whenever the objects of interest share one.  It is applied by S3 rather than here, so it
      reduces what is transferred and paid for, not just what is examined.
    - A scan is bounded by \c max_scan_keys.  Reaching the bound is logged as a warning naming the options
      that narrow it, because a scan that silently stopped early would look exactly like a bucket in which
      nothing had changed.

    A deletion is not reported: a listing shows what is there, and an object that has gone leaves nothing to
    report.  A watch on a version-enabled bucket sees a delete marker as an ordinary change to the key.
*/
public class AwsS3WatchObjectsDataProvider inherits AwsS3WatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_OR_UPDATED_FILE,
            "type": "AwsS3WatchObjectsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Options for this watch provider
        const WatchOptions = CommonWatchOptions + {
            "bucket": <DataProviderOptionInfo>{
                "display_name": "Bucket",
                "short_desc": "The bucket to watch",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The bucket to watch for new and updated objects",
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "prefix": <DataProviderOptionInfo>{
                "display_name": "Prefix",
                "short_desc": "Watch only keys beginning with this string",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Watch only keys beginning with this string.\n\n"
                    "The prefix is applied by Amazon S3 rather than after the listing arrives, so narrowing a "
                    "large bucket with it reduces what each poll transfers and costs, not only what it "
                    "examines",
                "example_value": "incoming/",
            },
            "max_scan_keys": <DataProviderOptionInfo>{
                "display_name": "Maximum Keys Scanned",
                "short_desc": "The maximum number of keys examined in one poll",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The maximum number of keys examined in one poll (default `" + DefaultMaxScanKeys
                    + "`).\n\n"
                    "S3 cannot filter a listing by modification time, so every poll examines the keys under "
                    "`prefix` to find the changed ones.  This bounds that work; a poll that reaches the bound "
                    "logs a warning, since a scan that stopped early would otherwise be indistinguishable "
                    "from a bucket in which nothing had changed",
                "default_value": DefaultMaxScanKeys,
            },
        };
    }

    private {
        #! The bucket to watch
        *string watch_bucket;

        #! The key prefix to watch, if any
        *string watch_prefix;

        #! The maximum number of keys examined in one poll
        int max_scan_keys = DefaultMaxScanKeys;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True, *hash<auto> options)
            : AwsS3WatchDataProviderBase(rest, path_style, options) {
        setWatchOptions(options);
    }

    constructor(*hash<auto> options) : AwsS3WatchDataProviderBase(options, WatchOptions) {
        setWatchOptions(options);
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Applies the watch-specific options
    private setWatchOptions(*hash<auto> options) {
        watch_bucket = options.bucket;
        watch_prefix = options.prefix;
        if (options.max_scan_keys) {
            max_scan_keys = options.max_scan_keys;
        }
    }

    #! Returns the configuration that defines which feed this provider follows
    /** The bucket and the prefix select which objects the feed covers, so each defines a different feed and a
        change to either must start a new cursor rather than resume the previous one against a different set of
        objects.  \c max_scan_keys is excluded: it bounds the work of one poll rather than selecting what the
        feed covers.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return AwsS3WatchDataProviderBase::getWatchStreamIdentityImpl() + {
            "bucket": watch_bucket,
            "prefix": watch_prefix,
        };
    }

    #! Verifies that a bucket to watch has been named
    /** @throw S3-WATCH-ERROR no bucket was configured
    */
    private checkWatchConfig() {
        if (!watch_bucket.val()) {
            throw "S3-WATCH-ERROR", sprintf("cannot watch for %y events: no bucket was given; set the 'bucket' "
                "option to the bucket to watch", EVENT_NEW_OR_UPDATED_FILE);
        }
    }

    #! Returns the objects modified at or after the given timestamp
    /** @param since the cursor position

        @return the candidate records

        @throw S3-WATCH-ERROR no bucket was configured
    */
    private list<hash<auto>> fetchRecords(date since) {
        checkWatchConfig();

        list<hash<auto>> rv = ();
        *string token;
        int scanned = 0;
        bool bounded = False;
        while (True) {
            if (isStopping()) {
                # the cursor stays where it is, so this cycle is read again
                break;
            }
            # NOTE: "hash<auto!>"; an all-integer literal narrows to hash<string, int>, and the string
            # options added below would then raise RUNTIME-TYPE-ERROR
            hash<auto!> query = {
                "list-type": 2,
                "max-keys": MaxKeys,
            };
            if (watch_prefix.val()) {
                query.prefix = watch_prefix;
            }
            if (token.val()) {
                query."continuation-token" = token;
            }

            hash<auto> resp = doS3Request("GET", watch_bucket, NOTHING, query);
            *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListBucketResult;
            foreach hash<auto> obj in (AwsS3DataProviderBase::forceList(doc.Contents)) {
                ++scanned;
                if (!obj.Key.val() || !obj.LastModified.val()) {
                    continue;
                }
                date modified = date(obj.LastModified);
                if (modified < since) {
                    continue;
                }
                rv += {
                    "id": obj.Key,
                    "bucket": watch_bucket,
                    "key": obj.Key,
                    "size": obj.Size.toInt(),
                    "last_modified": modified,
                    "etag": AwsS3ListObjectsDataProvider::stripEtag(obj.ETag),
                    "storage_class": obj.StorageClass,
                };
            }

            bool truncated = doc.IsTruncated == "true" || doc.IsTruncated === True;
            if (!truncated || !doc.NextContinuationToken.val()) {
                break;
            }
            if (scanned >= max_scan_keys) {
                bounded = True;
                break;
            }
            token = doc.NextContinuationToken;
        }

        # NOTE: reaching the bound is reported rather than passed over in silence; a truncated scan and a
        # bucket in which nothing changed produce exactly the same empty result otherwise
        if (bounded) {
            warn("watch on bucket %y%s stopped after examining %d keys, reaching the 'max_scan_keys' bound of "
                "%d; changes beyond that point were not seen this cycle. Narrow the watch with the 'prefix' "
                "option, or raise 'max_scan_keys'", watch_bucket,
                watch_prefix.val() ? sprintf(" prefix %y", watch_prefix) : "", scanned, max_scan_keys);
        }
        return rv;
    }

    private string getEventName() { return EVENT_NEW_OR_UPDATED_FILE; }
    private string getTimestampField() { return "last_modified"; }

    private string getEventDesc() {
        return "Fired when an object is created or updated in the bucket being watched.\n\n"
            "Amazon S3 has no change feed, so this is a poll of the bucket's key listing.  A deletion is not "
            "reported, and an object created and removed between two polls is not reported either.";
    }

    private AbstractDataProviderType getRecordDataType() {
        return AwsS3ObjectEventDataType;
    }

    private hash<auto> getFakeExampleData() {
        return {
            "id": "incoming/example.csv",
            "bucket": watch_bucket ?? "my-bucket",
            "key": "incoming/example.csv",
            "size": 1024,
            "last_modified": 2026-04-09T12:00:00Z,
            "etag": "d41d8cd98f00b204e9800998ecf8427e",
            "storage_class": "STANDARD",
        };
    }
}

#! Events container data provider
public class AwsS3EventsDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "AwsS3EventsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_observers": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            EVENT_NEW_BUCKET: Class::forName("AwsS3DataProvider::AwsS3WatchNewBucketDataProvider"),
            EVENT_NEW_OR_UPDATED_FILE: Class::forName("AwsS3DataProvider::AwsS3WatchObjectsDataProvider"),
        };
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, path_style);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
