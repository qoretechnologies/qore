# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapWatchDataProviders.qc event-generating data providers that react to new mail

/*  ImapWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ImapClientDataProvider {
#! Event raised when a message matches the watch criteria
public const EVENT_IMAP_MESSAGE_MATCHED = "IMAP-MESSAGE-MATCHED";

#! Event raised when an attachment matches the watch criteria
public const EVENT_IMAP_ATTACHMENT_MATCHED = "IMAP-ATTACHMENT-MATCHED";

#! Base class for the IMAP watch data providers
/** Reacts to new mail. When the server advertises \c IDLE the connection waits for the server to push
    mailbox changes, so a new message is seen as soon as it arrives; otherwise the mailbox is searched on
    each poll interval. Either way the same search criteria decide what matches, so the two modes differ
    only in latency.

    Messages are tracked by UID, which is stable for the lifetime of the mailbox's \c uidvalidity value,
    so a message is reported exactly once even across reconnects. If the server resets \c uidvalidity the
    watch starts again from the current end of the mailbox rather than re-reporting the whole mailbox.

    @since %ImapClientDataProvider 1.0
*/
public class ImapWatchDataProviderBase inherits ImapClientDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Constructor options shared by the watch providers
        const WatchOptions = ImapClientDataProviderBase::ConstructorOptions + {
            "mailbox": <DataProviderOptionInfo>{
                "display_name": "Mailbox",
                "short_desc": "The mailbox to watch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox to watch for new messages",
                "default_value": "INBOX",
                "example_value": "INBOX",
                "preselected": True,
            },
            "criteria": <DataProviderOptionInfo>{
                "display_name": "Search Criteria",
                "short_desc": "The IMAP search criteria for matching messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The IMAP search criteria that decide which new messages are reported.\n\n"
                    "Matching happens on the server, so messages that do not match are never "
                    "transferred; for example `UNSEEN FROM \"orders@example.com\"` reacts only to "
                    "unread mail from one sender",
                "default_value": "ALL",
                "example_value": "UNSEEN",
                "preselected": True,
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for new messages in seconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "How often to check for new messages, in seconds.\n\n"
                    "On servers that support `IDLE` this only bounds how long the connection waits "
                    "before checking again; new mail is still reported as soon as the server pushes it",
                "default_value": 60,
            },
            "mark_seen": <DataProviderOptionInfo>{
                "display_name": "Mark as Seen?",
                "short_desc": "Set the \\Seen flag on reported messages?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Set the `\\Seen` flag on each reported message.\n\n"
                    "Useful with an `UNSEEN` criterion so that a message is not considered again after "
                    "it has been handled",
                "default_value": False,
            },
            "delete_messages": <DataProviderOptionInfo>{
                "display_name": "Delete Messages?",
                "short_desc": "Delete reported messages?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Flag each reported message `\\Deleted` and expunge it.\n\n"
                    "**This removes mail from the server permanently**; prefer `mark_seen` or a move to "
                    "another mailbox unless the mailbox is a queue that is meant to be drained",
                "default_value": False,
            },
        };
    }

    #! @cond nodoc
    private {
        #! the mailbox being watched
        string mailbox;

        #! the search criteria used to match messages
        string criteria;

        #! set the \Seen flag on reported messages
        bool mark_seen = False;

        #! delete reported messages after reporting them
        bool delete_messages = False;

        #! the highest UID already reported; -1 until the first poll establishes the baseline
        int last_uid = -1;

        #! the mailbox's UID validity value when the baseline was established
        *int uidvalidity;

        #! True once IDLE has been entered on the connection
        bool idling = False;
    }
    #! @endcond

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<string, hash<DataProviderOptionInfo>> con_opts = WatchOptions)
            : ImapClientDataProviderBase(options, con_opts),
              AbstractWatchDataProviderBase(options.poll_interval ?? 60) {
        setWatchOptions(options);
    }

    #! Creates the object from an existing IMAP client
    /** Used when the provider is acquired as a child of the root provider, which supplies the
        connection; the watch options then come from the action's own option values.
    */
    constructor(ImapClient::ImapClient imap, *hash<auto> options)
            : ImapClientDataProviderBase(imap),
              AbstractWatchDataProviderBase(options.poll_interval ?? 60) {
        setWatchOptions(options);
    }

    #! @cond nodoc
    private setWatchOptions(*hash<auto> options) {
        mailbox = options.mailbox ?? "INBOX";
        criteria = options.criteria ?? "ALL";
        mark_seen = options.mark_seen ?? False;
        delete_messages = options.delete_messages ?? False;
    }
    #! @endcond

    #! Returns the mailbox being watched
    string getMailbox() {
        return mailbox;
    }

    #! Returns the search criteria
    string getCriteria() {
        return criteria;
    }

    #! @cond nodoc
    #! Cancels any in-flight polling operation so the polling thread can exit promptly
    private cancelPollOperation() {
        if (idling) {
            try {
                imap.stopIdle();
            } catch (hash<ExceptionInfo> ex) {
                # the connection is being torn down; nothing useful can be done here
            }
            idling = False;
        }
        imap.forceDisconnect();
    }

    #! Performs one polling cycle
    private pollOnce() {
        hash<ImapClient::ImapMailboxStatus> status = imap.selectMailbox(mailbox);

        # a uidvalidity change means the server reassigned UIDs; the stored baseline no longer refers
        # to the same messages, so start again from the current end of the mailbox
        if (exists uidvalidity && status.uidvalidity != uidvalidity) {
            logInfo("mailbox %y uidvalidity changed from %y to %y; resetting the watch baseline",
                mailbox, uidvalidity, status.uidvalidity);
            last_uid = -1;
        }
        uidvalidity = status.uidvalidity;

        if (last_uid < 0) {
            # establish the baseline without reporting the mail that was already there
            list<int> existing = imap.uidSearch("ALL");
            last_uid = existing ? existing.last() : 0;
            logInfo("watching mailbox %y from UID %d with criteria %y", mailbox, last_uid, criteria);
        } else {
            processNewMessages();
        }

        # Wait for the server to push a change when it can. A pushed update is swept immediately
        # rather than on the next cycle: the base class sleeps for the poll interval after pollOnce()
        # returns, so returning here without sweeping would delay every pushed message by up to that
        # interval — exactly what IDLE exists to avoid.
        if (waitForChange()) {
            processNewMessages();
        }
    }

    #! Reports every message newer than the baseline that matches the criteria
    /** @return the number of messages reported

        @note The baseline is advanced and the follow-up actions applied only for messages that were
        reported successfully. Advancing past a message whose delivery failed would drop it
        permanently on a transient error, and flagging or deleting a message that observers never
        received would destroy mail that was never handled — so a failed message is left in place and
        retried on the next cycle.
    */
    private int processNewMessages() {
        string search = sprintf("UID %d:* %s", last_uid + 1, criteria);
        list<int> uids = imap.uidSearch(search);

        int reported = 0;
        bool expunge_needed = False;
        foreach int uid in (uids) {
            # "UID n:*" is inclusive of the baseline on some servers, so filter explicitly
            if (uid <= last_uid) {
                continue;
            }
            try {
                reportMessage(uid);
            } catch (hash<ExceptionInfo> ex) {
                logPollError(ex);
                # leave the baseline where it is so this message is retried
                continue;
            }
            ++reported;
            # max(): stay correct even if a server ever returns UIDs out of order
            if (uid > last_uid) {
                last_uid = uid;
            }

            if (mark_seen) {
                imap.markSeen(uid);
            }
            if (delete_messages) {
                imap.markDeleted(uid);
                expunge_needed = True;
            }
        }
        if (expunge_needed) {
            imap.expunge();
        }
        return reported;
    }

    #! Waits for the server to report a mailbox change, when it supports doing so
    /** @return @ref True if the server reported a change that warrants an immediate sweep

        @note the wait is bounded by the poll interval here, and
        @ref ImapClient::ImapClient::waitForIdleEvent() "waitForIdleEvent()" bounds it again by the
        time left in the RFC 2177 window, re-issuing \c IDLE when that expires — so a long
        \c poll_interval cannot hold a session open past the 29-minute limit
    */
    private bool waitForChange() {
        if (!imap.hasCapability("IDLE")) {
            # the base class sleeps for the poll interval before calling pollOnce() again
            return False;
        }
        bool changed = False;
        try {
            if (!idling) {
                imap.startIdle();
                idling = True;
            }
            # a timeout simply means nothing arrived; the next poll cycle sweeps anyway
            *hash<ImapClient::ImapIdleEvent> event =
                imap.waitForIdleEvent(getPollIntervalSeconds() * 1000);
            # EXISTS and RECENT announce new mail; other updates (ex: EXPUNGE, FLAGS) do not add
            # messages, so they do not warrant a sweep
            if (event && (event.type == "EXISTS" || event.type == "RECENT")) {
                changed = True;
            }
        } catch (hash<ExceptionInfo> ex) {
            logPollError(ex);
        }
        # leave IDLE so the next cycle can issue commands on the connection
        if (idling) {
            try {
                list<hash<ImapClient::ImapIdleEvent>> trailing = imap.stopIdle();
                # updates reported while leaving IDLE count too
                foreach hash<ImapClient::ImapIdleEvent> ev in (trailing) {
                    if (ev.type == "EXISTS" || ev.type == "RECENT") {
                        changed = True;
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                logPollError(ex);
            }
            idling = False;
        }
        return changed;
    }

    #! Logs an error raised during a polling cycle
    private logPollError(hash<ExceptionInfo> ex) {
        logError("error watching mailbox %y: %s: %s", mailbox, ex.err, ex.desc);
    }

    private logInfo(string msg, ...) {
        if (logger) {
            logArgs(LoggerLevel::INFO, "ImapWatch: " + msg, argv);
        }
    }

    private logError(string msg, ...) {
        if (logger) {
            logArgs(LoggerLevel::ERROR, "ImapWatch: " + msg, argv);
        }
    }

    private logDbg(string msg, ...) {
        if (logger) {
            logArgs(LoggerLevel::DEBUG, "ImapWatch: " + msg, argv);
        }
    }
    #! @endcond

    #! Reports a single matching message
    /** @param uid the UID of the message to report
    */
    abstract private reportMessage(int uid);
}

#! Raises an event for every new message matching the search criteria
/** @since %ImapClientDataProvider 1.0
*/
public class ImapMessageWatchDataProvider inherits ImapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapMessageWatchDataProvider",
            "constructor_options": WatchOptions,
            "supports_observable": True,
            "events": {
                EVENT_IMAP_MESSAGE_MATCHED: <DataProviderMessageInfo>{
                    "desc": "Raised when a new message matching the search criteria arrives",
                    "type": ImapMessageEventType,
                },
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "watch",
            "desc": "Raises an event for every new message matching the search criteria",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapWatchDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap, *hash<auto> options)
            : ImapWatchDataProviderBase(imap, options) {
    }

    #! Returns the data provider name
    string getName() {
        return "watch";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raises an event for every new message in the watched mailbox that matches the search "
            "criteria";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the event types raised by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_IMAP_MESSAGE_MATCHED: <DataProviderMessageInfo>{
                "desc": "Raised when a new message matching the search criteria arrives",
                "type": ImapMessageEventType,
            },
        };
    }

    #! Returns example event data
    /** Returns a real message from the watched mailbox when one is available, so the example
        reflects the account's actual mail; falls back to representative data when the mailbox is
        empty or unreachable.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (event_id == EVENT_IMAP_MESSAGE_MATCHED) {
            try {
                imap.examineMailbox(mailbox);
                list<int> uids = imap.uidSearch(criteria);
                if (uids) {
                    return ImapClientDataProvider::describeMessage(imap, uids.last()) + {
                        "mailbox": mailbox,
                    };
                }
            } catch (hash<ExceptionInfo> ex) {
                logDbg("could not retrieve example event data: %s: %s", ex.err, ex.desc);
            }
            return ImapMessageWatchDataProvider::getFakeExampleData(mailbox);
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }

    #! @cond nodoc
    #! Returns representative event data for when no live message is available
    private static hash<auto> getFakeExampleData(string mailbox) {
        return {
            "mailbox": mailbox,
            "uid": 4392,
            "subject": "Order confirmation #10583",
            "from": "orders@example.com",
            "to": ("purchasing@example.com",),
            "date": 2026-07-25T09:14:22Z,
            "message_id": "<10583.20260725@example.com>",
            "flags": ("\\Recent",),
            "body": "Your order #10583 has been confirmed and will ship on 2026-07-28.",
            "attachments": (
                {"name": "invoice-10583.pdf", "mime": "application/pdf", "size": 24576},
            ),
            "raw": NOTHING,
        };
    }

    private reportMessage(int uid) {
        hash<auto> msg = ImapClientDataProvider::describeMessage(imap, uid) + {
            "mailbox": mailbox,
        };
        notifyObservers(EVENT_IMAP_MESSAGE_MATCHED, msg);
    }
    #! @endcond
}

#! Raises an event for every attachment on a new message matching the search criteria
/** @since %ImapClientDataProvider 1.0
*/
public class ImapAttachmentWatchDataProvider inherits ImapWatchDataProviderBase {
    public {
        #! Constructor options
        const AttachmentWatchOptions = ImapWatchDataProviderBase::WatchOptions + {
            "attachment_glob": <DataProviderOptionInfo>{
                "display_name": "Attachment File Pattern",
                "short_desc": "An optional filename pattern for matching attachments",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "An optional filename pattern (glob) for matching attachments.\n\n"
                    "When unset, every attachment is reported; when set, only attachments whose "
                    "filename matches are reported, so a mailbox carrying mixed content can be "
                    "narrowed to just the files of interest (ex: `*.csv`)",
                "example_value": "*.csv",
                "preselected": True,
            },
            "ignore_case": <DataProviderOptionInfo>{
                "display_name": "Ignore Pattern Case?",
                "short_desc": "Match the filename pattern case-insensitively?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Match the filename pattern case-insensitively; ignored when "
                    "`attachment_glob` is not set",
                "default_value": True,
            },
            "retrieve_attachments": <DataProviderOptionInfo>{
                "display_name": "Retrieve Attachment Data?",
                "short_desc": "Include the attachment content in the event?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Include the decoded attachment content in the event.\n\n"
                    "Leave this `False` to receive only the attachment metadata, which keeps events "
                    "small when attachments are large; the content can then be retrieved on demand "
                    "with the `get-attachment` action",
                "default_value": True,
            },
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapAttachmentWatchDataProvider",
            "constructor_options": AttachmentWatchOptions,
            "supports_observable": True,
            "events": {
                EVENT_IMAP_ATTACHMENT_MATCHED: <DataProviderMessageInfo>{
                    "desc": "Raised for each attachment on a new message matching the search criteria",
                    "type": ImapAttachmentEventType,
                },
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "watch-attachment",
            "desc": "Raises an event for every attachment on a new matching message",
        });
    }

    #! @cond nodoc
    private {
        #! optional filename pattern for matching attachments
        *string attachment_glob;

        #! match the filename pattern case-insensitively
        bool ignore_case = True;

        #! include the decoded attachment content in the event
        bool retrieve_attachments = True;
    }
    #! @endcond

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapWatchDataProviderBase(options, AttachmentWatchOptions) {
        setAttachmentOptions(options);
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap, *hash<auto> options)
            : ImapWatchDataProviderBase(imap, options) {
        setAttachmentOptions(options);
    }

    #! @cond nodoc
    private setAttachmentOptions(*hash<auto> options) {
        attachment_glob = options.attachment_glob;
        ignore_case = options.ignore_case ?? True;
        retrieve_attachments = options.retrieve_attachments ?? True;
    }
    #! @endcond

    #! Returns the data provider name
    string getName() {
        return "watch-attachment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Raises an event for every attachment on a new message in the watched mailbox that "
            "matches the search criteria";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the event types raised by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_IMAP_ATTACHMENT_MATCHED: <DataProviderMessageInfo>{
                "desc": "Raised for each attachment on a new message matching the search criteria",
                "type": ImapAttachmentEventType,
            },
        };
    }

    #! Returns example event data
    /** Returns a real attachment from the watched mailbox when one is available; falls back to
        representative data when none is found or the mailbox is unreachable.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (event_id == EVENT_IMAP_ATTACHMENT_MATCHED) {
            try {
                imap.examineMailbox(mailbox);
                foreach int uid in (imap.uidSearch(criteria)) {
                    *hash<auto> ev = getExampleAttachment(uid);
                    if (ev) {
                        return ev;
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                logDbg("could not retrieve example event data: %s: %s", ex.err, ex.desc);
            }
            return ImapAttachmentWatchDataProvider::getFakeExampleData(mailbox);
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }

    #! @cond nodoc
    #! Returns the first matching attachment of a message as event data, if any
    private *hash<auto> getExampleAttachment(int uid) {
        hash<string, hash<ImapClient::ImapMessageInfo>> info = imap.fetch(string(uid),
            ("BODY.PEEK[]",), True);
        *binary raw = info.firstValue().body."BODY[]";
        if (!exists raw) {
            return;
        }
        MailMessage::Message m(binary_to_string(raw, ImapClient::ProtocolEncoding));
        foreach MailMessage::Attachment att in (m.getAttachments()) {
            string name = att.getName();
            if (attachment_glob.val() && !ImapAttachmentWatchDataProvider::matchesGlob(name,
                attachment_glob, ignore_case)) {
                continue;
            }
            return {
                "mailbox": mailbox,
                "uid": uid,
                "subject": m.getSubject(),
                "from": m.getFrom(),
                "name": name,
                "mime": att.getMime(),
                "size": att.getData().size(),
            };
        }
    }

    #! Returns representative event data for when no live attachment is available
    private static hash<auto> getFakeExampleData(string mailbox) {
        return {
            "mailbox": mailbox,
            "uid": 4392,
            "subject": "Daily sales export",
            "from": "reports@example.com",
            "name": "sales-20260725.csv",
            "mime": "text/csv",
            "size": 18432,
        };
    }

    private reportMessage(int uid) {
        hash<string, hash<ImapClient::ImapMessageInfo>> info = imap.fetch(string(uid),
            ("BODY.PEEK[]",), True);
        *binary raw = info.firstValue().body."BODY[]";
        if (!exists raw) {
            return;
        }
        MailMessage::Message m(binary_to_string(raw, ImapClient::ProtocolEncoding));

        foreach MailMessage::Attachment att in (m.getAttachments()) {
            string name = att.getName();
            if (attachment_glob.val() && !ImapAttachmentWatchDataProvider::matchesGlob(name,
                attachment_glob, ignore_case)) {
                continue;
            }
            hash<auto> event = {
                "mailbox": mailbox,
                "uid": uid,
                "subject": m.getSubject(),
                "from": m.getFrom(),
                "name": name,
                "mime": att.getMime(),
                "size": att.getData().size(),
            };
            if (retrieve_attachments) {
                event.data = att.getData();
            }
            notifyObservers(EVENT_IMAP_ATTACHMENT_MATCHED, event);
        }
    }

    #! Returns True if a filename matches a glob pattern
    private static bool matchesGlob(string name, string pattern, bool ignore_case) {
        # translate the glob into an anchored regex so matching does not depend on the filesystem
        string re = "";
        foreach string c in (pattern.split("")) {
            switch (c) {
                case "*": re += ".*"; break;
                case "?": re += "."; break;
                case ".":
                case "\\":
                case "+":
                case "(":
                case ")":
                case "[":
                case "]":
                case "{":
                case "}":
                case "^":
                case "$":
                case "|":
                    re += "\\" + c;
                    break;
                default: re += c; break;
            }
        }
        return regex(name, sprintf("^%s$", re), ignore_case ? RE_Caseless : 0);
    }
    #! @endcond
}
}
