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
        DataProvider::AbstractWatchDataProviderBase {
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
        };

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

        #! Current epoch for polling (tracks latest message timestamp)
        int poll_epoch;

        #! Message IDs already processed at current epoch (deduplication)
        hash<string, bool> last_msg = {};

        #! Setup info for polling (initialized on first poll)
        hash<auto> setup_info;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest,
            hash<string, hash<DataProviderOptionInfo>> constructor_options, *hash<auto> options)
            : GoogleDataProviderBase(rest),
              AbstractWatchDataProviderBase(GmailMessageWatchDataProviderBase::getPollInterval(
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

        q = copts.q;
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

    #! Performs a single polling operation
    private pollOnce() {
        if (!setup_info) {
            setup_info = setup();
        }
        pollOnceIntern(\setup_info);
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

    #! Perform one poll
    private *hash<auto> pollOnceIntern(reference<hash<auto>> setup) {
        # check for matching mail
        try {
            hash<auto> res;
            string gmail_query = q;
            if (poll_epoch) {
                # Include the preceding second so messages sharing the checkpoint second can be
                # deduplicated by ID instead of being lost if Gmail applies a strict timestamp bound.
                gmail_query += sprintf(" after:%d", poll_epoch - 1);
            }
            string this_uri = "gmail/v1/users/me/messages?q=" + encode_url(gmail_query);
            LoggerWrapper::debug("Gmail polling using URI: %y epoch: %y poll_interval: %ds", this_uri, poll_epoch,
                poll_secs);
            res = rest.restGet(this_uri).body;
            int match;
            list<hash<auto>> messages;
            foreach hash<auto> minfo in (res.messages) {
                # check for shutdown between REST calls
                if (isStopping()) {
                    break;
                }
                # retrieve each email
                string msguri = sprintf("gmail/v1/users/me/messages/%s", minfo.id);
                hash<auto> msg = rest.restGet(msguri).body;

                # try to decode body as base64 if there is a Content-Transfer-Encoding header
                # do not allow message bodies to be re-encoded as base64
                processHeaders(\msg.payload, GmailMessageEventDataType::Fields,
                    GmailMessageEventDataType::PartFields);

                # check message structure and convert types where necessary
                msg = setup.msgtype.acceptsValue(msg);

                # convert base64-decoded message bodies to strings if they have text MIME types
                processBody(\msg.payload);

                # Fetch the page before delivery and process it oldest-first. Gmail lists newest
                # messages first; advancing a durable timestamp in that order could skip an older
                # message if delivery of a later item failed.
                push messages, msg;
            }
            messages = sort(messages, int sub (hash<auto> left, hash<auto> right) {
                return left.internalDate <=> right.internalDate;
            });
            foreach hash<auto> msg in (messages) {
                int msg_epoch = msg.internalDate / 1000;
                if (isStopping()) {
                    break;
                }
                if (msg_epoch < poll_epoch || (msg_epoch == poll_epoch && last_msg{msg.id})) {
                    continue;
                }
                if (retrieve_attachments) {
                    msg.attachments = ();
                    foreach hash<auto> part in (msg.payload.parts) {
                        if (*string aid = part.body.attachmentId) {
                            # retrieve attachment; skip on error to preserve the message event
                            string atturi = sprintf("gmail/v1/users/me/messages/%s/attachments/%s", msg.id,
                                aid);
                            try {
                                hash<auto> att = rest.restGet(atturi).body;
                                msg.attachments += getAttachment(att, part.headers);
                            } catch (hash<ExceptionInfo> att_ex) {
                                LoggerWrapper::warn("Gmail: failed to retrieve attachment %y for message %y: %s",
                                    aid, msg.id, get_exception_string(att_ex));
                            }
                        }
                    }
                    LoggerWrapper::info("Gmail: matched message %y attachments: %d", msg.id,
                        msg.attachments.size());
                } else {
                    LoggerWrapper::info("Gmail: matched message %y", msg.id);
                }

                if (setup.test_mode) {
                    return msg;
                }
                ++match;
                messageReceived(msg);
                bool epoch_updated = msg_epoch > poll_epoch;
                commitCheckpoint(msg_epoch, msg.id);
                if (delete_messages) {
                    string deluri = sprintf("gmail/v1/users/me/messages/%s", msg.id);
                    rest.restDel(deluri);
                    LoggerWrapper::info("Gmail: deleted matched message %y", msg.id);
                }
                if (epoch_updated) {
                    LoggerWrapper::debug("Gmail using new epoch: %y", poll_epoch);
                }
            }
            if (!match) {
                LoggerWrapper::debug("Gmail: matched no messages");
            }
        } catch (hash<ExceptionInfo> ex) {
            if (setup.test_mode) {
                LoggerWrapper::error("Gmail error retrieving emails in test mode: %s", get_exception_string(ex));
                return;
            }

            LoggerWrapper::error("Gmail error retrieving emails: %s; polling again in %ds", get_exception_string(ex),
                poll_secs);
        }
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
