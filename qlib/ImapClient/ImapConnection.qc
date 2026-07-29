# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapConnection.qc IMAP connection class

/*  ImapConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Class for IMAP connections; returns an object of class @ref ImapClient::ImapClient "ImapClient"
/** Supports the polling API, so connections can be monitored without holding a thread.

    @since %ImapClient 1.0
*/
public class ImapConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "IMAP Connection",
            "short_desc": "A connection to an IMAP server",
            "desc": "A connection to an IMAP4rev1 server for reading and managing email.\n\n"
                "IMAP keeps messages on the server, so mail can be searched, retrieved, flagged, and "
                "moved without downloading and deleting it. This is the standard way to integrate with "
                "email providers that do not publish their own API",
            "cls": Class::forName("ImapConnection"),
            "schemes": {
                "imaps": True,
                "imap": True,
            },
            "options": {
                "auth_mechanism": <ConnectionOptionInfo>{
                    "display_name": "Auth Mechanism",
                    "short_desc": "Forces a specific SASL authentication mechanism",
                    "type": "string",
                    "desc": "Forces a specific SASL authentication mechanism instead of selecting one "
                        "from the mechanisms the server advertises in its `CAPABILITY` response.\n\n"
                        "Leave this at `auto` unless a server advertises a mechanism it cannot actually "
                        "complete; the automatic selection uses the capability list read after "
                        "`STARTTLS` and again after login, which is the list that reflects what the "
                        "connection really supports",
                    "default_value": ImapClient::AuthMechanismAuto,
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": ImapClient::AuthMechanismAuto,
                            "display_name": "Automatic",
                            "desc": "Select the mechanism from the server's `CAPABILITY` list",
                        }, <AllowedValueInfo>{
                            "value": ImapClient::AuthMechanismPlain,
                            "display_name": "SASL PLAIN",
                            "desc": "Username and password authentication with SASL `PLAIN`, falling "
                                "back to `LOGIN` on servers without `SASL-IR`",
                        }, <AllowedValueInfo>{
                            "value": ImapClient::AuthMechanismXoauth2,
                            "display_name": "SASL XOAUTH2",
                            "desc": "OAuth2 bearer-token authentication with SASL `XOAUTH2`; requires "
                                "the `token` option",
                        }, <AllowedValueInfo>{
                            "value": ImapClient::AuthMechanismOauthBearer,
                            "display_name": "SASL OAUTHBEARER",
                            "desc": "OAuth2 bearer-token authentication with SASL `OAUTHBEARER` "
                                "([RFC 7628](https://datatracker.ietf.org/doc/html/rfc7628)); requires "
                                "the `token` option",
                        },
                    ),
                },
                "connect_timeout": <ConnectionOptionInfo>{
                    "display_name": "Connect Timeout",
                    "short_desc": "The connect timeout in milliseconds",
                    "type": "timeout",
                    "desc": "The connect timeout in milliseconds",
                    "default_value": DefaultConnectTimeout.durationMilliseconds(),
                },
                "io_timeout": <ConnectionOptionInfo>{
                    "display_name": "I/O Timeout",
                    "short_desc": "The I/O timeout in milliseconds",
                    "type": "timeout",
                    "desc": "The I/O timeout in milliseconds",
                    "default_value": DefaultIoTimeout.durationMilliseconds(),
                },
                "max_response_size": <ConnectionOptionInfo>{
                    "display_name": "Maximum Response Size",
                    "short_desc": "The maximum accepted size of a single IMAP response in bytes",
                    "type": "int",
                    "desc": "The maximum accepted size of a single IMAP response in bytes.\n\n"
                        "This bounds memory use when talking to a broken or hostile server; raise it "
                        "when retrieving very large messages or attachments",
                    "default_value": DefaultMaxResponseSize,
                },
                "password": <ConnectionOptionInfo>{
                    "display_name": "Password",
                    "short_desc": "The password for authentication",
                    "type": "string",
                    "desc": "The password for authentication; conflicts with the `token` option",
                    "sensitive": True,
                },
                "tls": <ConnectionOptionInfo>{
                    "display_name": "Use STARTTLS?",
                    "short_desc": "Issue the STARTTLS command after connecting with insecure connections",
                    "type": "bool",
                    "desc": "Issue the `STARTTLS` command after connecting with insecure connections; "
                        "ignored when the `imaps` scheme is used, since that connection is already "
                        "secure",
                },
                "token": <ConnectionOptionInfo>{
                    "display_name": "Authorization Token",
                    "short_desc": "An OAuth2 bearer token for SASL XOAUTH2 / OAUTHBEARER logins",
                    "type": "string",
                    "desc": "An OAuth2 bearer token to use for authentication.\n\n"
                        "When set, the connection authenticates with a bearer-token SASL mechanism "
                        "(`XOAUTH2` or `OAUTHBEARER`) instead of a password, which is what providers "
                        "that have retired password logins require.\n\n"
                        "**Requires the `username` option** and conflicts with the `password` option. "
                        "Access tokens expire; refresh them on the connection rather than relying on a "
                        "token captured once",
                    "sensitive": True,
                },
                "username": <ConnectionOptionInfo>{
                    "display_name": "Username",
                    "short_desc": "The username for authentication",
                    "type": "string",
                    "desc": "The username for authentication; always required",
                },
            },
        };
    }

    #! creates the ImapConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes
        @param options connection options

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {})
            : AbstractConnectionWithInfo(name, description, url, attributes, options) {
    }

    #! creates the ImapConnection object
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! returns \c "imap"
    string getType() {
        return "imap";
    }

    #! returns @ref True, since this connection type supports the polling API
    bool supportsPollingApi() {
        return True;
    }

    #! Called to start a non-blocking polling ping operation on the remote IMAP server
    /** @param logger an optional logger for the polling operation

        @return a socket poll operation object that will allow the connection goal to be reached with
        polling

        @see supportsPollingApi()
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        return new ImapPingPollOperation(getImpl(False));
    }

    #! Performs a blocking ping operation on the IMAP server
    /** Connects, authenticates, and sends a \c NOOP to verify the connection is working.
    */
    private pingImpl() {
        ImapPingPollOperation op(getImpl(False));
        execPollOperation(op, sprintf("imap-ping:%s", name));
    }

    #! returns an @ref ImapClient::ImapClient "ImapClient" object
    /** @param connect if @ref True, \c ImapClient::connect() is called
        @param rtopts runtime options for the connection

        @return an @ref ImapClient::ImapClient "ImapClient" object
    */
    private ImapClient getImpl(bool connect = True, *hash<auto> rtopts) {
        ImapClient client(url, opts + {"logger": logger});
        if (connect) {
            client.connect();
        }
        return client;
    }

    #! Executes a polling operation synchronously through the global AsyncIoController
    private static execPollOperation(Qore::AbstractPollOperation op, string owner, timeout to = 30s) {
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

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}
