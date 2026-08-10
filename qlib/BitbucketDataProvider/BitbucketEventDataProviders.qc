# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BitbucketDataProvider module
public namespace BitbucketDataProvider {
#! The minimum polling interval in seconds
public const BitbucketMinPollInterval = 15;

#! The default polling interval in seconds
public const BitbucketDefaultPollInterval = 60;

#! The most pages a single polling cycle will read
/** Bitbucket offers no date filter on any of these collections, so a cycle reads from the newest record
    backwards and stops once it is past the cursor.  This bounds the first cycle of a repository with
    years of history, which would otherwise read every page before delivering anything.
*/
public const BitbucketMaxPollPages = 5;

#! Base class for Bitbucket polling watch data providers
/** Bitbucket's REST API has no event stream a credential can read, so every event provider here polls a
    collection positioned by a record timestamp.

    @section bitbucketwatch_improvement What This Improves On

    The application this module replaces kept the identities it had already delivered in a bounded
    in-memory set, so a restart re-delivered whatever was still in the first page.
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" positions
    the feed by a timestamp instead and keeps only the identities recorded at exactly the cursor to break
    ties, so delivery is at-least-once across restarts when the host supplies checkpoint persistence, and
    the cursor only advances once an event has been raised successfully.

    @section bitbucketwatch_nobackfill No Date Filter Upstream

    None of these collections accepts a "changed since" filter, so a cycle reads the newest records and
    relies on the cursor to discard what it has already delivered.  @ref BitbucketMaxPollPages bounds how
    far back a single cycle reads.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to
    the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation
    as unsupported.
*/
public class BitbucketWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Bitbucket event provider
        const WatchConstructorOptions = {
            "workspace": <DataProviderOptionInfo>{
                "display_name": "Workspace",
                "short_desc": "The workspace holding the repository to watch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The workspace holding the repository to watch, as its slug or its UUID in braces",
                "required": True,
                "preselected": True,
                "ref_data": BitbucketManifest::RefWorkspace,
                "allowed_values_creatable": True,
                "has_dependents": True,
                "on_change": ("refetch",),
            },
            "repo_slug": <DataProviderOptionInfo>{
                "display_name": "Repository",
                "short_desc": "The repository to watch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The repository to watch, as its slug or its UUID in braces",
                "required": True,
                "preselected": True,
                "ref_data": BitbucketManifest::RefRepository,
                "depends_on": ("workspace",),
                "allowed_values_creatable": True,
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Bitbucket, in seconds (minimum " + BitbucketMinPollInterval
                    + ")",
                "default_value": BitbucketDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records created at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records created at or after this time.\n\n"
                    "Without it the cursor starts at the moment the subscription starts, so nothing "
                    "already in the repository is delivered",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *BitbucketRestClient::BitbucketRestClientIo rest;

        #! The workspace being watched
        /** Optional here rather than required, because the provider is also constructed as a **template**
            when the application's children are enumerated, before anybody has chosen one.  It is required
            to subscribe, which is checked in @ref observersReady().
        */
        *string workspace;

        #! The repository being watched
        *string repo_slug;
    }

    #! Creates the object
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval
                ?? BitbucketDefaultPollInterval, BitbucketMinPollInterval), options.since) {
        self.rest = rest;
        if (exists options.workspace && options.workspace.toString().val()) {
            workspace = options.workspace.toString();
        }
        if (exists options.repo_slug && options.repo_slug.toString().val()) {
            repo_slug = options.repo_slug.toString();
        }
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Starts polling, once the options needed to poll at all are present
    /** @throw BITBUCKET-EVENT-ERROR a required option is missing
    */
    observersReady() {
        checkWatchOptions();
        AbstractTimestampWatchDataProvider::observersReady();
    }

    #! Verifies that the options needed to poll are present
    /** @throw BITBUCKET-EVENT-ERROR a required option is missing
    */
    private checkWatchOptions() {
        if (!workspace.val()) {
            throw "BITBUCKET-EVENT-ERROR", "the \"workspace\" option is required; it names the workspace "
                "holding the repository to watch";
        }
        if (!repo_slug.val()) {
            throw "BITBUCKET-EVENT-ERROR", "the \"repo_slug\" option is required; it names the repository "
                "to watch";
        }
    }

    #! Cancels an in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! Returns the configuration that defines which scan this provider follows
    /** The repository scopes the feed, so watching a different one is a different scan and the previous
        cursor must not be resumed against it.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return (AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() ?? {}) + {
            "workspace": workspace,
            "repo_slug": repo_slug,
        } + (getWatchFilterIdentity() ?? {});
    }

    #! Returns any further options that decide which records the feed returns
    private *hash<auto> getWatchFilterIdentity() {
    }

    #! Returns the repository path segment
    private string getRepoPath() {
        checkWatchOptions();
        return sprintf("%s/repositories/%s/%s", BitbucketRestClient::BitbucketRestClientBase::ApiPrefix,
            workspace, repo_slug);
    }

    #! Fetches records from a Bitbucket collection, newest first
    /** @param path the request path
        @param params the query arguments, without paging

        @return the records read

        @note Bitbucket returns every collection as \c {values, next, …} and offers no date filter here,
        so this reads from the newest record backwards and stops at @ref BitbucketMaxPollPages; the cursor
        discards whatever has already been delivered.
    */
    private list<hash<auto>> getRecords(string path, *hash<auto> params) {
        if (!rest) {
            throw "BITBUCKET-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        int pagelen = BitbucketRestClient::BitbucketRestClientBase::MaxPageLen;
        list<hash<auto>> rv = ();
        int page = 1;
        while (page <= BitbucketMaxPollPages) {
            if (isStopping()) {
                break;
            }
            hash<auto!> args = (params ?? {}) + {"page": page, "pagelen": pagelen};
            list<string> qargs = ();
            foreach hash<auto> i in (args.pairIterator()) {
                if (exists i.value) {
                    qargs += sprintf("%s=%s", i.key, encode_url(i.value.toString(), True));
                }
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(path + "?" + join("&", qargs));
            auto values = resp.body.values;
            if (!exists values) {
                break;
            }
            if (values.typeCode() != NT_LIST) {
                throw "BITBUCKET-EVENT-ERROR", sprintf("expected a list in \"values\" from %y, but got "
                    "type %y", path, values.type());
            }
            foreach auto rec in (values) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
            if (!resp.body.next.val() || values.size() < pagelen) {
                break;
            }
            ++page;
        }
        return rv;
    }

    #! Returns the event this provider raises, and the shape of what it carries
    /** The type is the schema's own record type for the collection the provider polls, taken from the
        action set this application already ships.
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc() ?? getEventName(),
                "type": getEventRecordType(getEventSourceAction()),
            },
        };
    }

    #! Returns the action whose records this provider reports
    abstract private string getEventSourceAction();

    #! Returns the record type an action's listing returns
    private AbstractDataProviderType getEventRecordType(string action) {
        RestSchemaActionSet aset = BitbucketSchema::getActionSet();
        *RestSchemaActionOperation op = aset.getOperation(action);
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                # a Bitbucket collection is an envelope; the records are in "values"
                *hash<string, AbstractDataField> fields = t.getFields();
                if (fields.values) {
                    *AbstractDataProviderType vt = fields.values.getType();
                    if (vt && vt.getBaseTypeCode() == NT_LIST) {
                        *AbstractDataProviderType et = vt.getElementType();
                        if (et) {
                            return et;
                        }
                    }
                }
                return t;
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }
}

#! Raised for each new commit in the watched repository
public class BitbucketNewCommitDataProvider inherits BitbucketWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_commit";

        #! The event display name
        const DisplayName = "New Commit";

        #! The event short description
        const ShortDesc = "Fires when a commit is pushed to the watched repository";

        #! The event description
        const Desc = "Fires for each new commit in the watched repository, across every branch.\n\n"
            "The event carries the commit record Bitbucket returns, including its hash, message, author "
            "and date.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "BitbucketNewCommitDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest, *hash<auto> options)
            : BitbucketWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "repositories_workspace_repo_slug_commits_get";
    }

    #! A commit is identified by its hash
    private string getIdField() {
        return "hash";
    }

    #! A commit's position in the feed is its date, not a \c created_at
    private string getTimestampField() {
        return "date";
    }

    #! Fetches the most recent commits
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords(getRepoPath() + "/commits");
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new deployment in the watched repository
public class BitbucketNewDeploymentDataProvider inherits BitbucketWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_deployment";

        #! The event display name
        const DisplayName = "New Deployment";

        #! The event short description
        const ShortDesc = "Fires when a deployment is created for the watched repository";

        #! The event description
        const Desc = "Fires for each new deployment in the watched repository.\n\n"
            "The event carries the deployment record Bitbucket returns, including the environment it "
            "went to, the state it reached and the release it deployed.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "BitbucketNewDeploymentDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest, *hash<auto> options)
            : BitbucketWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getDeploymentsForRepository";
    }

    #! A deployment is identified by its UUID
    private string getIdField() {
        return "uuid";
    }

    #! Returns a deployment's position in the feed
    /** A deployment carries no timestamp of its own: the only one in the record is the creation time of
        the release it deployed, so that is the position.  A deployment with no release has no position
        and is skipped rather than delivered out of order.
    */
    private *date getWatchRecordTimestamp(hash<auto> record) {
        auto val = record.release.created_on;
        if (val.typeCode() == NT_STRING && val.val()) {
            try {
                date parsed = date(val);
                return parsed.absolute() ? parsed : NOTHING;
            } catch (hash<ExceptionInfo> ex) {
                # ex: INVALID-DATE; the record simply has no position in the feed
                return;
            }
        }
        return val.typeCode() == NT_DATE && val.absolute() ? val : NOTHING;
    }

    #! Fetches the most recent deployments
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords(getRepoPath() + "/deployments");
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new pull request in the watched repository
public class BitbucketNewPullRequestDataProvider inherits BitbucketWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_pull_request";

        #! The event display name
        const DisplayName = "New Pull Request";

        #! The event short description
        const ShortDesc = "Fires when a pull request is opened in the watched repository";

        #! The event description
        const Desc = "Fires for each new pull request in the watched repository.\n\n"
            "`state` restricts which pull requests are watched, and defaults to `OPEN`. The event carries "
            "the pull request record Bitbucket returns, including its source and destination branches, "
            "author and reviewers.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "BitbucketNewPullRequestDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Only report pull requests in this state",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only report pull requests in this state: `OPEN`, `MERGED`, `DECLINED` or "
                    "`SUPERSEDED`; `OPEN` when unset",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "OPEN", "desc": "open pull requests"},
                    <AllowedValueInfo>{"value": "MERGED", "desc": "merged pull requests"},
                    <AllowedValueInfo>{"value": "DECLINED", "desc": "declined pull requests"},
                    <AllowedValueInfo>{"value": "SUPERSEDED", "desc": "superseded pull requests"},
                ),
            },
        };
    }

    private {
        #! Report only pull requests in this state, if set
        *string state;
    }

    #! Creates the object
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest, *hash<auto> options)
            : BitbucketWatchDataProviderBase(rest, options) {
        state = options.state;
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the state filter that decides which pull requests the feed returns
    private *hash<auto> getWatchFilterIdentity() {
        return {"state": state};
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "repositories_workspace_repo_slug_pullrequests_get";
    }

    #! A pull request's position in the feed is when it was opened
    private string getTimestampField() {
        return "created_on";
    }

    #! Fetches the most recent pull requests
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords(getRepoPath() + "/pullrequests", state.val() ? {"state": state} : NOTHING);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the Bitbucket event providers
public class BitbucketEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": BitbucketDataProvider::EventsChildName,
            "type": "BitbucketEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            BitbucketNewCommitDataProvider::EventName: Class::forName("BitbucketNewCommitDataProvider"),
            BitbucketNewDeploymentDataProvider::EventName:
                Class::forName("BitbucketNewDeploymentDataProvider"),
            BitbucketNewPullRequestDataProvider::EventName:
                Class::forName("BitbucketNewPullRequestDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *BitbucketRestClient::BitbucketRestClientIo rest;
    }

    #! Creates the object
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return BitbucketDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Bitbucket event providers";
    }

    #! Returns the event names
    private *list<string> getChildProviderNamesImpl() {
        return keys EventMap;
    }

    #! Returns the given event provider or @ref nothing if the event is unknown
    /** The child returned here is a **template**: it carries the event's option declarations, and the
        values are supplied when a subscription is created.
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = EventMap{name};
        if (!cls) {
            return;
        }
        return cast<AbstractDataProvider>(cls.newObject(rest));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
