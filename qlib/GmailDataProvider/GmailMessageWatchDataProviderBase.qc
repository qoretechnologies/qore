# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageWatchDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GmailDataProvider module
public namespace GmailDataProvider {
#! The parent class for Gmail REST APIs
public class GmailMessageWatchDataProviderBase inherits GoogleDataProvider::GoogleDataProviderBase,
        DataProvider::AbstractOrderedWatchDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "delete_messages": <DataProviderOptionInfo>{
                "display_name": "Delete Messages?",
                "short_desc": "Permanently delete each matched message after raising its event?",
                "type": (AbstractDataProviderTypeMap."bool",),
                "desc": "Permanently delete each matched message after raising its event?\n\n"
                    "**The message is deleted as soon as its event has been raised, whatever the event "
                    "consumer then does with it.** Event delivery is not consumer processing: a consumer "
                    "that fails, times out, or is restarted mid-processing has already lost the only copy "
                    "of the message, and a Gmail delete cannot be undone.\n\n"
                    "For any workload that must not lose the source message, leave this option unset and "
                    "delete from the consumer once processing has succeeded — every event carries the "
                    "`message_id` of its source message, and the `delete-message` and `trash-message` "
                    "actions take that ID",
            },
            "q": <DataProviderOptionInfo>{
                "display_name": "Search Criteria",
                "short_desc": "The gmail search criteria for matching messages",
                "desc": "The gmail search criteria for matching messages; ex: `in:inbox to:me@gmail.com "
                    "subject:hello is:unread`",
                "type": (AbstractDataProviderTypeMap."string",),
                "example_value": "in:inbox to:me@gmail.com subject:hello is:unread",
                "required": True,
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for matching emails in seconds",
                "type": (AbstractDataProviderTypeMap."int",),
                "desc": "How often to check for matching emails in seconds; lower bound 30; lower numbers will set "
                    "the minimum polling period",
                "default_value": 120,
            },
            "retrieve_attachments": <DataProviderOptionInfo>{
                "display_name": "Retrieve Attachments?",
                "short_desc": "Retrieve attachment data automatically?",
                "type": (AbstractDataProviderTypeMap."bool",),
                "desc": "Retrieve attachment data automatically?",
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Do not retrieve emails earlier than the given date",
                "type": (AbstractDataProviderTypeMap."date",),
                "desc": "Do not retrieve emails earlier than the given date",
            },
            "snapshot_mode": <DataProviderOptionInfo>{
                "display_name": "Latest Message Only?",
                "short_desc": "Collapse a backlog to its latest message instead of replaying it?",
                "type": (AbstractDataProviderTypeMap."bool",),
                "desc": "Collapse a backlog to its latest message instead of replaying it.\n\n"
                    "Some feeds are snapshots rather than a log: each message restates the current state, so "
                    "after an outage only the last one is worth processing. With this option set, each polling "
                    "cycle raises one event per group (see `snapshot_key`) carrying the newest matching message, "
                    "and reports the messages it collapsed away in the event's `superseded_count` and "
                    "`superseded_ids` fields. The messages that are collapsed away are never fetched in full, "
                    "so a large backlog costs one message retrieval, not one per message.\n\n"
                    "Leave this unset for a feed where every message must be processed",
            },
            "snapshot_key": <DataProviderOptionInfo>{
                "display_name": "Snapshot Grouping",
                "short_desc": "How messages are grouped before the latest one is selected",
                "type": (AbstractDataProviderTypeMap."string",),
                "desc": "How matching messages are grouped before the latest one in each group is selected; "
                    "ignored unless `snapshot_mode` is set.\n\n"
                    "- `all`: one global group, so each cycle raises a single event carrying the newest "
                    "matching message\n"
                    "- `thread`: group by Gmail thread, so each cycle raises one event per thread carrying that "
                    "thread's newest message",
                "default_value": SnapshotKeyAll,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": SnapshotKeyAll,
                        "display_name": "Latest Overall",
                        "desc": "Retain the single newest matching message",
                    },
                    <AllowedValueInfo>{
                        "value": SnapshotKeyThread,
                        "display_name": "Latest Per Thread",
                        "desc": "Retain the newest message in each Gmail thread",
                    },
                ),
            },
            "superseded_action": <DataProviderOptionInfo>{
                "display_name": "Superseded Message Action",
                "short_desc": "What to do with messages that were collapsed away",
                "type": (AbstractDataProviderTypeMap."string",),
                "desc": "What to do with the messages that `snapshot_mode` collapsed away; ignored unless "
                    "`snapshot_mode` is set.\n\n"
                    "- `ignore`: leave them in the mailbox, so they remain available for audit; they are not "
                    "raised as events again because the watch cursor has moved past them\n"
                    "- `trash`: move them to the Gmail trash, where they can still be recovered\n"
                    "- `delete`: delete them permanently; **this cannot be undone**\n\n"
                    "A removal that fails is logged and the message is left in place; it is not retried, "
                    "because its event has already been raised and the watch cursor has already moved past it",
                "default_value": WSA_IGNORE,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": WSA_IGNORE,
                        "display_name": "Keep",
                        "desc": "Leave superseded messages in the mailbox",
                    },
                    <AllowedValueInfo>{
                        "value": WSA_TRASH,
                        "display_name": "Move to Trash",
                        "desc": "Move superseded messages to the Gmail trash",
                    },
                    <AllowedValueInfo>{
                        "value": WSA_DELETE,
                        "display_name": "Delete Permanently",
                        "desc": "Delete superseded messages permanently",
                    },
                ),
            },
            "max_messages_per_cycle": <DataProviderOptionInfo>{
                "display_name": "Maximum Messages Per Cycle",
                "short_desc": "The maximum number of messages delivered in one polling cycle",
                "type": (AbstractDataProviderTypeMap."int",),
                "desc": "The maximum number of messages delivered in one polling cycle; unset or `0` means no "
                    "limit.\n\n"
                    "The limit is applied from the **oldest** end of the backlog, so the watch cursor still "
                    "advances contiguously and the remaining messages are delivered by the following cycles "
                    "rather than skipped. Use it to bound the work a single cycle can do when a large backlog "
                    "may accumulate",
            },
        };

        #! Snapshot grouping: retain the newest matching message overall
        const SnapshotKeyAll = "all";

        #! Snapshot grouping: retain the newest message in each Gmail thread
        const SnapshotKeyThread = "thread";

        #! Minimum poll interval in seconds
        const MinPollInterval = 30;

        #! Default poll interval in seconds
        const DefaultPollInterval = 120;

        #! For parsing months
        const MonthMap = {
            "jan": 1,
            "feb": 2,
            "mar": 3,
            "apr": 4,
            "may": 5,
            "jun": 6,
            "jul": 7,
            "aug": 8,
            "sep": 9,
            "oct": 10,
            "nov": 11,
            "dec": 12,
        };
    }

    private {
        #! Poll interval in seconds
        int poll_secs;

        #! Retrieve attachments
        bool retrieve_attachments;

        #! Delete emails after retreiving
        bool delete_messages;

        #! Start date for scan
        date start_date;

        #! Match query
        string q;

        #! Current epoch for polling (tracks latest message timestamp); 0 until the first message
        int poll_epoch = 0;

        #! Message IDs already processed at current epoch (deduplication)
        hash<string, bool> last_msg = {};

        #! How messages are grouped in snapshot mode
        string snapshot_key = SnapshotKeyAll;

        #! Setup info for polling (initialized on first poll)
        hash<auto> setup_info;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest,
            hash<string, hash<DataProviderOptionInfo>> constructor_options, *hash<auto> options)
            : GoogleDataProviderBase(rest),
              AbstractOrderedWatchDataProvider(GmailMessageWatchDataProviderBase::getPollInterval(
                  DataProviderDataContextHelper::getHash(){keys constructor_options} + options)) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", constructor_options,
            DataProviderDataContextHelper::getHash(){keys constructor_options} + options);
        processOptions(copts);
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Cancels any in-flight REST request during shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Process constructor options
    private processOptions(*hash<auto> copts) {
        poll_secs = GmailMessageWatchDataProviderBase::getPollInterval(copts);
        if (copts.retrieve_attachments) {
            retrieve_attachments = True;
        }
        if (copts.delete_messages) {
            delete_messages = True;
        }
        if (copts.start_date) {
            start_date = copts.start_date;
            poll_epoch = mktime(start_date);
        }
        if (copts.snapshot_key.val()) {
            if (copts.snapshot_key != SnapshotKeyAll && copts.snapshot_key != SnapshotKeyThread) {
                throw "CONSTRUCTOR-ERROR", sprintf("snapshot_key %y is unknown; expecting one of %y",
                    copts.snapshot_key, (SnapshotKeyAll, SnapshotKeyThread));
            }
            snapshot_key = copts.snapshot_key;
        }
        setWatchPolicy(copts.snapshot_mode ?? False, copts.superseded_action, copts.max_messages_per_cycle);

        q = copts.q;
    }

    #! Gmail lists messages newest-first, so the watch reverses them before delivery
    string getSourceOrdering() {
        return WSO_DESCENDING;
    }

    #! Returns the effective poll interval in seconds
    private static int getPollInterval(*hash<auto> opts) {
        return max(opts.poll_interval ?? DefaultPollInterval, MinPollInterval);
    }

    #! Setup for a poll operation
    private hash<auto> setup() {
        return {
            "msgtype": GmailMessageWatchDataProvider::getMessageEventType(),
        };
    }

    #! Returns the message type used to validate and convert retrieved messages
    private hash<auto> getSetupInfo() {
        if (!setup_info) {
            setup_info = setup();
        }
        return setup_info;
    }

    #! Returns the configuration that defines which mailbox scan this provider follows
    /** The search criteria select the messages the cursor is a position in, and the start date sets
        where that scan begins, so changing either defines a different scan and must start a new
        cursor.

        Everything else is deliberately excluded: the poll interval, attachment retrieval, message
        deletion, and the snapshot grouping key all change how the same scan is polled or what is done
        with a message once it has been delivered, not which messages the scan covers.  Including them
        would make an ordinary tuning change discard the cursor and redeliver the whole mailbox.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "q": q,
            "start_date": start_date,
        };
    }

    #! Restores the Gmail polling cursor
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.poll_epoch.typeCode() != NT_INT || checkpoint.poll_epoch < 0
                || checkpoint.last_message_ids.typeCode() != NT_LIST) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid Gmail watch checkpoint; expecting version 1, a non-negative "
                "poll_epoch, and last_message_ids";
        }
        hash<string, bool> restored_ids = {};
        foreach string message_id in (checkpoint.last_message_ids) {
            if (!message_id) {
                throw "WATCH-CHECKPOINT-ERROR", "invalid Gmail watch checkpoint; message IDs cannot be empty";
            }
            restored_ids{message_id} = True;
        }
        poll_epoch = checkpoint.poll_epoch;
        last_msg = restored_ids;
    }

    #! Durably advances the Gmail polling cursor
    private commitCheckpoint(int message_epoch, string message_id) {
        if (message_epoch < 0 || !message_id) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist a Gmail checkpoint with a negative epoch or empty "
                "message ID";
        }
        hash<string, bool> next_ids = message_epoch == poll_epoch ? last_msg : {};
        next_ids{message_id} = True;
        persistCheckpoint({
            "version": 1,
            "poll_epoch": message_epoch,
            "last_message_ids": keys next_ids,
        });
        poll_epoch = message_epoch;
        last_msg = next_ids;
    }

    #! Lists every message matching the watch query, following pagination
    /** @param gmail_query the Gmail search expression
        @param max_pages stop after this many pages; unset means walk every page

        @return the listed message stubs, newest first as Gmail returns them, or an empty list if
        shutdown interrupted the walk

        @note Every page in scope is walked by default. A partial list is not usable: Gmail lists
        newest-first, so stopping early yields the newest messages and delivering any of them would
        advance the watch cursor past older messages that were never listed. If shutdown interrupts
        the walk, this method therefore returns nothing at all and the cycle does no work.
    */
    private list<hash<auto>> listMessages(string gmail_query, *int max_pages) {
        list<hash<auto>> listed = ();
        *string page_token;
        int pages = 0;
        while (True) {
            # 500 is the maximum page size the Gmail API accepts; using it minimizes round trips
            string this_uri = "gmail/v1/users/me/messages?maxResults=500&q=" + encode_url(gmail_query);
            if (page_token.val()) {
                this_uri += "&pageToken=" + encode_url(page_token);
            }
            LoggerWrapper::debug("Gmail polling using URI: %y epoch: %y poll_interval: %ds", this_uri, poll_epoch,
                poll_secs);
            hash<auto> res = rest.restGet(this_uri).body;
            ++pages;
            if (res.messages) {
                listed += res.messages;
            }
            page_token = res.nextPageToken;
            if (!page_token.val()) {
                break;
            }
            if (isStopping()) {
                LoggerWrapper::debug("Gmail: shutting down during pagination after %d page(s); discarding the "
                    "partial list so the cursor cannot advance past unlisted messages", pages);
                return ();
            }
            if (max_pages && pages >= max_pages) {
                break;
            }
        }
        LoggerWrapper::debug("Gmail: listed %d message(s) over %d page(s)", listed.size(), pages);
        return listed;
    }

    #! Retrieves and normalizes one message in full
    private hash<auto> getFullMessage(string message_id) {
        hash<auto> msg = rest.restGet(sprintf("gmail/v1/users/me/messages/%s", message_id)).body;

        # try to decode body as base64 if there is a Content-Transfer-Encoding header
        # do not allow message bodies to be re-encoded as base64
        processHeaders(\msg.payload, GmailMessageEventDataType::Fields,
            GmailMessageEventDataType::PartFields);

        # check message structure and convert types where necessary
        msg = getSetupInfo().msgtype.acceptsValue(msg);

        # convert base64-decoded message bodies to strings if they have text MIME types
        processBody(\msg.payload);
        return msg;
    }

    #! Returns the message's position in the feed in seconds since the epoch
    private static int getMessageEpoch(auto internal_date) {
        return int(internal_date) / 1000;
    }

    #! Returns @ref True if the cycle needs a cheap ordering pass before fetching payloads
    /** Collapsing a backlog or bounding a batch means deciding which messages to deliver before
        paying for their payloads, which needs each message's timestamp up front. Without either
        option every listed message is delivered anyway, so the full retrieval doubles as the
        ordering pass and no extra API call is made.
    */
    private bool usesOrderingPass() {
        return hasSnapshotMode() || getMaxItemsPerCycle() > 0;
    }

    #! Returns the cycle's candidate messages
    private list<hash<WatchItemInfo>> fetchWatchItems() {
        string gmail_query = q;
        if (poll_epoch) {
            # Include the preceding second so messages sharing the checkpoint second can be
            # deduplicated by ID instead of being lost if Gmail applies a strict timestamp bound.
            gmail_query += sprintf(" after:%d", poll_epoch - 1);
        }
        bool ordering_pass = usesOrderingPass();
        list<hash<WatchItemInfo>> items = ();
        foreach hash<auto> minfo in (listMessages(gmail_query)) {
            # check for shutdown between REST calls
            if (isStopping()) {
                return ();
            }
            hash<auto> msg;
            if (ordering_pass) {
                # "minimal" returns the message's identity, thread, and internalDate without its
                # payload, so a message that is about to be collapsed away costs almost nothing
                msg = rest.restGet(sprintf("gmail/v1/users/me/messages/%s?format=minimal", minfo.id)).body;
            } else {
                msg = getFullMessage(minfo.id);
            }
            push items, <WatchItemInfo>{
                "id": msg.id,
                "order_key": GmailMessageWatchDataProviderBase::getMessageEpoch(msg.internalDate),
                "group_key": snapshot_key == SnapshotKeyThread ? msg.threadId : NOTHING,
                "data": ordering_pass ? NOTHING : msg,
            };
        }
        return items;
    }

    #! Returns @ref True if the message lies beyond the durable cursor
    private bool isNewWatchItem(hash<WatchItemInfo> item) {
        int msg_epoch = item.order_key;
        if (msg_epoch > poll_epoch) {
            return True;
        }
        if (msg_epoch == poll_epoch) {
            return !last_msg{item.id};
        }
        return False;
    }

    #! Retrieves the message payload, including attachment data when requested
    private auto fetchWatchItemPayload(hash<WatchItemInfo> item) {
        hash<auto> msg = item.data ?? getFullMessage(item.id);
        if (retrieve_attachments) {
            msg = addAttachments(msg);
            LoggerWrapper::info("Gmail: matched message %y attachments: %d", msg.id, msg.attachments.size());
        } else {
            LoggerWrapper::info("Gmail: matched message %y", msg.id);
        }
        return msg;
    }

    #! Retrieves and attaches the message's attachment data
    private hash<auto> addAttachments(hash<auto> msg) {
        msg.attachments = ();
        foreach hash<auto> part in (msg.payload.parts) {
            if (*string aid = part.body.attachmentId) {
                # retrieve attachment; skip on error to preserve the message event
                string atturi = sprintf("gmail/v1/users/me/messages/%s/attachments/%s", msg.id, aid);
                try {
                    hash<auto> att = rest.restGet(atturi).body;
                    msg.attachments += getAttachment(att, part.headers);
                } catch (hash<ExceptionInfo> att_ex) {
                    LoggerWrapper::warn("Gmail: failed to retrieve attachment %y for message %y: %s",
                        aid, msg.id, get_exception_string(att_ex));
                }
            }
        }
        return msg;
    }

    #! Raises the event for one message
    private dispatchWatchItem(hash<WatchItemInfo> item, auto payload, list<hash<WatchItemInfo>> superseded) {
        hash<auto> msg = payload;
        if (superseded) {
            msg += {
                "superseded_count": superseded.size(),
                "superseded_ids": map $1.id, superseded,
            };
        }
        messageReceived(msg);
    }

    #! Durably advances the Gmail cursor to the given message
    private advanceWatchCursor(hash<WatchItemInfo> item) {
        int msg_epoch = item.order_key;
        bool epoch_updated = msg_epoch > poll_epoch;
        commitCheckpoint(msg_epoch, item.id);
        if (epoch_updated) {
            LoggerWrapper::debug("Gmail using new epoch: %y", poll_epoch);
        }
    }

    #! Deletes the source message when the watch was configured to do so
    /** @note the message is deleted because its event was raised, which is not the same as the
        consumer having processed it; see @ref watch_removal_semantics
    */
    private afterWatchItemDispatched(hash<WatchItemInfo> item, auto payload) {
        if (delete_messages) {
            rest.restDel(sprintf("gmail/v1/users/me/messages/%s", item.id));
            LoggerWrapper::info("Gmail: deleted matched message %y", item.id);
        }
    }

    #! Trashes or deletes the messages that snapshot mode collapsed away
    /** A failure is logged per message and does not stop the others: the retained event has already
        been raised and its cursor committed, so nothing can be lost by continuing, and the message
        left behind is simply not raised again.
    */
    private applySupersededAction(list<hash<WatchItemInfo>> superseded) {
        string action = getSupersededAction();
        if (action == WSA_IGNORE) {
            return;
        }
        foreach hash<WatchItemInfo> item in (superseded) {
            try {
                if (action == WSA_TRASH) {
                    rest.restPost(sprintf("gmail/v1/users/me/messages/%s/trash", item.id), {});
                } else {
                    rest.restDel(sprintf("gmail/v1/users/me/messages/%s", item.id));
                }
                LoggerWrapper::info("Gmail: applied superseded action %y to message %y", action, item.id);
            } catch (hash<ExceptionInfo> ex) {
                LoggerWrapper::warn("Gmail: could not apply superseded action %y to message %y: %s; the message "
                    "is left in place and will not be raised again", action, item.id, get_exception_string(ex));
            }
        }
    }

    #! Returns one matching message for use as example event data, without delivering it
    private *hash<auto> getExampleMessage() {
        try {
            # one page is enough for an example
            foreach hash<auto> minfo in (listMessages(q, 1)) {
                hash<auto> msg = getFullMessage(minfo.id);
                return retrieve_attachments ? addAttachments(msg) : msg;
            }
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Gmail error retrieving example message: %s", get_exception_string(ex));
        }
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        LoggerWrapper::error("Gmail error retrieving emails: %s; polling again in %ds", get_exception_string(ex),
            poll_secs);
    }

    #! Logs an informational message about a polling cycle
    private logWatchInfo(string fmt, ...) {
        LoggerWrapper::logArgs(LoggerLevel::INFO, "Gmail: " + fmt, argv);
    }

    #! Process headers and body
    private processHeaders(*reference<hash<auto>> msg, hash<auto> fields, *hash<auto> pfields) {
        # reformat message to make it more user-friendly
        msg.headers = map {
            $1.name: getHeaderValue($1.value, fields{$1.name}),
        }, msg.headers, $1.value.val();

        for (int i = 0; i < msg.parts.size(); ++i) {
            processHeaders(\msg.parts[i], pfields ?? fields);
        }
    }

    #! Process body
    private processBody(*reference<hash<auto>> msg) {
        # headers / body
        if (msg.headers."Content-Type".val() && msg.body.size) {
            bool text = is_mime_text(msg.headers."Content-Type");
            if (text && (msg.body."data".typeCode() == NT_BINARY)) {
                msg.body."data" = msg.body."data".toString();
            }
        }

        for (int i = 0; i < msg.parts.size(); ++i) {
            processBody(\msg.parts[i]);
        }
    }

    #! Get header value
    private static auto getHeaderValue(string val, hash<auto> f) {
        if (f.type == AbstractDataProviderTypeMap."date") {
            val =~ s/^[^,]+,\s*//;
            val =~ s/ +/ /g;
            list<string> l = val.split(" ");
            # Expect at least: day mon year time tz (5 tokens)
            if (l.size() < 5) {
                return;
            }
            if (*int m = MonthMap{l[1].lwr()}) {
                return date(sprintf("%04d-%02d-%02dT%s %s", l[2], m, l[0], l[3], l[4]));
            }
            return;
        }
        if (f.type instanceof ListDataType) {
            return val.split(",", "\'", True);
        }
        return val;
    }

    #! Get header value
    private static auto getHeaderValue(string val) {
        return val;
    }

    #! Returns attachment data
    private hash<auto> getAttachment(hash<auto> att, *hash<auto> hdr) {
        string filename;
        # try to get filename
        if (hdr."Content-Disposition" && (*string fn =
            (hdr."Content-Disposition" =~ x/filename="([^"]+)"/)[0])) {
            filename = fn;
        }
        # try to determine the content type
        *string content_type = hdr."Content-Type";
        *string encoding;
        if (content_type.val()) {
            encoding = (content_type =~ x/charset="([^"]+)"/)[0];
            content_type =~ s/;.*//;
        } else if (filename.val()) {
            content_type = get_mime_type_from_ext(filename);
        }
        # get filename from content type, if necessary
        if (!filename.val() && content_type.val() && (*string fn = (content_type =~ x/name=([^;]+)/)[0]).val()) {
            filename = fn;
        }
        data body;
        if (content_type.val() && is_mime_text(content_type)) {
            body = parse_base64_url_string_to_string(att."data", encoding);
        } else {
            body = parse_base64_url_string(att."data");
        }
        hash<auto> attachment = {
            "hdr": hdr,
            "body": body,
        };
        if (filename.val()) {
            attachment.filename = filename;
        }
        if (content_type) {
            attachment.content_type = content_type;
        }
        if (encoding) {
            attachment.encoding = encoding;
        }
        return attachment;
    }

    #! Raise events
    abstract private messageReceived(hash<auto> msg);
}
}
