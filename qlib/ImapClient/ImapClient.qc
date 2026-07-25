# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapClient.qc IMAP4rev1 client class

/*  ImapClient.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ImapClient {
#! Provides an IMAP4rev1 client
/** This class uses a @ref Qore::Thread::Mutex "Mutex" to serialize access to the underlying socket and
    internal state, so it is safe to use from multiple threads.

    IMAP is a stateful protocol: @ref selectMailbox() establishes the mailbox that subsequent
    @ref fetch(), @ref search(), and flag operations act on. The connection is made implicitly by any
    method that needs it, so calling @ref connect() explicitly is optional.

    All I/O is non-blocking and executed through the async I/O controller; no thread is held per
    connection while waiting for the server.

    @since %ImapClient 1.0
*/
public class ImapClient inherits Logger::LoggerWrapper, ConnectionProvider::OptionHelper {
    public {
        #! accepted protocols
        const Protocols = {
            "imap": {"tls": False, "port": ImapPort},
            "imaps": {"tls": True, "port": ImapsPort},
        };

        #! Separator byte used in the SASL XOAUTH2 and OAUTHBEARER payloads
        const CtrlA = chr(1);

        #! Selects the SASL mechanism from the server's \c CAPABILITY list
        const AuthMechanismAuto = "auto";

        #! SASL \c PLAIN / \c LOGIN authentication with a username and password
        const AuthMechanismPlain = "PLAIN";

        #! SASL \c XOAUTH2 bearer-token authentication
        const AuthMechanismXoauth2 = "XOAUTH2";

        #! SASL \c OAUTHBEARER bearer-token authentication (RFC 7628)
        const AuthMechanismOauthBearer = "OAUTHBEARER";

        #! All supported values for the \c auth_mechanism option
        const AuthMechanisms = {
            AuthMechanismAuto: True,
            AuthMechanismPlain: True,
            AuthMechanismXoauth2: True,
            AuthMechanismOauthBearer: True,
        };
    }

    #! @cond nodoc
    private {
        #! the socket used for the session
        Socket sock();

        #! the connection target
        string connect;

        #! serializes access to the socket and internal state
        Mutex mutex();

        #! immediate TLS flag (imaps scheme)
        bool ssl = False;

        #! STARTTLS flag
        bool starttls = False;

        #! authentication credentials
        string user;
        *string pass;
        *string token;

        #! forced SASL mechanism
        string auth_mechanism = ImapClient::AuthMechanismAuto;

        #! optional client identification sent with the ID command
        *hash<string, string> client_id;

        #! I/O timeout in milliseconds
        timeout ioTimeout;

        #! connect timeout in milliseconds
        timeout connectTimeout;

        #! the maximum accepted response size in bytes
        int max_response_size;

        #! capabilities advertised by the server
        *hash<string, bool> capabilities;

        #! the currently-selected mailbox, if any
        *hash<ImapMailboxStatus> selected;

        #! command tag counter
        int tag_counter = 0;

        #! maximum length of a single debug log line
        const MaxDebugLine = 2048;
    }
    #! @endcond

    #! Creates the ImapClient object
    /** @par Example:
        @code{.py}
ImapClient imap("imaps://imap.example.com", {
    "username": "user@example.com",
    "token": access_token,
});
        @endcode

        @param url the URL of the IMAP server; accepted schemes are \c "imap" (port @ref ImapPort) and
        \c "imaps" (port @ref ImapsPort); if no scheme is given, \c "imap" is assumed
        @param opts options for the object; supported options:
        - \c auth_mechanism (string): forces a specific SASL mechanism instead of selecting one from the
          server's \c CAPABILITY list; one of @ref AuthMechanismAuto (the default),
          @ref AuthMechanismPlain, @ref AuthMechanismXoauth2, or @ref AuthMechanismOauthBearer
        - \c client_id (hash): field/value pairs sent with the \c ID command
          (<a href="https://datatracker.ietf.org/doc/html/rfc2971">RFC 2971</a>) after login
        - \c connect_timeout (int): the connect timeout in milliseconds
        - \c io_timeout (int): the I/O timeout in milliseconds
        - \c logger (@ref Logger::LoggerInterface "LoggerInterface"): a logger to use for logging
        - \c max_response_size (int): the maximum accepted size of a single response in bytes; defaults
          to @ref DefaultMaxResponseSize
        - \c password (string): the password for authentication; conflicts with the \c token option
        - \c token (string): an OAuth2 bearer token; conflicts with the \c password option
        - \c tls (bool): issue \c STARTTLS after connecting with an insecure connection
        - \c username (string): the username for authentication; always required

        @throw PARSE-URL-ERROR the URL could not be parsed
        @throw IMAP-URL-ERROR the scheme is unknown, or no target or username was given
        @throw IMAP-CONFIG-ERROR conflicting or incomplete authentication options were given
    */
    constructor(string url, *hash<auto> opts) {
        logger = remove opts.logger;

        hash<UrlInfo> hurl = parse_url(url, True);

        if (!hurl.protocol.val()) {
            if (!hurl.port && hurl.host[0] != "/") {
                hurl.port = ImapPort;
            }
        } else {
            *hash<auto> conf = Protocols.(hurl.protocol.lwr());
            if (!exists conf) {
                throw "IMAP-URL-ERROR", sprintf("unknown scheme %y in %y; known schemes: %y", hurl.protocol,
                    get_safe_url(url), keys Protocols);
            }
            ssl = conf.tls;
            if (!hurl.port && hurl.host[0] != "/") {
                hurl.port = conf.port;
            }
        }

        if (!hurl.host.val()) {
            throw "IMAP-URL-ERROR", sprintf("no hostname was given in URL %y", get_safe_url(url));
        }

        if (string(int(hurl.host)) == hurl.host) {
            connect = sprintf("localhost:%d", hurl.host);
        } else if (hurl.host[0] != "/") {
            connect = sprintf("%s:%d", hurl.host, hurl.port);
        } else {
            connect = hurl.host;
        }

        *string username = getString(opts, "username");
        *string password = getString(opts, "password");
        *string tok = getString(opts, "token");

        if (hurl.username.val()) {
            if (!exists hurl.password) {
                throw "IMAP-URL-ERROR", sprintf("have a username but no password in IMAP URL %y",
                    get_safe_url(url));
            }
            if (username.val()) {
                throw "IMAP-CONFIG-ERROR", sprintf("username is set in URL %y and also as an option: %y",
                    get_safe_url(url), username);
            }
            if (tok.val()) {
                throw "IMAP-CONFIG-ERROR", sprintf("a token was given for URL %y that already carries a "
                    "username and password; to use a token, give the username as an option and leave the "
                    "credentials out of the URL", get_safe_url(url));
            }
            user = hurl.username;
            pass = hurl.password;
        } else {
            if (!username.val()) {
                throw "IMAP-URL-ERROR", sprintf("missing username option for IMAP connection %y",
                    get_safe_url(url));
            }
            user = username;
            if (tok.val()) {
                if (password.val()) {
                    throw "IMAP-CONFIG-ERROR", sprintf("cannot provide both a password and a token for "
                        "connection %y", get_safe_url(url));
                }
                token = tok;
            } else if (password.val()) {
                pass = password;
            } else {
                throw "IMAP-CONFIG-ERROR", sprintf("missing both password and token for connection %y",
                    get_safe_url(url));
            }
        }

        if (opts.auth_mechanism.val()) {
            string mech = opts.auth_mechanism;
            string norm = mech.lwr() == AuthMechanismAuto ? AuthMechanismAuto : mech.upr();
            if (!AuthMechanisms{norm}) {
                throw "IMAP-CONFIG-ERROR", sprintf("unknown auth_mechanism %y; known mechanisms: %y", mech,
                    keys AuthMechanisms);
            }
            auth_mechanism = norm;
        }

        if (opts.client_id) {
            client_id = opts.client_id;
        }

        connectTimeout = getInt(opts, "connect_timeout", False, DefaultConnectTimeout.durationMilliseconds());
        ioTimeout = getInt(opts, "io_timeout", False, DefaultIoTimeout.durationMilliseconds());
        max_response_size = getInt(opts, "max_response_size", False, DefaultMaxResponseSize);
        if (max_response_size <= 0) {
            throw "IMAP-CONFIG-ERROR", sprintf("max_response_size must be > 0; got %d", max_response_size);
        }

        if (opts.tls) {
            starttls = True;
        }

        logDbg("user: %y server: %y (%s)", user, connect, token ? "bearer token" : "password");
    }

    #! Disconnects if connected and destroys the object
    destructor() {
        try {
            disconnect();
        } catch (hash<ExceptionInfo> ex) {
            logDbg("ImapClient disconnect: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns the connection target string
    string getTarget() {
        return connect;
    }

    #! Returns a hash with connection information used to build poll operations
    /** @return a hash with the following keys:
        - \c target: the connection target string
        - \c ssl: @ref True if an immediate TLS connection should be made
        - \c tls: @ref True if \c STARTTLS should be issued after connecting
        - \c user: the username for authentication
        - \c pass: the password, if any
        - \c token: the OAuth2 bearer token, if any
        - \c auth_mechanism: the configured SASL mechanism
        - \c max_response_size: the maximum accepted response size in bytes
    */
    hash<auto> getInfo() {
        return {
            "target": connect,
            "ssl": ssl,
            "tls": starttls,
            "user": user,
            "pass": pass,
            "token": token,
            "auth_mechanism": auth_mechanism,
            "max_response_size": max_response_size,
        };
    }

    #! Sets or clears the OAuth2 bearer token
    /** @param tok the OAuth2 bearer token, or @ref nothing to clear it

        @note
        - OAuth2 access tokens expire; use this method to install a refreshed token on an existing object
        - any password set for the connection is cleared, since the two are mutually exclusive
        - the new token takes effect on the next connection; it does not re-authenticate an open session
        - this method is subject to thread serialization
    */
    setToken(*string tok) {
        AutoLock am(mutex);
        if (tok.val()) {
            pass = NOTHING;
        }
        token = tok;
    }

    #! Returns the OAuth2 bearer token, if any
    *string getToken() {
        AutoLock am(mutex);
        return token;
    }

    #! Returns the configured SASL mechanism
    string getAuthMechanism() {
        return auth_mechanism;
    }

    #! Returns the connection status
    bool isConnected() {
        return sock.isOpen();
    }

    #! Connects and authenticates
    /** It is not necessary to call this method explicitly; any method that needs a connection makes one.

        @throw IMAP-POLL-ERROR the server returned an error response
    */
    connect() {
        AutoLock am(mutex);
        connectIntern();
    }

    #! Disconnects from the server, sending \c LOGOUT if connected
    disconnect() {
        AutoLock am(mutex);
        disconnectIntern();
    }

    #! Closes the connection without sending \c LOGOUT
    forceDisconnect() {
        AutoLock am(mutex);
        if (sock.isOpen()) {
            sock.close();
        }
        capabilities = NOTHING;
        selected = NOTHING;
    }

    #! Returns the capabilities advertised by the server
    /** @return a hash of capability names in upper case to @ref True

        @note connects if not already connected
    */
    hash<string, bool> getCapabilities() {
        AutoLock am(mutex);
        checkConnectedIntern();
        return capabilities ?? {};
    }

    #! Returns True if the server advertises the given capability
    /** @param cap the capability name; the comparison is case-insensitive

        @note connects if not already connected
    */
    bool hasCapability(string cap) {
        AutoLock am(mutex);
        checkConnectedIntern();
        return capabilities{cap.upr()} ?? False;
    }

    #! Returns the currently-selected mailbox status, if any
    *hash<ImapMailboxStatus> getSelectedMailbox() {
        AutoLock am(mutex);
        return selected;
    }

    #! Sends a \c NOOP command
    /** \c NOOP is also the standard way to poll for mailbox updates on a selected mailbox.
    */
    noop() {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern("NOOP");
    }

    #! Lists mailboxes matching a pattern
    /** @par Example:
        @code{.py}
foreach hash<ImapMailboxInfo> mb in (imap.listMailboxes()) {
    printf("%s (%s)\n", mb.name, mb.flags.join(", "));
}
        @endcode

        @param reference the reference name, normally an empty string
        @param pattern the mailbox name pattern; \c "*" matches at any depth, \c "%%" matches within one
        hierarchy level

        @return the matching mailboxes

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    list<hash<ImapMailboxInfo>> listMailboxes(string reference = "", string pattern = "*") {
        AutoLock am(mutex);
        checkConnectedIntern();
        return parseListResponse(commandIntern(sprintf("LIST %s %s", ImapClient::quoteString(reference),
            ImapClient::quoteString(pattern))), "LIST");
    }

    #! Lists subscribed mailboxes matching a pattern
    /** @param reference the reference name, normally an empty string
        @param pattern the mailbox name pattern

        @return the matching subscribed mailboxes

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    list<hash<ImapMailboxInfo>> listSubscribed(string reference = "", string pattern = "*") {
        AutoLock am(mutex);
        checkConnectedIntern();
        return parseListResponse(commandIntern(sprintf("LSUB %s %s", ImapClient::quoteString(reference),
            ImapClient::quoteString(pattern))), "LSUB");
    }

    #! Selects a mailbox for read-write access
    /** @par Example:
        @code{.py}
hash<ImapMailboxStatus> status = imap.selectMailbox("INBOX");
        @endcode

        @param mailbox the mailbox name

        @return the mailbox state

        @throw IMAP-SERVER-ERROR the mailbox does not exist or cannot be selected
    */
    hash<ImapMailboxStatus> selectMailbox(string mailbox) {
        AutoLock am(mutex);
        return selectIntern(mailbox, False);
    }

    #! Selects a mailbox for read-only access
    /** @param mailbox the mailbox name

        @return the mailbox state

        @throw IMAP-SERVER-ERROR the mailbox does not exist or cannot be selected
    */
    hash<ImapMailboxStatus> examineMailbox(string mailbox) {
        AutoLock am(mutex);
        return selectIntern(mailbox, True);
    }

    #! Returns status information for a mailbox without selecting it
    /** @param mailbox the mailbox name
        @param items the status items to request (ex: \c "MESSAGES", \c "UNSEEN", \c "UIDNEXT",
        \c "UIDVALIDITY", \c "RECENT")

        @return a hash of the requested item names to their values

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    hash<auto> getMailboxStatus(string mailbox, list<string> items = ("MESSAGES", "RECENT", "UNSEEN")) {
        AutoLock am(mutex);
        checkConnectedIntern();
        hash<ImapResponse> resp = commandIntern(sprintf("STATUS %s (%s)",
            ImapClient::quoteString(mailbox), items.join(" ")));

        hash<auto> rv = {};
        foreach list<auto> line in (resp.untagged) {
            if (line.size() < 4 || line[1].typeCode() != NT_STRING || line[1].upr() != "STATUS") {
                continue;
            }
            auto items_list = line[3];
            if (items_list.typeCode() != NT_LIST) {
                continue;
            }
            for (int i = 0; (i + 1) < items_list.size(); i += 2) {
                rv{items_list[i].toString().upr()} = items_list[i + 1];
            }
        }
        return rv;
    }

    #! Creates a mailbox
    /** @param mailbox the mailbox name

        @throw IMAP-SERVER-ERROR the mailbox could not be created
    */
    createMailbox(string mailbox) {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern(sprintf("CREATE %s", ImapClient::quoteString(mailbox)));
    }

    #! Deletes a mailbox
    /** @param mailbox the mailbox name

        @throw IMAP-SERVER-ERROR the mailbox could not be deleted
    */
    deleteMailbox(string mailbox) {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern(sprintf("DELETE %s", ImapClient::quoteString(mailbox)));
    }

    #! Renames a mailbox
    /** @param old_name the current mailbox name
        @param new_name the new mailbox name

        @throw IMAP-SERVER-ERROR the mailbox could not be renamed
    */
    renameMailbox(string old_name, string new_name) {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern(sprintf("RENAME %s %s", ImapClient::quoteString(old_name),
            ImapClient::quoteString(new_name)));
    }

    #! Subscribes to a mailbox
    /** @param mailbox the mailbox name

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    subscribe(string mailbox) {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern(sprintf("SUBSCRIBE %s", ImapClient::quoteString(mailbox)));
    }

    #! Unsubscribes from a mailbox
    /** @param mailbox the mailbox name

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    unsubscribe(string mailbox) {
        AutoLock am(mutex);
        checkConnectedIntern();
        commandIntern(sprintf("UNSUBSCRIBE %s", ImapClient::quoteString(mailbox)));
    }

    #! Searches the selected mailbox and returns matching message sequence numbers
    /** @par Example:
        @code{.py}
list<int> nums = imap.search("UNSEEN SINCE 1-Jan-2026");
        @endcode

        @param criteria the IMAP search criteria
        @param charset an optional charset for the criteria

        @return the matching message sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server rejected the search criteria
    */
    list<int> search(string criteria, *string charset) {
        AutoLock am(mutex);
        return searchIntern(criteria, charset, False);
    }

    #! Searches the selected mailbox and returns matching UIDs
    /** @par Example:
        @code{.py}
list<int> uids = imap.uidSearch("UNSEEN FROM \"boss@example.com\"");
        @endcode

        @param criteria the IMAP search criteria
        @param charset an optional charset for the criteria

        @return the matching message UIDs

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server rejected the search criteria
    */
    list<int> uidSearch(string criteria, *string charset) {
        AutoLock am(mutex);
        return searchIntern(criteria, charset, True);
    }

    #! Fetches message data from the selected mailbox
    /** @par Example:
        @code{.py}
hash<string, hash<ImapMessageInfo>> info = imap.fetch("1:*", ("UID", "FLAGS", "ENVELOPE"));
        @endcode

        @param set the message set (ex: \c "1", \c "1:10", \c "1,3,5", \c "1:*")
        @param items the data items to fetch (ex: \c "UID", \c "FLAGS", \c "ENVELOPE",
        \c "RFC822.SIZE", \c "INTERNALDATE", \c "BODY.PEEK[]")
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @return the fetched messages keyed by UID when available, otherwise by sequence number

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    hash<string, hash<ImapMessageInfo>> fetch(string set, list<string> items, bool uid = True) {
        AutoLock am(mutex);
        return fetchIntern(set, items, uid);
    }

    #! Retrieves a complete message by UID
    /** @par Example:
        @code{.py}
MailMessage::Message msg = imap.getMessage(uid);
        @endcode

        @param uid the message UID

        @return the parsed message

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-MESSAGE-NOT-FOUND no message with the given UID was returned
        @throw IMAP-SERVER-ERROR the server returned an error response

        @note \c BODY.PEEK is used, so retrieving a message does not set the \c "\\Seen" flag; use
        @ref markSeen() to set it explicitly
    */
    MailMessage::Message getMessage(int uid) {
        AutoLock am(mutex);
        hash<string, hash<ImapMessageInfo>> info = fetchIntern(string(uid), ("BODY.PEEK[]",), True);
        *hash<ImapMessageInfo> msg = info.firstValue();
        *binary raw = msg.body."BODY[]";
        if (!exists raw) {
            throw "IMAP-MESSAGE-NOT-FOUND", sprintf("no message body was returned for UID %d in mailbox "
                "%y", uid, selected.mailbox);
        }
        return new MailMessage::Message(binary_to_string(raw, ProtocolEncoding));
    }

    #! Retrieves multiple complete messages by UID
    /** @param uids the message UIDs

        @return the parsed messages keyed by UID as a string

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    hash<string, MailMessage::Message> getMessages(list<int> uids) {
        if (!uids) {
            return {};
        }
        AutoLock am(mutex);
        hash<string, hash<ImapMessageInfo>> info = fetchIntern(
            (map string($1), uids).join(","), ("BODY.PEEK[]",), True);

        hash<string, MailMessage::Message> rv = {};
        foreach string key in (keys info) {
            *binary raw = info{key}.body."BODY[]";
            if (exists raw) {
                rv{key} = new MailMessage::Message(binary_to_string(raw, ProtocolEncoding));
            }
        }
        return rv;
    }

    #! Retrieves the envelope of a message by UID
    /** @param uid the message UID

        @return the parsed envelope, or @ref nothing if the server returned none

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    *hash<ImapEnvelopeInfo> getEnvelope(int uid) {
        AutoLock am(mutex);
        hash<string, hash<ImapMessageInfo>> info = fetchIntern(string(uid), ("ENVELOPE",), True);
        return info.firstValue().envelope;
    }

    #! Retrieves a single MIME part of a message by UID
    /** @par Example:
        @code{.py}
# retrieve just the second MIME part rather than the whole message
binary attachment = imap.getBodyPart(uid, "2");
        @endcode

        @param uid the message UID
        @param part the MIME part specifier (ex: \c "1", \c "2.1", \c "HEADER", \c "TEXT")

        @return the raw part content exactly as sent by the server

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-MESSAGE-NOT-FOUND the requested part was not returned
        @throw IMAP-SERVER-ERROR the server returned an error response

        @note this retrieves one part without downloading the whole message, which is how an attachment
        can be fetched cheaply from a large message
    */
    binary getBodyPart(int uid, string part) {
        AutoLock am(mutex);
        hash<string, hash<ImapMessageInfo>> info = fetchIntern(string(uid),
            (sprintf("BODY.PEEK[%s]", part),), True);
        *hash<ImapMessageInfo> msg = info.firstValue();
        string key = sprintf("BODY[%s]", part);
        *binary raw = msg.body{key};
        if (!exists raw) {
            throw "IMAP-MESSAGE-NOT-FOUND", sprintf("part %y of UID %d was not returned by the server; "
                "returned sections: %y", part, msg.body ? (keys msg.body) : ());
        }
        return raw;
    }

    #! Replaces the flags on the given messages
    /** @param set the message set
        @param flags the flags to set (ex: \c "\\Seen", \c "\\Flagged")
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    setFlags(string set, list<string> flags, bool uid = True) {
        AutoLock am(mutex);
        storeIntern(set, "FLAGS", flags, uid);
    }

    #! Adds flags to the given messages
    /** @param set the message set
        @param flags the flags to add
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    addFlags(string set, list<string> flags, bool uid = True) {
        AutoLock am(mutex);
        storeIntern(set, "+FLAGS", flags, uid);
    }

    #! Removes flags from the given messages
    /** @param set the message set
        @param flags the flags to remove
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    removeFlags(string set, list<string> flags, bool uid = True) {
        AutoLock am(mutex);
        storeIntern(set, "-FLAGS", flags, uid);
    }

    #! Marks a message as seen
    /** @param uid the message UID

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    markSeen(int uid) {
        addFlags(string(uid), ("\\Seen",), True);
    }

    #! Marks a message as deleted
    /** @param uid the message UID

        @note the message is not removed until @ref expunge() is called

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    markDeleted(int uid) {
        addFlags(string(uid), ("\\Deleted",), True);
    }

    #! Permanently removes all messages flagged \c "\\Deleted" from the selected mailbox
    /** @return the sequence numbers reported as expunged

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    list<int> expunge() {
        AutoLock am(mutex);
        checkSelectedIntern();
        hash<ImapResponse> resp = commandIntern("EXPUNGE");

        list<int> rv = ();
        foreach list<auto> line in (resp.untagged) {
            if (line.size() >= 3 && line[1].typeCode() == NT_INT && line[2].typeCode() == NT_STRING
                && line[2].upr() == "EXPUNGE") {
                rv += line[1];
            }
        }
        return rv;
    }

    #! Copies messages to another mailbox
    /** @param set the message set
        @param mailbox the destination mailbox
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    copyMessages(string set, string mailbox, bool uid = True) {
        AutoLock am(mutex);
        checkSelectedIntern();
        commandIntern(sprintf("%sCOPY %s %s", uid ? "UID " : "", set,
            ImapClient::quoteString(mailbox)));
    }

    #! Moves messages to another mailbox
    /** Uses the \c MOVE command (<a href="https://datatracker.ietf.org/doc/html/rfc6851">RFC 6851</a>)
        when the server advertises it. Servers without \c MOVE are handled with the equivalent
        \c COPY + \c STORE \c "\\Deleted" + \c EXPUNGE sequence; when \c UIDPLUS is also unavailable the
        fallback uses a plain \c EXPUNGE, which removes every message already flagged \c "\\Deleted" in
        the mailbox, not only the ones moved.

        @param set the message set
        @param mailbox the destination mailbox
        @param uid if @ref True, \a set contains UIDs rather than sequence numbers

        @throw IMAP-NOT-SELECTED no mailbox is selected
        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    moveMessages(string set, string mailbox, bool uid = True) {
        AutoLock am(mutex);
        checkSelectedIntern();

        if (capabilities.MOVE) {
            commandIntern(sprintf("%sMOVE %s %s", uid ? "UID " : "", set,
                ImapClient::quoteString(mailbox)));
            return;
        }

        logDbg("server does not support MOVE; using COPY + STORE + EXPUNGE");
        commandIntern(sprintf("%sCOPY %s %s", uid ? "UID " : "", set, ImapClient::quoteString(mailbox)));
        commandIntern(sprintf("%sSTORE %s +FLAGS (\\Deleted)", uid ? "UID " : "", set));
        if (uid && capabilities.UIDPLUS) {
            commandIntern(sprintf("UID EXPUNGE %s", set));
        } else {
            commandIntern("EXPUNGE");
        }
    }

    #! Appends a message to a mailbox
    /** @par Example:
        @code{.py}
imap.appendMessage("Sent", msg, ("\\Seen",));
        @endcode

        @param mailbox the destination mailbox
        @param message the message to append
        @param flags optional flags to set on the appended message
        @param internal_date optional server-side arrival time to record

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    appendMessage(string mailbox, MailMessage::Message message, *list<string> flags,
            *date internal_date) {
        AutoLock am(mutex);
        checkConnectedIntern();
        appendIntern(mailbox, binary(message.serialize()), flags, internal_date);
    }

    #! Appends a raw RFC 822 message to a mailbox
    /** @param mailbox the destination mailbox
        @param message the serialized message
        @param flags optional flags to set on the appended message
        @param internal_date optional server-side arrival time to record

        @throw IMAP-SERVER-ERROR the server returned an error response
    */
    appendRawMessage(string mailbox, binary message, *list<string> flags, *date internal_date) {
        AutoLock am(mutex);
        checkConnectedIntern();
        appendIntern(mailbox, message, flags, internal_date);
    }

    #! Runs an arbitrary IMAP command on the connection and returns the parsed response
    /** Provided as an escape hatch for commands this class does not model directly.

        @param command the command text without the tag or trailing CRLF

        @return the parsed response

        @throw IMAP-SERVER-ERROR the server returned a \c NO or \c BAD completion
    */
    hash<ImapResponse> sendCommand(string command) {
        AutoLock am(mutex);
        checkConnectedIntern();
        return commandIntern(command);
    }

    #! Builds the SASL setup implied by a connection info hash
    /** @param info a connection info hash as returned by @ref getInfo()

        @return the authentication setup used to drive the poll operation pipelines

        @throw IMAP-CONFIG-ERROR a bearer mechanism was forced without a token, or
        @ref AuthMechanismPlain was forced with a token
    */
    static hash<ImapAuthInfo> getAuthInfo(hash<auto> info) {
        hash<ImapAuthInfo> rv();

        string mech = info.auth_mechanism ?? AuthMechanismAuto;
        bool wants_bearer = mech == AuthMechanismXoauth2 || mech == AuthMechanismOauthBearer;

        if (wants_bearer && !info.token.val()) {
            throw "IMAP-CONFIG-ERROR", sprintf("auth_mechanism %y requires an OAuth2 bearer token, but "
                "none is set for the connection", mech);
        }
        if (mech == AuthMechanismPlain && info.token.val()) {
            throw "IMAP-CONFIG-ERROR", sprintf("auth_mechanism %y cannot be used with an OAuth2 bearer "
                "token", mech);
        }

        if (!info.user.val()) {
            return rv;
        }

        if (info.token.val()) {
            rv.do_auth = True;
            rv.use_bearer = True;
            rv.mechanism = mech == AuthMechanismAuto ? AuthMechanismXoauth2 : mech;

            string xoauth2 = sprintf("user=%s%sauth=Bearer %s%s%s", info.user, CtrlA, info.token, CtrlA,
                CtrlA).toBase64();
            (*string host, *string port) = ImapClient::splitTarget(info.target);
            string oauthbearer = sprintf("n,a=%s,%shost=%s%sport=%s%sauth=Bearer %s%s%s", info.user,
                CtrlA, host, CtrlA, port, CtrlA, info.token, CtrlA, CtrlA).toBase64();

            if (mech == AuthMechanismXoauth2) {
                # forced: use the same payload whichever way the capability check goes
                rv.xoauth2_ir = rv.oauthbearer_ir = xoauth2;
            } else if (mech == AuthMechanismOauthBearer) {
                rv.xoauth2_ir = rv.oauthbearer_ir = oauthbearer;
            } else {
                rv.xoauth2_ir = xoauth2;
                rv.oauthbearer_ir = oauthbearer;
            }
            rv.bearer_ir = mech == AuthMechanismOauthBearer ? oauthbearer : xoauth2;
            return rv;
        }

        if (info.pass.val()) {
            rv.do_auth = True;
            rv.mechanism = AuthMechanismPlain;
            rv.plain_ir = make_base64_string(binary() + <00> + binary(info.user) + <00>
                + binary(info.pass));
            rv.login_args = sprintf("%s %s", ImapClient::quoteString(info.user),
                ImapClient::quoteString(info.pass));
        }
        return rv;
    }

    #! Extracts the capabilities from a raw \c CAPABILITY response
    /** @param raw the raw response

        @return a hash of capability names in upper case to @ref True
    */
    static hash<string, bool> parseCapabilities(binary raw) {
        hash<string, bool> caps = {};
        # the tag is irrelevant here: every line is treated as data and scanned for a CAPABILITY token
        hash<ImapResponse> resp = ImapResponseParser::parse(raw, "no-tag");
        foreach list<auto> line in (resp.untagged) {
            ImapClient::collectCapabilities(line, \caps);
        }
        return caps;
    }

    #! Quotes a string for use as an IMAP argument
    /** @param str the string to quote

        @return the quoted string with backslashes and double quotes escaped
    */
    static string quoteString(string str) {
        string rv = str;
        rv =~ s/\\/\\\\/g;
        rv =~ s/"/\\"/g;
        return sprintf("\"%s\"", rv);
    }

    #! Formats a date as an IMAP \c INTERNALDATE value
    /** @param d the date to format

        @return the formatted date (ex: \c "17-Jul-1996 02:44:25 -0700")
    */
    static string formatInternalDate(date d) {
        return format_date("DD-Mon-YYYY HH:mm:SS ", d) + ImapClient::formatUtcOffset(d);
    }

    #! Splits a \c "host:port" connection target into its parts
    /** @param target the connection target string

        @return a list of the host and the port as a string; the port is an empty string for targets
        without one
    */
    static list<string> splitTarget(string target) {
        int pos = target.rfind(":");
        if (pos < 0) {
            return (target, "");
        }
        return (target.substr(0, pos), target.substr(pos + 1));
    }

    #! @cond nodoc
    # Formats the UTC offset of a date as +HHMM / -HHMM
    private static string formatUtcOffset(date d) {
        int secs = d.getUtcOffset();
        string sign = secs < 0 ? "-" : "+";
        int abs_secs = secs < 0 ? -secs : secs;
        return sprintf("%s%02d%02d", sign, abs_secs / 3600, (abs_secs % 3600) / 60);
    }

    # Collects capability tokens from a parsed response line
    private static collectCapabilities(list<auto> values, reference<hash<string, bool>> caps) {
        int idx = -1;
        foreach auto v in (values) {
            if (v.typeCode() == NT_LIST) {
                ImapClient::collectCapabilities(v, \caps);
            } else if (idx < 0 && v.typeCode() == NT_STRING && v.upr() == "CAPABILITY") {
                idx = $#;
            }
        }
        if (idx < 0) {
            return;
        }
        for (int i = idx + 1; i < values.size(); ++i) {
            if (values[i].typeCode() == NT_STRING) {
                caps{values[i].upr()} = True;
            }
        }
    }

    private log(string msg, ...) {
        if (logger) {
            logArgs(LoggerLevel::INFO, "ImapClient: " + msg, argv);
        }
    }

    private logDbg(string msg, ...) {
        if (logger) {
            msg = vsprintf(msg, argv);
            trim msg;
            if (msg.size() > MaxDebugLine) {
                splice msg, MaxDebugLine;
                msg += "...";
            }
            logArgs(LoggerLevel::DEBUG, "%s", "ImapClient: " + msg);
        }
    }

    # Returns the next command tag
    private string nextTag() {
        return sprintf("t%d", ++tag_counter);
    }

    # Connects and authenticates if not already connected
    private checkConnectedIntern() {
        if (!sock.isOpen()) {
            connectIntern();
        }
    }

    # Throws unless a mailbox is selected
    private checkSelectedIntern() {
        checkConnectedIntern();
        if (!selected) {
            throw "IMAP-NOT-SELECTED", "no mailbox is selected; call ImapClient::selectMailbox() or "
                "ImapClient::examineMailbox() first";
        }
    }

    private connectIntern() {
        try {
            ImapPingPollOperation op(self, sock, False);
            execPollOperation(op, sprintf("imap-connect:%s", connect), connectTimeout);
            capabilities = op.getCapabilities();
            selected = NOTHING;
            logDbg("connected to %y; capabilities: %y", connect, keys capabilities);
            if (client_id) {
                sendClientIdIntern();
            }
        } catch (hash<ExceptionInfo> ex) {
            sock.close();
            rethrow;
        }
    }

    # Sends the RFC 2971 ID command; failures are not fatal since ID is optional
    private sendClientIdIntern() {
        if (!capabilities.ID) {
            logDbg("server does not advertise the ID capability; not sending client identification");
            return;
        }
        list<string> parts = ();
        foreach string k in (keys client_id) {
            parts += ImapClient::quoteString(k);
            parts += ImapClient::quoteString(client_id{k});
        }
        try {
            commandIntern(sprintf("ID (%s)", parts.join(" ")));
        } catch (hash<ExceptionInfo> ex) {
            logDbg("ID command failed: %s: %s", ex.err, ex.desc);
        }
    }

    private disconnectIntern() {
        if (sock.isOpen()) {
            try {
                commandIntern("LOGOUT");
            } catch (hash<ExceptionInfo> ex) {
                logDbg("LOGOUT failed: %s: %s", ex.err, ex.desc);
            }
            sock.close();
        }
        capabilities = NOTHING;
        selected = NOTHING;
    }

    # Runs one command on the open socket and returns the parsed response
    private hash<ImapResponse> commandIntern(string command) {
        string tag = nextTag();
        ImapCommandPollOperation op(sock, tag, command, max_response_size);
        execPollOperation(op, sprintf("imap-command:%s:%s", connect, command));
        return op.getResponse();
    }

    private hash<ImapMailboxStatus> selectIntern(string mailbox, bool read_only) {
        checkConnectedIntern();
        hash<ImapResponse> resp = commandIntern(sprintf("%s %s", read_only ? "EXAMINE" : "SELECT",
            ImapClient::quoteString(mailbox)));

        hash<ImapMailboxStatus> status(<ImapMailboxStatus>{
            "mailbox": mailbox,
            "read_only": read_only,
        });

        foreach list<auto> line in (resp.untagged) {
            if (line.size() < 2) {
                continue;
            }
            # "* <n> EXISTS" / "* <n> RECENT"
            if (line.size() >= 3 && line[1].typeCode() == NT_INT && line[2].typeCode() == NT_STRING) {
                switch (line[2].upr()) {
                    case "EXISTS": status.message_count = line[1]; break;
                    case "RECENT": status.recent = line[1]; break;
                }
                continue;
            }
            if (line[1].typeCode() != NT_STRING) {
                continue;
            }
            # "* FLAGS (...)"
            if (line[1].upr() == "FLAGS" && line.size() >= 3 && line[2].typeCode() == NT_LIST) {
                status.flags = map $1.toString(), line[2];
                continue;
            }
            # "* OK [<code> ...] text"
            if (line[1].upr() == "OK" && line.size() >= 3 && line[2].typeCode() == NT_LIST) {
                ImapClient::applyStatusCode(line[2], \status);
            }
        }

        if (resp.code) {
            ImapClient::applyStatusCode(resp.code, \status);
        }

        selected = status;
        return status;
    }

    # Applies a bracketed response code to a mailbox status
    private static applyStatusCode(list<auto> code, reference<hash<ImapMailboxStatus>> status) {
        if (!code || code[0].typeCode() != NT_STRING) {
            return;
        }
        switch (code[0].upr()) {
            case "UIDVALIDITY":
                if (code.size() >= 2 && code[1].typeCode() == NT_INT) {
                    status.uidvalidity = code[1];
                }
                break;
            case "UIDNEXT":
                if (code.size() >= 2 && code[1].typeCode() == NT_INT) {
                    status.uidnext = code[1];
                }
                break;
            case "UNSEEN":
                if (code.size() >= 2 && code[1].typeCode() == NT_INT) {
                    status.unseen = code[1];
                }
                break;
            case "PERMANENTFLAGS":
                if (code.size() >= 2 && code[1].typeCode() == NT_LIST) {
                    status.permanent_flags = map $1.toString(), code[1];
                }
                break;
            case "READ-ONLY":
                status.read_only = True;
                break;
            case "READ-WRITE":
                status.read_only = False;
                break;
        }
    }

    private list<hash<ImapMailboxInfo>> parseListResponse(hash<ImapResponse> resp, string verb) {
        list<hash<ImapMailboxInfo>> rv = ();
        foreach list<auto> line in (resp.untagged) {
            if (line.size() < 5 || line[1].typeCode() != NT_STRING || line[1].upr() != verb) {
                continue;
            }
            rv += <ImapMailboxInfo>{
                "flags": line[2].typeCode() == NT_LIST ? (map $1.toString(), line[2]) : (),
                "delimiter": exists line[3] ? line[3].toString() : "",
                "name": line[4].toString(),
            };
        }
        return rv;
    }

    private list<int> searchIntern(string criteria, *string charset, bool uid) {
        checkSelectedIntern();
        string cmd = sprintf("%sSEARCH ", uid ? "UID " : "");
        if (charset.val()) {
            cmd += sprintf("CHARSET %s ", charset);
        }
        cmd += criteria;

        hash<ImapResponse> resp = commandIntern(cmd);
        list<int> rv = ();
        foreach list<auto> line in (resp.untagged) {
            if (line.size() < 2 || line[1].typeCode() != NT_STRING || line[1].upr() != "SEARCH") {
                continue;
            }
            for (int i = 2; i < line.size(); ++i) {
                if (line[i].typeCode() == NT_INT) {
                    rv += line[i];
                }
            }
        }
        return rv;
    }

    private hash<string, hash<ImapMessageInfo>> fetchIntern(string set, list<string> items, bool uid) {
        checkSelectedIntern();
        # UID must be requested explicitly for UID FETCH results to be keyed reliably
        list<string> req = items;
        if (uid && !(select req, $1.upr() == "UID")) {
            req = ("UID",) + req;
        }
        hash<ImapResponse> resp = commandIntern(sprintf("%sFETCH %s (%s)", uid ? "UID " : "", set,
            req.join(" ")));

        hash<string, hash<ImapMessageInfo>> rv = {};
        foreach list<auto> line in (resp.untagged) {
            if (line.size() < 4 || line[1].typeCode() != NT_INT || line[2].typeCode() != NT_STRING
                || line[2].upr() != "FETCH" || line[3].typeCode() != NT_LIST) {
                continue;
            }
            hash<ImapMessageInfo> info = ImapClient::parseFetchItems(line[1], line[3]);
            rv{string(info.uid ?? info.seqnum)} = info;
        }
        return rv;
    }

    # Parses a FETCH item list into typed message information
    private static hash<ImapMessageInfo> parseFetchItems(int seqnum, list<auto> items) {
        hash<ImapMessageInfo> info(<ImapMessageInfo>{"seqnum": seqnum});

        for (int i = 0; i < items.size(); ++i) {
            if (items[i].typeCode() != NT_STRING) {
                continue;
            }
            string name = items[i].upr();
            auto value = (i + 1) < items.size() ? items[i + 1] : NOTHING;

            # RFC822.SIZE is an integer, not content, so it must not be caught by the /^RFC822/
            # family here; it is handled with the other scalar items below
            if (name =~ /^BODY(\.PEEK)?\[/ || name == "RFC822" || name == "RFC822.HEADER"
                || name == "RFC822.TEXT") {
                # section data: normalize the key and index the payload by section
                switch (name) {
                    case "RFC822": name = "BODY[]"; break;
                    case "RFC822.HEADER": name = "BODY[HEADER]"; break;
                    case "RFC822.TEXT": name = "BODY[TEXT]"; break;
                    default: name =~ s/\.PEEK//; break;
                }
                if (exists value) {
                    info.body{name} = (value.typeCode() == NT_BINARY)
                        ? value
                        : binary(value.toString());
                }
                ++i;
                continue;
            }

            switch (name) {
                case "UID":
                    if (value.typeCode() == NT_INT) {
                        info.uid = value;
                    }
                    ++i;
                    break;
                case "FLAGS":
                    if (value.typeCode() == NT_LIST) {
                        info.flags = map $1.toString(), value;
                    }
                    ++i;
                    break;
                case "RFC822.SIZE":
                    if (value.typeCode() == NT_INT) {
                        info.size = value;
                    }
                    ++i;
                    break;
                case "INTERNALDATE":
                    info.internal_date = ImapResponseParser::parseInternalDate(
                        (exists value) ? value.toString() : NOTHING);
                    ++i;
                    break;
                case "ENVELOPE":
                    if (value.typeCode() == NT_LIST) {
                        info.envelope = ImapClient::parseEnvelope(value);
                    }
                    ++i;
                    break;
                default:
                    # unknown item: skip its value so parsing stays aligned
                    ++i;
                    break;
            }
        }
        return info;
    }

    # Parses an ENVELOPE list into typed envelope information
    private static hash<ImapEnvelopeInfo> parseEnvelope(list<auto> env) {
        hash<ImapEnvelopeInfo> rv();
        rv.date = ImapResponseParser::parseEnvelopeDate(ImapClient::asText(env[0]));
        rv.subject = ImapClient::asText(env[1]);
        rv.from = ImapClient::parseAddressList(env[2]);
        rv.sender = ImapClient::parseAddressList(env[3]);
        rv.reply_to = ImapClient::parseAddressList(env[4]);
        rv.to = ImapClient::parseAddressList(env[5]);
        rv.cc = ImapClient::parseAddressList(env[6]);
        rv.bcc = ImapClient::parseAddressList(env[7]);
        rv.in_reply_to = ImapClient::asText(env[8]);
        rv.message_id = ImapClient::asText(env[9]);
        return rv;
    }

    # Renders a parsed value as text; literals arrive as binary and are decoded byte-preservingly
    private static *string asText(auto v) {
        if (!exists v) {
            return;
        }
        if (v.typeCode() == NT_BINARY) {
            return binary_to_string(v, ProtocolEncoding);
        }
        return v.toString();
    }

    # Parses an ENVELOPE address list
    private static *list<hash<ImapAddressInfo>> parseAddressList(auto addrs) {
        if (!exists addrs || addrs.typeCode() != NT_LIST || !addrs) {
            return;
        }
        list<hash<ImapAddressInfo>> rv = ();
        foreach auto a in (addrs) {
            if (a.typeCode() != NT_LIST) {
                continue;
            }
            hash<ImapAddressInfo> info(<ImapAddressInfo>{
                "name": ImapClient::asText(a[0]),
                "adl": ImapClient::asText(a[1]),
                "mailbox": ImapClient::asText(a[2]),
                "host": ImapClient::asText(a[3]),
            });
            if (info.mailbox.val() && info.host.val()) {
                info.address = sprintf("%s@%s", info.mailbox, info.host);
            }
            rv += info;
        }
        return rv;
    }

    private storeIntern(string set, string item, list<string> flags, bool uid) {
        checkSelectedIntern();
        commandIntern(sprintf("%sSTORE %s %s (%s)", uid ? "UID " : "", set, item, flags.join(" ")));
    }

    private appendIntern(string mailbox, binary message, *list<string> flags, *date internal_date) {
        string tag = nextTag();
        ImapAppendPollOperation op(sock, tag, mailbox, message, flags, internal_date,
            capabilities."LITERAL+" ?? False, max_response_size);
        execPollOperation(op, sprintf("imap-append:%s:%s", connect, mailbox), connectTimeout);
    }

    # Executes a poll operation synchronously through the global AsyncIoController
    private execPollOperation(Qore::AbstractPollOperation op, string owner, timeout to = ioTimeout) {
        *hash<SocketPollInfo> pi = op.continuePoll();
        if (!pi) {
            if (!op.goalReached()) {
                throw "ASYNC-IO-ERROR", sprintf("%s operation ended before reaching goal: %s", owner,
                    op.getState());
            }
            return;
        }

        hash<SocketPollResultInfo> result = AsyncIoController::exec(operation_info: <SocketPollOperationInfo>{
            "sock": pi.socket,
            "spop": op,
            "owner": owner,
            "to": to,
        });
        if (result.ex) {
            throw result.ex.err, result.ex.desc;
        }
        if (result.canceled) {
            throw "ASYNC-IO-CANCELED", sprintf("%s operation was canceled", owner);
        }
        if (!op.goalReached()) {
            throw "ASYNC-IO-ERROR", sprintf("%s operation ended before reaching goal: %s", owner,
                op.getState());
        }
    }
    #! @endcond
}
}
