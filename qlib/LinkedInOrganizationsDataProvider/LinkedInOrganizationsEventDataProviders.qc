# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsEventDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInOrganizationsDataProvider module
public namespace LinkedInOrganizationsDataProvider {
#! Constructor options for the new-post watch provider
public const NewPostWatchConstructorOptions = LinkedInOrganizationsDataProviderBase::ConstructorOptions
    + LinkedInOrganizationField + {
    "organization": LinkedInOrganizationField."organization" + <DataProviderOptionInfo>{
        "required": True,
        "ref_data": "organizations",
    },
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
            + DefaultPollInterval + "`).\n\n"
            "LinkedIn rate-limits the feed API to **one request per 60 seconds** and serves data "
            "that can be up to 48 hours old, so polling faster than the minimum cannot see anything "
            "sooner and earns HTTP 429s.",
        "default_value": DefaultPollInterval,
    },
};

#! Watch provider for posts published by an organization
/** @par Why this is not a pure cursor resume

    The DMA feed finder does hand back a \c paginationCursor, and it is used here — but it is
    **not** a resume token in the sense a delta feed's is.  \c postsByAuthor is sorted by creation
    time **descending**, so its cursor walks from the newest post backwards through history:
    following a cursor saved on a previous poll returns *older* posts, never newer ones, and a
    trigger built that way would silently stop firing after the first tick.

    What this provider does instead is use the cursor for what it is good at — walking back through
    the feed **within one poll** — and remember the newest post it has already delivered.  Each poll
    reads the head of the feed and follows the cursor until it reaches that post, so a burst of
    posts between two polls is still delivered in full without re-reading the whole history, and a
    quiet period costs exactly one request.
*/
public class LinkedInNewPostDataProvider inherits LinkedInOrganizationsDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_POST,
            "type": "LinkedInNewPostDataProvider",
            "supports_observable": True,
            "constructor_options": NewPostWatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! The maximum number of feed pages read in one poll
        /** A bound on the work a single tick can do: without it, a page that had its checkpoint
            post deleted would walk the organization's entire history, at one rate-limited request
            per page.
        */
        const MaxPollPages = 10;

        #! The number of post URNs requested per feed page
        const PollPageSize = 50;
    }

    private {
        #! The organization whose feed is watched
        string organization;

        #! The URN of the newest post already delivered
        *string last_urn;

        #! Flag to track first poll (seed the checkpoint without firing events)
        bool first_poll = True;
    }

    #! Creates the object from a REST client and options
    constructor(LinkedInRestClient::LinkedInRestClientIo rest, *hash<auto> options)
            : LinkedInOrganizationsDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        organization = LinkedInNewPostDataProvider::requireOrganization(options);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : LinkedInOrganizationsDataProviderBase(options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        organization = LinkedInNewPostDataProvider::requireOrganization(options);
    }

    #! Returns the organization option or throws
    private static string requireOrganization(*hash<auto> options) {
        *string rv = options.organization;
        if (!rv.val()) {
            throw "CONSTRUCTOR-ERROR", "missing required 'organization' option for the LinkedIn "
                "new-post trigger";
        }
        return rv;
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Events are raised oldest first
    /** The feed is read newest-first and reversed before delivery, so a consumer sees a burst of
        posts in the order they were published.
    */
    string getSourceOrdering() {
        return WSO_ASCENDING;
    }

    #! The stream is identified by the organization whose feed is followed
    /** Keyed on the organization alone, so the checkpoint survives an unrelated option edit such as
        a change of poll interval.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "organization": organization,
        };
    }

    #! Restores the newest delivered post URN
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.last_urn.typeCode() != NT_STRING || !checkpoint.last_urn.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid LinkedIn watch checkpoint; expecting version 1 "
                "and a non-empty last_urn";
        }
        last_urn = checkpoint.last_urn;
        # a restored checkpoint means the baseline has already been established
        first_poll = False;
    }

    #! Durably advances the newest delivered post URN
    private commitCheckpoint(string urn) {
        if (!urn.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist an empty LinkedIn post URN";
        }
        persistCheckpoint({
            "version": 1,
            "last_urn": urn,
        });
        last_urn = urn;
    }

    #! Polls once, advancing the checkpoint only after every post has been delivered
    /** @note The checkpoint is stored only after every post it supersedes has been raised.  Storing
        it first would skip the whole batch if delivery then failed; storing it afterwards means a
        failure re-reads the batch, so delivery is at-least-once.
    */
    private pollOnce() {
        try {
            list<string> fresh = collectNewPostUrns();
            if (!fresh) {
                return;
            }
            # fresh is newest-first; the newest is the checkpoint this poll will commit to
            string newest = fresh[0];

            if (first_poll) {
                # on the first poll the checkpoint is seeded without firing events, so that turning
                # the trigger on does not replay the organization's entire history
                first_poll = False;
                commitCheckpoint(newest);
                return;
            }

            # deliver oldest first, so a consumer sees a burst in publication order
            list<string> ordered = reverse(fresh);
            hash<auto> posts = batchGet(PostsPath, ordered, {"viewContext": "READER"});
            foreach string urn in (ordered) {
                if (isStopping()) {
                    # the checkpoint stays where it is, so the remaining posts are read again
                    return;
                }
                *hash<auto> post = posts{urn};
                if (!post) {
                    # the feed named a post the Posts API would not return; nothing useful can be
                    # delivered for it, and skipping it must not stall the checkpoint
                    warn("LinkedIn post %y was named by the feed but not returned by the Posts API; "
                        "skipping it", urn);
                    continue;
                }
                notifyObservers(EVENT_NEW_POST, LinkedInOrganizationsUtil::normalizePost(post));
            }

            # the batch has been delivered, so the checkpoint can be made durable
            commitCheckpoint(newest);
        } catch (hash<ExceptionInfo> ex) {
            # NOTE: "LoggerWrapper::error()", not a bare "error()"; AbstractDataProvider declares
            # "error(string err, string fmt, ...)", which throws rather than logging and wins
            # overload resolution, so a bare call would raise an exception from the handler
            # reporting the error
            LoggerWrapper::error(sprintf("Error polling for %s events: %s: %s", EVENT_NEW_POST,
                ex.err, ex.desc));
        }
    }

    #! Returns the URNs of posts newer than the checkpoint, newest first
    /** Walks the feed from its head, following the cursor, until the checkpointed post is reached
        or the feed runs out.  On the first poll only the head of the feed is read: there is nothing
        to catch up to yet, and the whole history would be discarded anyway.
    */
    private list<string> collectNewPostUrns() {
        list<string> rv = ();
        *string cursor;
        int pages = 0;
        while (True) {
            if (isStopping()) {
                return ();
            }
            hash<auto> page = doCursorFinder(FeedContentsPath, {
                "q": "postsByAuthor",
                "author": LinkedInOrgActionProviderBase::organizationUrn(organization),
            }, PollPageSize, cursor);

            foreach hash<auto> element in (page.elements) {
                *string urn = element.id;
                if (!urn.val()) {
                    continue;
                }
                if (last_urn.val() && urn == last_urn) {
                    # everything from here on has already been delivered
                    return rv;
                }
                push rv, urn;
            }

            cursor = page.cursor;
            # an absent cursor is the documented end of the collection; a short page is not
            if (!cursor.val() || first_poll) {
                break;
            }
            if (++pages >= MaxPollPages) {
                warn("stopped after %d page(s) looking for LinkedIn post %y in the feed of "
                    "organization %y; it may have been deleted.  %d post(s) will be delivered and "
                    "the checkpoint moved forward", MaxPollPages, last_urn, organization, rv.size());
                break;
            }
        }
        return rv;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_POST: <DataProviderMessageInfo>{
                "desc": "Fired when the organization publishes a post",
                "type": LinkedInOrgPostDataType,
            },
        };
    }

    #! Returns the organization's most recent post as example event data
    /** A real post is fetched so the example matches what the trigger actually delivers; a static
        example stands in when the organization has never posted.
    */
    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> page = doCursorFinder(FeedContentsPath, {
                "q": "postsByAuthor",
                "author": LinkedInOrgActionProviderBase::organizationUrn(organization),
            }, 1, NOTHING);
            *string urn = page.elements[0].id;
            if (urn.val()) {
                hash<auto> posts = batchGet(PostsPath, (urn,), {"viewContext": "READER"});
                if (*hash<auto> post = posts{urn}) {
                    return LinkedInOrganizationsUtil::normalizePost(post);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getExampleEventData: error: %s: %s", ex.err, ex.desc);
        }
        return LinkedInOrganizationsEventExamples::getPostExample();
    }
}

#! Container for the LinkedIn Organizations watch providers
public class LinkedInOrganizationsEventsDataProvider inherits LinkedInOrganizationsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "LinkedInOrganizationsEventsDataProvider",
            "supports_children": True,
            "constructor_options": LinkedInOrganizationsDataProviderBase::ConstructorOptions,
            "children_can_support_observers": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            EVENT_NEW_POST: Class::forName(
                "LinkedInOrganizationsDataProvider::LinkedInNewPostDataProvider"),
        };

        #! The options this provider was created with, passed on to the watch providers
        *hash<auto> child_options;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest, *hash<auto> options)
            : LinkedInOrganizationsDataProviderBase(rest) {
        child_options = options;
    }

    constructor(*hash<auto> options) : LinkedInOrganizationsDataProviderBase(options) {
        child_options = options;
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

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
        return cls.newObject(rest, child_options);
    }
}

#! Static example event data for the LinkedIn Organizations watch providers
public class LinkedInOrganizationsEventExamples {
    #! Returns an example post, used when the organization has never posted
    static hash<auto> getPostExample() {
        return LinkedInOrganizationsUtil::normalizePost({
            "id": "urn:li:share:1188488118",
            "commentary": "We are hiring! Take a look at the roles open on our team.",
            "visibility": "PUBLIC",
            "lifecycleState": "PUBLISHED",
            "isReshareDisabledByAuthor": False,
            "publishedAt": 1763375562669,
            "distribution": {
                "feedDistribution": "MAIN_FEED",
                "thirdPartyDistributionChannels": (),
            },
            "lifecycleStateInfo": {
                "isEditedByAuthor": False,
            },
            "created": {
                "actor": "urn:li:organization:2414183",
                "time": 1763375562669,
            },
            "lastModified": {
                "actor": "urn:li:organization:2414183",
                "time": 1763375562669,
            },
        });
    }
}
}
