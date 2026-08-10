# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! Raised when an issue is opened
public class GitHubNewIssueDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        /** The ID the superseded application registered, which is what existing workflows reference.
        */
        const EventName = "new_repository_issue";

        #! The event display name
        const DisplayName = "New Issue";

        #! The event short description
        const ShortDesc = "Fires when an issue is opened in a repository";

        #! The event description
        const Desc = "Fires when an issue is opened.\n\n"
            "The delivery carries the `issue` itself, the `repository` and the `sender` who opened it.\n\n"
            "GitHub's `issues` event also covers editing, closing, labelling and assigning; those are not "
            "raised unless **All Actions** is set, in which case read the payload's `action` to tell them "
            "apart.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "issues";
    }

    #! Returns the payload action this event raises
    *string getEventAction() {
        return "opened";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-issue-event", (
            {"name": "action", "desc": "the change that occurred, ex: \"opened\"", "type": "string"},
            {"name": "issue", "desc": "the issue", "component": "issue"},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when a pull request is opened
public class GitHubNewPullRequestDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_pull_request";

        #! The event display name
        const DisplayName = "New Pull Request";

        #! The event short description
        const ShortDesc = "Fires when a pull request is opened";

        #! The event description
        const Desc = "Fires when a pull request is opened.\n\n"
            "The delivery carries the `pull_request`, its `number`, the `repository` and the `sender`.\n\n"
            "GitHub's `pull_request` event also covers closing, merging, reopening, review requests and "
            "every push to the branch (`synchronize`); those are not raised unless **All Actions** is set.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "pull_request";
    }

    #! Returns the payload action this event raises
    *string getEventAction() {
        return "opened";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-pull-request-event", (
            {"name": "action", "desc": "the change that occurred, ex: \"opened\"", "type": "string"},
            {"name": "number", "desc": "the pull request number", "type": "int"},
            {"name": "pull_request", "desc": "the pull request", "component": "pull-request"},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when a review is submitted on a pull request
/** @note The superseded application called this event a review **request**, and its description said so,
    but it subscribes to GitHub's \c pull_request_review event and its payload carries a \c review — so what
    it actually reports is a review being **given**.  The name is a product commitment and is kept; the
    presentation is corrected here rather than the behaviour, because the payload is what existing workflows
    read.  A review being *requested* is the \c pull_request event with action \c review_requested, which
    **New Pull Request** raises with **All Actions** set.
*/
public class GitHubNewReviewDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_review_request";

        #! The event display name
        const DisplayName = "New Pull Request Review";

        #! The event short description
        const ShortDesc = "Fires when a review is submitted on a pull request";

        #! The event description
        const Desc = "Fires when someone submits a review on a pull request.\n\n"
            "The delivery carries the `review` — its state is `approved`, `changes_requested` or "
            "`commented` — together with the `pull_request` it is on, the `repository` and the `sender`.\n\n"
            "**This is a review being given, not one being asked for**, despite the event's ID: the "
            "application this module replaces named it `new_review_request` while subscribing to reviews. "
            "The ID is kept because existing workflows use it. For a review being *requested*, use **New "
            "Pull Request** with **All Actions** set and match `action` against `review_requested`.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "pull_request_review";
    }

    #! Returns the payload action this event raises
    *string getEventAction() {
        return "submitted";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-pull-request-review-event", (
            {"name": "action", "desc": "the change that occurred, ex: \"submitted\"", "type": "string"},
            # GitHub does not publish the review payload as a reusable component schema
            {"name": "review", "desc": "the review that was submitted", "type": "hash"},
            {"name": "pull_request", "desc": "the pull request reviewed", "component": "pull-request-simple"},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when commits are pushed to a repository
public class GitHubNewCommitDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_commit";

        #! The event display name
        const DisplayName = "New Commit";

        #! The event short description
        const ShortDesc = "Fires when commits are pushed to a repository";

        #! The event description
        const Desc = "Fires on every push to the repository.\n\n"
            "**One delivery can carry many commits**: `commits` holds those in the push, up to GitHub's "
            "limit of 20, and `head_commit` is the last of them. `ref` says which branch or tag was "
            "pushed to, and `before` and `after` bound the range.\n\n"
            "A push that deletes a branch also arrives here, with `deleted` set and `after` all zeroes.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "push";
    }

    #! Returns the payload action this event raises
    /** A push delivery carries no \c action property.
    */
    *string getEventAction() {
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-push-event", (
            {"name": "ref", "desc": "the fully qualified ref pushed to, ex: \"refs/heads/main\"",
                "type": "string"},
            {"name": "before", "desc": "the SHA the ref pointed at before the push", "type": "string"},
            {"name": "after", "desc": "the SHA the ref points at after the push", "type": "string"},
            {"name": "created", "desc": "whether the push created the ref", "type": "bool",
                "optional": True},
            {"name": "deleted", "desc": "whether the push deleted the ref", "type": "bool",
                "optional": True},
            {"name": "forced", "desc": "whether the push was forced", "type": "bool", "optional": True},
            {"name": "compare", "desc": "a URL comparing the range that was pushed", "type": "string",
                "optional": True},
            # GitHub's push payload carries its own commit shape rather than the "commit" component the
            # REST API returns, so it is left open rather than typed as something it is not
            {"name": "commits", "desc": "the commits in the push, newest last", "type": "list",
                "optional": True},
            {"name": "head_commit", "desc": "the last commit in the push", "type": "hash",
                "optional": True},
            {"name": "pusher", "desc": "the account that pushed", "type": "hash", "optional": True},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when a branch is created
public class GitHubNewBranchDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_repository_branch";

        #! The event display name
        const DisplayName = "New Branch";

        #! The event short description
        const ShortDesc = "Fires when a branch is created in a repository";

        #! The event description
        const Desc = "Fires when a branch is created.\n\n"
            "GitHub's `create` event covers **branches and tags alike** and says which through `ref_type`; "
            "this event raises only branches, because that is what it is called. Set **All Actions** to "
            "receive tag creations as well, and read `ref_type` to tell them apart.\n\n"
            "The application this module replaces reported a new tag as a new branch.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "create";
    }

    #! Returns the payload action this event raises
    /** A \c create delivery carries no \c action property; \c ref_type is what separates a branch from a
        tag, and @ref acceptsDelivery() uses it.
    */
    *string getEventAction() {
    }

    #! Returns @ref True if the delivery created a branch rather than a tag
    private bool acceptsDelivery(hash<auto> event) {
        if (all_actions) {
            return True;
        }
        return event.ref_type == "branch";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-create-event", (
            {"name": "ref", "desc": "the branch or tag that was created", "type": "string"},
            {"name": "ref_type", "desc": "\"branch\" or \"tag\"", "type": "string"},
            {"name": "master_branch", "desc": "the repository's default branch", "type": "string",
                "optional": True},
            {"name": "description", "desc": "the repository description", "type": "string",
                "optional": True},
            {"name": "pusher_type", "desc": "\"user\", or the name of an integration", "type": "string",
                "optional": True},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when a release is published
public class GitHubNewReleaseDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_release";

        #! The event display name
        const DisplayName = "New Release";

        #! The event short description
        const ShortDesc = "Fires when a release is published";

        #! The event description
        const Desc = "Fires when a release is published.\n\n"
            "A **draft** release does not raise this event when it is created; it raises it when it is "
            "published, which is what makes `published` the right moment to act on rather than `created`.\n\n"
            "Set **All Actions** to receive editing, deleting and pre-release transitions as well.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "release";
    }

    #! Returns the payload action this event raises
    *string getEventAction() {
        return "published";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-release-event", (
            {"name": "action", "desc": "the change that occurred, ex: \"published\"", "type": "string"},
            {"name": "release", "desc": "the release", "component": "release"},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! Raised when a commit is commented on
public class GitHubNewCommitCommentDataProvider inherits GitHubBaseEventDataProvider {
    public {
        #! The event name
        const EventName = "new_commit_comment";

        #! The event display name
        const DisplayName = "New Commit Comment";

        #! The event short description
        const ShortDesc = "Fires when a comment is made on a commit";

        #! The event description
        const Desc = "Fires when a comment is made directly on a commit.\n\n"
            "This is a comment on a **commit**, not on an issue or in a pull request review — those are "
            "separate GitHub events this application does not subscribe to. The delivery carries the "
            "`comment`, with the `commit_id` it is attached to and, for a comment on a diff line, the "
            "`path` and `position`.";
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options)
            : GitHubBaseEventDataProvider(rest, options) {
    }

    #! Returns the GitHub event this provider subscribes to
    string getGitHubEvent() {
        return "commit_comment";
    }

    #! Returns the payload action this event raises
    *string getEventAction() {
        return "created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EventName;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return Desc;
    }

    #! Returns the type of the payload this event carries
    AbstractDataProviderType getEventType() {
        return GitHubEventDataType::getPayloadType("github-commit-comment-event", (
            {"name": "action", "desc": "the change that occurred, ex: \"created\"", "type": "string"},
            # GitHub does not publish the commit comment payload as a reusable component schema
            {"name": "comment", "desc": "the comment that was made", "type": "hash"},
        ) + GitHubEventDataType::getCommonFields());
    }
}

#! The container holding the GitHub event providers
public class GitHubEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": GitHubDataProvider::EventsChildName,
            "type": "GitHubEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            GitHubNewIssueDataProvider::EventName: Class::forName("GitHubNewIssueDataProvider"),
            GitHubNewPullRequestDataProvider::EventName: Class::forName("GitHubNewPullRequestDataProvider"),
            GitHubNewReviewDataProvider::EventName: Class::forName("GitHubNewReviewDataProvider"),
            GitHubNewCommitDataProvider::EventName: Class::forName("GitHubNewCommitDataProvider"),
            GitHubNewBranchDataProvider::EventName: Class::forName("GitHubNewBranchDataProvider"),
            GitHubNewReleaseDataProvider::EventName: Class::forName("GitHubNewReleaseDataProvider"),
            GitHubNewCommitCommentDataProvider::EventName:
                Class::forName("GitHubNewCommitCommentDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *GitHubRestClient::GitHubRestClientIo rest;
    }

    #! Creates the object
    constructor(*GitHubRestClient::GitHubRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return GitHubDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub webhook event providers";
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
