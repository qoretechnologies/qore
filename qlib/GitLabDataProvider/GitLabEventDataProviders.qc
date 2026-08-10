# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitLabDataProvider module
public namespace GitLabDataProvider {
#! The minimum polling interval in seconds
/** GitLab.com allows 2000 authenticated requests a minute per user, so the limit here is not the API but
    the pointlessness of polling a repository more often than commits arrive.
*/
public const GitLabMinPollInterval = 10;

#! The default polling interval in seconds
public const GitLabDefaultPollInterval = 60;

#! Base class for GitLab polling watch data providers
/** GitLab's REST API has no event stream, so every event provider here polls a listing positioned by a
    record timestamp.

    @section gitlabwatch_improvement What This Improves On

    The application this module replaces polled the same listings, but kept the identities it had already
    delivered in a bounded in-memory set, so a restart re-delivered whatever was still in the first page
    and a busy repository could push an item out of the set and deliver it twice.

    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" positions the
    feed by \c created_at instead, keeps only the identities recorded **at exactly** the cursor timestamp to
    break ties, and makes the cursor durable when the host supplies persistence — so delivery is
    at-least-once across restarts rather than best-effort. It also dispatches in ascending order and only
    advances the cursor after an item has been delivered successfully, so a consumer that throws gets the
    item again instead of losing it.

    Every provider filters server-side on the cursor where GitLab offers a filter for it, so a poll costs
    one small request rather than a walk through the whole collection.

    @note The watch base is inherited **first**, deliberately.  Both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to the
    first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation as
    unsupported — so listing the data provider base first would leave every subscription failing with
    \c UNIMPLEMENTED.
*/
public class GitLabWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every GitLab event provider
        const WatchConstructorOptions = {
            "project": <DataProviderOptionInfo>{
                "display_name": "Project",
                "short_desc": "The project to watch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The project to watch, given as its numeric ID or as its URL-encoded path "
                    "(`group%2Fproject`)",
                "required": True,
                "preselected": True,
                "ref_data": GitLabManifest::RefProject,
                "allowed_values_creatable": True,
                "has_dependents": True,
                "on_change": ("refetch",),
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll GitLab, in seconds (minimum " + GitLabMinPollInterval + ")",
                "default_value": GitLabDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records created at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records created at or after this time.\n\n"
                    "Without it the cursor starts at the moment the subscription starts, so nothing "
                    "already in the repository is delivered — set it to backfill",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *GitLabRestClient::GitLabRestClientIo rest;

        #! The project being watched, as given
        /** Optional here rather than required, because the provider is also constructed as a **template**
            when the application's children are enumerated, before anybody has chosen a project.  It is
            required to subscribe, which is checked in @ref observersReady().
        */
        *string project;
    }

    #! Creates the object
    /** @param rest the REST client to use for API calls
        @param options the provider options
    */
    constructor(*GitLabRestClient::GitLabRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval ?? GitLabDefaultPollInterval,
                GitLabMinPollInterval), options.since) {
        self.rest = rest;
        if (exists options.project && options.project.toString().val()) {
            project = options.project.toString();
        }
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Returns the project being watched, if one was given
    *string getProject() {
        return project;
    }

    #! Starts polling, once the options needed to poll at all are present
    /** @throw GITLAB-EVENT-ERROR a required option is missing
    */
    observersReady() {
        checkWatchOptions();
        AbstractTimestampWatchDataProvider::observersReady();
    }

    #! Verifies that the options needed to poll are present
    /** @throw GITLAB-EVENT-ERROR a required option is missing
    */
    private checkWatchOptions() {
        if (!project.val()) {
            throw "GITLAB-EVENT-ERROR", "the \"project\" option is required; it names the project to watch";
        }
    }

    #! Cancels an in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! Returns the configuration that defines which scan this provider follows
    /** The project scopes the feed, so watching a different project is a different scan and the previous
        cursor must not be resumed against it.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return (AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() ?? {}) + {
            "project": project,
        } + (getWatchFilterIdentity() ?? {});
    }

    #! Returns any further options that decide which records the feed returns
    /** Overridden by a provider with a filter of its own — a branch, a commit — so that changing it starts
        a new cursor rather than resuming the old one against a different selection.
    */
    private *hash<auto> getWatchFilterIdentity() {
    }

    #! Returns the project path segment, URL-encoded
    /** A project given as \c "group/project" has to be encoded, or the slash addresses a path segment that
        is not there.
    */
    private string getProjectSegment() {
        checkWatchOptions();
        return encode_url(project, True);
    }

    #! Fetches every page of a GitLab listing
    /** @param path the request path, with the project already substituted
        @param params the query arguments, without paging

        @return every record the listing returned

        @note GitLab reports paging in response **headers** rather than in the body, and the body is the
        bare array, so paging ends when a page comes back shorter than the page size.
    */
    private list<hash<auto>> getAllPages(string path, *hash<auto> params) {
        if (!rest) {
            throw "GITLAB-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        list<hash<auto>> rv = ();
        int page = 1;
        while (True) {
            if (isStopping()) {
                break;
            }
            hash<auto!> args = (params ?? {}) + {
                "page": page,
                "per_page": GitLabRestClient::GitLabRestClientBase::MaxPerPage,
            };
            string query = foldl $1 + "&" + $2, (map sprintf("%s=%s", $1.key, encode_url($1.value.toString(),
                True)), args.pairIterator(), exists $1.value);
            hash<RestClient::RestResponseInfo> resp = rest.restGet(path + "?" + query);
            auto body = resp.body;
            if (body.typeCode() != NT_LIST) {
                throw "GITLAB-EVENT-ERROR", sprintf("expected a list from %y, but got type %y", path,
                    body.type());
            }
            foreach auto rec in (body) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
            if (body.size() < GitLabRestClient::GitLabRestClientBase::MaxPerPage) {
                break;
            }
            ++page;
        }
        return rv;
    }

    #! Returns the event this provider raises, and the shape of what it carries
    /** The type is the **schema's own** record type for the listing the provider polls, taken from the
        action set this application already ships.  An event that carried an untyped hash would make a
        consumer guess at the fields, and a second hand-written copy of the record shape would drift from
        the schema the actions use.
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
        RestSchemaActionSet aset = GitLabSchema::getActionSet();
        *RestSchemaActionOperation op = aset.getOperation(action);
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                # a listing publishes a list of records; the event carries one of them
                if (t.getBaseTypeCode() == NT_LIST) {
                    *AbstractDataProviderType et = t.getElementType();
                    if (et) {
                        return et;
                    }
                } else {
                    return t;
                }
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Formats a timestamp the way GitLab's date filters take it
    private static string formatSince(date since) {
        return format_date("YYYY-MM-DDTHH:mm:ssZ", since);
    }
}

#! Raised for each new commit in the watched project
public class GitLabNewCommitDataProvider inherits GitLabWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_commit";

        #! The event display name
        const DisplayName = "New Commit";

        #! The event short description
        const ShortDesc = "Fires when a commit is pushed to the watched project";

        #! The event description
        const Desc = "Fires for each new commit in the watched project.\n\n"
            "Restrict it to one branch or tag with `ref_name`, to commits touching one file or "
            "directory with `path`, or to one author with `author`. The event carries the commit "
            "record GitLab returns, including its message, author and web URL.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "GitLabNewCommitDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "ref_name": <DataProviderOptionInfo>{
                "display_name": "Branch or Tag",
                "short_desc": "Watch this branch or tag instead of the default branch",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The branch or tag to watch; the project's default branch when unset",
                "ref_data": GitLabManifest::RefBranch,
                "depends_on": ("project",),
                "allowed_values_creatable": True,
                "preselected": True,
            },
            "path": <DataProviderOptionInfo>{
                "display_name": "Path",
                "short_desc": "Only report commits touching this file or directory",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only report commits that touch this file or directory",
            },
            "author": <DataProviderOptionInfo>{
                "display_name": "Author",
                "short_desc": "Only report commits by this author",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only report commits whose author name matches this value",
            },
            "withStats": <DataProviderOptionInfo>{
                "display_name": "With Stats",
                "short_desc": "Include the lines added and removed by each commit",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Include the lines added and removed by each commit in the event",
            },
        };
    }

    private {
        #! The branch or tag being watched, if any
        *string ref_name;

        #! Report only commits touching this path, if set
        *string path;

        #! Report only commits by this author, if set
        *string author;

        #! Include commit stats?
        bool with_stats;
    }

    #! Creates the object
    constructor(*GitLabRestClient::GitLabRestClientIo rest, *hash<auto> options)
            : GitLabWatchDataProviderBase(rest, options) {
        ref_name = options.ref_name;
        path = options.path;
        author = options.author;
        with_stats = options.withStats.toBool();
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raised for each new commit in the watched project";
    }

    #! Returns the filters that decide which commits the feed returns
    private *hash<auto> getWatchFilterIdentity() {
        return {
            "ref_name": ref_name,
            "path": path,
            "author": author,
        };
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getApiV4ProjectsIdRepositoryCommits";
    }

    #! Fetches the commits created at or after the cursor
    /** GitLab filters commits server-side with \c since, so a poll costs one small request even in a
        repository with a long history.
    */
    private list<hash<auto>> fetchRecords(date since) {
        return getAllPages(sprintf("/api/v4/projects/%s/repository/commits", getProjectSegment()), {
            "since": GitLabWatchDataProviderBase::formatSince(since),
            "ref_name": ref_name,
            "path": path,
            "author": author,
            "with_stats": with_stats ? True : NOTHING,
        });
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new comment on the watched commit
public class GitLabNewCommitCommentDataProvider inherits GitLabWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_commit_comment";

        #! The event display name
        const DisplayName = "New Commit Comment";

        #! The event short description
        const ShortDesc = "Fires when a comment is added to the watched commit";

        #! The event description
        const Desc = "Fires for each new comment on one commit.\n\n"
            "The commit is a required option, so this watches the comments on that commit rather "
            "than every comment in the project. The event carries the note, its position in the diff "
            "when it has one, and its author.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "GitLabNewCommitCommentDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "commit": <DataProviderOptionInfo>{
                "display_name": "Commit",
                "short_desc": "The commit whose comments are watched",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The commit whose comments are watched, given as a SHA, a branch name or a tag "
                    "name",
                "required": True,
                "preselected": True,
                "ref_data": GitLabManifest::RefCommit,
                "depends_on": ("project",),
                "allowed_values_creatable": True,
            },
        };
    }

    private {
        #! The commit whose comments are watched
        /** Optional for the same reason as the project; required to subscribe.
        */
        *string commit;
    }

    #! Creates the object
    constructor(*GitLabRestClient::GitLabRestClientIo rest, *hash<auto> options)
            : GitLabWatchDataProviderBase(rest, options) {
        if (exists options.commit && options.commit.toString().val()) {
            commit = options.commit.toString();
        }
    }

    #! Verifies that the options needed to poll are present
    private checkWatchOptions() {
        GitLabWatchDataProviderBase::checkWatchOptions();
        if (!commit.val()) {
            throw "GITLAB-EVENT-ERROR", "the \"commit\" option is required; it names the commit whose "
                "comments are watched";
        }
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raised for each new comment on the watched commit";
    }

    #! Returns the commit that scopes the feed
    private *hash<auto> getWatchFilterIdentity() {
        return {"commit": commit};
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getApiV4ProjectsIdRepositoryCommitsShaComments";
    }

    #! Fetches the comments on the watched commit
    /** GitLab offers no date filter here, so every comment on the commit is read each cycle and the
        cursor discards the ones already delivered.  A commit's comment list is small, so this is cheap.
    */
    private list<hash<auto>> fetchRecords(date since) {
        string segment = getProjectSegment();
        return getAllPages(sprintf("/api/v4/projects/%s/repository/commits/%s/comments", segment,
            encode_url(commit, True)));
    }

    #! Returns a comment's stable identity
    /** A commit comment is the one record in this application with no identifier of its own: GitLab's
        commit-comment listing returns the note, its position in the diff, its author and its timestamp,
        and nothing else.

        A digest of exactly those fields is therefore the identity.  Two genuinely distinct comments agree
        on all of them only if the same author posted the same text at the same place in the same second,
        in which case delivering one event is the better failure: the alternative — treating them as
        distinct — would make every poll re-deliver both, because the cursor could never advance past a
        tie it cannot break.
    */
    private *string getWatchRecordId(hash<auto> record) {
        return SHA256(sprintf("%s\n%s\n%s\n%s\n%s", record.created_at ?? "", record.author.id ?? "",
            record.path ?? "", record.line ?? "", record.note ?? ""));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new issue in the watched project
public class GitLabNewIssueDataProvider inherits GitLabWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_issue";

        #! The event display name
        const DisplayName = "New Issue";

        #! The event short description
        const ShortDesc = "Fires when an issue is raised in the watched project";

        #! The event description
        const Desc = "Fires for each new issue in the watched project.\n\n"
            "The event carries the issue record GitLab returns, including its IID, title, labels, "
            "assignees and web URL. An issue that is edited later does not fire this event again; it "
            "reports creation only.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "GitLabNewIssueDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*GitLabRestClient::GitLabRestClientIo rest, *hash<auto> options)
            : GitLabWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raised for each new issue in the watched project";
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getApiV4ProjectsIdIssues";
    }

    #! Fetches the issues created at or after the cursor
    private list<hash<auto>> fetchRecords(date since) {
        return getAllPages(sprintf("/api/v4/projects/%s/issues", getProjectSegment()), {
            "created_after": GitLabWatchDataProviderBase::formatSince(since),
            "order_by": "created_at",
            "sort": "asc",
        });
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new merge request in the watched project
public class GitLabNewMergeRequestDataProvider inherits GitLabWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_merge_request";

        #! The event display name
        const DisplayName = "New Merge Request";

        #! The event short description
        const ShortDesc = "Fires when a merge request is opened in the watched project";

        #! The event description
        const Desc = "Fires for each new merge request in the watched project.\n\n"
            "Restrict it to one target branch with `target_branch`, which is how a release branch is "
            "watched without hearing about every other proposal. The event carries the merge request "
            "record GitLab returns, including its IID, source and target branches and web URL.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "GitLabNewMergeRequestDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "target_branch": <DataProviderOptionInfo>{
                "display_name": "Target Branch",
                "short_desc": "Only report merge requests targeting this branch",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only report merge requests whose target branch is this one",
                "ref_data": GitLabManifest::RefBranch,
                "depends_on": ("project",),
                "allowed_values_creatable": True,
            },
        };
    }

    private {
        #! Report only merge requests targeting this branch, if set
        *string target_branch;
    }

    #! Creates the object
    constructor(*GitLabRestClient::GitLabRestClientIo rest, *hash<auto> options)
            : GitLabWatchDataProviderBase(rest, options) {
        target_branch = options.target_branch;
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raised for each new merge request in the watched project";
    }

    #! Returns the branch filter that decides which merge requests the feed returns
    private *hash<auto> getWatchFilterIdentity() {
        return {"target_branch": target_branch};
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getApiV4ProjectsIdMergeRequests";
    }

    #! Fetches the merge requests created at or after the cursor
    private list<hash<auto>> fetchRecords(date since) {
        return getAllPages(sprintf("/api/v4/projects/%s/merge_requests", getProjectSegment()), {
            "created_after": GitLabWatchDataProviderBase::formatSince(since),
            "target_branch": target_branch,
            "order_by": "created_at",
            "sort": "asc",
        });
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the GitLab event providers
public class GitLabEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": GitLabDataProvider::EventsChildName,
            "type": "GitLabEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            GitLabNewCommitDataProvider::EventName: Class::forName("GitLabNewCommitDataProvider"),
            GitLabNewCommitCommentDataProvider::EventName:
                Class::forName("GitLabNewCommitCommentDataProvider"),
            GitLabNewIssueDataProvider::EventName: Class::forName("GitLabNewIssueDataProvider"),
            GitLabNewMergeRequestDataProvider::EventName:
                Class::forName("GitLabNewMergeRequestDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *GitLabRestClient::GitLabRestClientIo rest;
    }

    #! Creates the object
    constructor(*GitLabRestClient::GitLabRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return GitLabDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitLab event providers";
    }

    #! Returns the event names
    private *list<string> getChildProviderNamesImpl() {
        return keys EventMap;
    }

    #! Returns the given event provider or @ref nothing if the event is unknown
    /** The child returned here is a **template**: it carries the event's option declarations, and the
        values are supplied when a subscription is created.  That is why the project is not required to
        construct one — see @ref GitLabDataProvider::GitLabWatchDataProviderBase "GitLabWatchDataProviderBase".
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
