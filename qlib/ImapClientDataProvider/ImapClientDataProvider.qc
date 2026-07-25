# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapClientDataProvider.qc root IMAP data provider

/*  ImapClientDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The root IMAP data provider, providing the API operations as children
/** @since %ImapClientDataProvider 1.0
*/
public class ImapClientDataProvider inherits ImapClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ImapClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! The child providers keyed by name
        const ChildMap = {
            "search": Class::forName("ImapClientDataProvider::ImapSearchDataProvider"),
            "get-message": Class::forName("ImapClientDataProvider::ImapGetMessageDataProvider"),
            "get-attachment": Class::forName("ImapClientDataProvider::ImapGetAttachmentDataProvider"),
            "list-mailboxes": Class::forName("ImapClientDataProvider::ImapListMailboxesDataProvider"),
            "set-flags": Class::forName("ImapClientDataProvider::ImapSetFlagsDataProvider"),
            "move-messages": Class::forName("ImapClientDataProvider::ImapMoveMessagesDataProvider"),
            "append-message": Class::forName("ImapClientDataProvider::ImapAppendMessageDataProvider"),
            "watch": Class::forName("ImapClientDataProvider::ImapMessageWatchDataProvider"),
            "watch-attachment":
                Class::forName("ImapClientDataProvider::ImapAttachmentWatchDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ImapClientDataProviderBase(options) {
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) : ImapClientDataProviderBase(imap) {
    }

    #! Returns the data provider name
    string getName() {
        return "imap";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for IMAP servers";
    }

    #! Returns child provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(imap);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Renders a message as a typed response hash
    /** @param imap the IMAP client with the mailbox already selected
        @param uid the message UID

        @return the message described according to @ref ImapMessageResponseDataType

        @note attachment content is summarized rather than included, so that a message with large
        attachments can be inspected cheaply; retrieve content with the \c get-attachment operation
    */
    static hash<auto> describeMessage(ImapClient::ImapClient imap, int uid) {
        hash<string, hash<ImapClient::ImapMessageInfo>> info = imap.fetch(string(uid),
            ("FLAGS", "BODY.PEEK[]"), True);
        *hash<ImapClient::ImapMessageInfo> msg = info.firstValue();
        *binary raw = msg.body."BODY[]";
        if (!exists raw) {
            throw "IMAP-MESSAGE-NOT-FOUND", sprintf("no message with UID %d was returned by the server",
                uid);
        }

        MailMessage::Message m(binary_to_string(raw, ImapClient::ProtocolEncoding));

        list<hash<auto>> attachments = ();
        foreach MailMessage::Attachment att in (m.getAttachments()) {
            attachments += {
                "name": att.getName(),
                "mime": att.getMime(),
                "size": att.getData().size(),
            };
        }

        *hash<auto> headers = m.getHeaders();
        *data body = m.getBody();

        return {
            "uid": uid,
            "subject": m.getSubject(),
            "from": m.getFrom(),
            "to": m.getTO() ?? (),
            "date": ImapClient::ImapResponseParser::parseEnvelopeDate(
                ImapClientDataProvider::getHeader(headers, "Date")),
            "message_id": ImapClientDataProvider::getHeader(headers, "Message-ID"),
            "flags": msg.flags ?? (),
            "body": exists body
                ? (body.typeCode() == NT_BINARY
                    ? binary_to_string(body, ImapClient::ProtocolEncoding)
                    : body.toString())
                : NOTHING,
            "attachments": attachments,
            "raw": raw,
        };
    }

    #! Returns a header value, matching the header name case-insensitively
    /** @param headers the parsed message headers
        @param name the header name to look up

        @return the header value, or @ref nothing if the header is not present

        @note header names are case-insensitive per RFC 5322, and senders do not agree on casing
    */
    static *string getHeader(*hash<auto> headers, string name) {
        if (!headers) {
            return;
        }
        string want = name.lwr();
        foreach string key in (keys headers) {
            if (key.lwr() == want) {
                return headers{key}.toString();
            }
        }
    }
}

#! The IMAP data provider factory
/** @since %ImapClientDataProvider 1.0
*/
public class ImapClientDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        #! Data provider type info
        static Class cls = new Class("ImapClientDataProvider");

        #! Factory info
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "imap",
            "desc": "IMAP client data provider factory",
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };
    }

    #! Returns static factory information without \a provider_info
    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    #! Returns static provider information
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return ImapClientDataProvider::ProviderInfo;
    }

    #! Returns the class for the data provider object
    private Class getClassImpl() {
        return cls;
    }
}
}
