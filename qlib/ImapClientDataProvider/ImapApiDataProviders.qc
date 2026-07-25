# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapApiDataProviders.qc request/response data providers for IMAP operations

/*  ImapApiDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Searches a mailbox and returns matching message UIDs
/** The search runs on the server, so only matching messages are transferred.

    @since %ImapClientDataProvider 1.0
*/
public class ImapSearchDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapSearchDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "search",
            "desc": "Searches a mailbox and returns matching message UIDs",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "search";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Searches a mailbox with server-side IMAP search criteria and returns matching message "
            "UIDs";
    }

    #! Makes a request and returns the response
    /** @param req the search request
        @param request_options the request options

        @return the matching UIDs along with the mailbox's UID validity value
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<ImapClient::ImapMailboxStatus> status = (req.read_only ?? True)
            ? imap.examineMailbox(req.mailbox ?? "INBOX")
            : imap.selectMailbox(req.mailbox ?? "INBOX");
        list<int> uids = imap.uidSearch(req.criteria ?? "ALL", req.charset);
        return {
            "uids": uids,
            "count": uids.size(),
            "uidvalidity": status.uidvalidity,
            "message_count": status.message_count,
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapSearchRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapSearchResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Retrieves a complete message by UID
/** @since %ImapClientDataProvider 1.0
*/
public class ImapGetMessageDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapGetMessageDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "get-message",
            "desc": "Retrieves a complete message by UID",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "get-message";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a complete message by UID, including its headers, body text, and a summary "
            "of its attachments";
    }

    #! Makes a request and returns the response
    /** @param req the get-message request
        @param request_options the request options

        @return the message
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string mailbox = req.mailbox ?? "INBOX";
        # a read-write SELECT is required only when the flag has to change afterwards
        if (req.mark_seen) {
            imap.selectMailbox(mailbox);
        } else {
            imap.examineMailbox(mailbox);
        }
        int uid = req.uid;
        hash<auto> rv = ImapClientDataProvider::describeMessage(imap, uid);
        if (req.mark_seen) {
            imap.markSeen(uid);
        }
        return rv;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapGetMessageRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapMessageResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Retrieves a single MIME part of a message
/** @since %ImapClientDataProvider 1.0
*/
public class ImapGetAttachmentDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapGetAttachmentDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "get-attachment",
            "desc": "Retrieves a single MIME part of a message",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "get-attachment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Retrieves a single MIME part of a message without transferring the whole message";
    }

    #! Makes a request and returns the response
    /** @param req the get-attachment request
        @param request_options the request options

        @return the raw part content
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        imap.examineMailbox(req.mailbox ?? "INBOX");
        binary data = imap.getBodyPart(req.uid, req.part);
        return {
            "uid": req.uid,
            "part": req.part,
            "size": data.size(),
            "data": data,
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapGetAttachmentRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapAttachmentResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Lists the mailboxes in the account
/** @since %ImapClientDataProvider 1.0
*/
public class ImapListMailboxesDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapListMailboxesDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "list-mailboxes",
            "desc": "Lists the mailboxes in the account",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "list-mailboxes";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Lists the mailboxes in the account, optionally restricted to subscribed mailboxes";
    }

    #! Makes a request and returns the response
    /** @param req the list request
        @param request_options the request options

        @return the matching mailboxes
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string reference = req.reference ?? "";
        string pattern = req.pattern ?? "*";
        list<hash<ImapClient::ImapMailboxInfo>> boxes = req.subscribed_only
            ? imap.listSubscribed(reference, pattern)
            : imap.listMailboxes(reference, pattern);
        return {
            "mailboxes": (map ("name": $1.name, "delimiter": $1.delimiter,
                "flags": $1.flags), boxes),
            "count": boxes.size(),
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapListMailboxesRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapListMailboxesResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Changes the flags on a set of messages
/** @since %ImapClientDataProvider 1.0
*/
public class ImapSetFlagsDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapSetFlagsDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "set-flags",
            "desc": "Changes the flags on a set of messages",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "set-flags";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Adds, removes, or replaces the flags on a set of messages";
    }

    #! Makes a request and returns the response
    /** @param req the set-flags request
        @param request_options the request options

        @return the UIDs that were changed
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        imap.selectMailbox(req.mailbox ?? "INBOX");
        list<auto> uids = req.uids;
        string set = (map string($1), uids).join(",");
        list<string> flags = map string($1), req.flags;

        switch (req.mode ?? "add") {
            case "add": imap.addFlags(set, flags, True); break;
            case "remove": imap.removeFlags(set, flags, True); break;
            case "replace": imap.setFlags(set, flags, True); break;
            default:
                throw "IMAP-INVALID-MODE", sprintf("unknown flag mode %y; expected one of "
                    "\"add\", \"remove\", or \"replace\"", req.mode);
        }
        return {
            "uids": uids,
            "count": uids.size(),
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapSetFlagsRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapMessageSetResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Moves or copies messages to another mailbox
/** @since %ImapClientDataProvider 1.0
*/
public class ImapMoveMessagesDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapMoveMessagesDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "move-messages",
            "desc": "Moves or copies messages to another mailbox",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "move-messages";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Moves or copies messages to another mailbox; servers without the `MOVE` capability are "
            "handled with the equivalent copy-and-delete sequence";
    }

    #! Makes a request and returns the response
    /** @param req the move request
        @param request_options the request options

        @return the UIDs that were moved or copied
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        imap.selectMailbox(req.mailbox ?? "INBOX");
        list<auto> uids = req.uids;
        string set = (map string($1), uids).join(",");
        if (req.copy_only) {
            imap.copyMessages(set, req.target, True);
        } else {
            imap.moveMessages(set, req.target, True);
        }
        return {
            "uids": uids,
            "count": uids.size(),
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapMoveMessagesRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapMessageSetResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Appends a message to a mailbox
/** @since %ImapClientDataProvider 1.0
*/
public class ImapAppendMessageDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapAppendMessageDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "append-message",
            "desc": "Appends a message to a mailbox",
        });
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "append-message";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Stores a complete RFC 822 message in a mailbox, for filing a copy of a message sent by "
            "other means or importing mail from another system";
    }

    #! Makes a request and returns the response
    /** @param req the append request
        @param request_options the request options

        @return the mailbox the message was stored in and its size
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        binary message = req.message.typeCode() == NT_BINARY
            ? req.message
            : binary(req.message.toString());
        imap.appendRawMessage(req.mailbox, message, req.flags, req.internal_date);
        return {
            "mailbox": req.mailbox,
            "size": message.size(),
        };
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return ImapAppendRequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ImapAppendResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
