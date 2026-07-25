# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapClientDataProviderBase.qc base class for IMAP data providers

/*  ImapClientDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ImapClientDataProvider module
public namespace ImapClientDataProvider {
#! The base class for IMAP data providers
/** Holds the @ref ImapClient::ImapClient "ImapClient" that the concrete providers act on and declares the
    connection options shared by all of them.

    @since %ImapClientDataProvider 1.0
*/
public class ImapClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor arguments
        const ConstructorOptions = {
            "auth_mechanism": <DataProviderOptionInfo>{
                "display_name": "Auth Mechanism",
                "short_desc": "Forces a specific SASL authentication mechanism",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Forces a specific SASL authentication mechanism instead of selecting one from "
                    "the mechanisms the server advertises in its `CAPABILITY` response.\n\n"
                    "Leave this at `auto` unless a server advertises a mechanism it cannot actually "
                    "complete",
                "default_value": ImapClient::ImapClient::AuthMechanismAuto,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": ImapClient::ImapClient::AuthMechanismAuto,
                        "display_name": "Automatic",
                        "desc": "Select the mechanism from the server's `CAPABILITY` list",
                    }, <AllowedValueInfo>{
                        "value": ImapClient::ImapClient::AuthMechanismPlain,
                        "display_name": "SASL PLAIN",
                        "desc": "Username and password authentication with SASL `PLAIN`, falling back "
                            "to `LOGIN` on servers without `SASL-IR`",
                    }, <AllowedValueInfo>{
                        "value": ImapClient::ImapClient::AuthMechanismXoauth2,
                        "display_name": "SASL XOAUTH2",
                        "desc": "OAuth2 bearer-token authentication with SASL `XOAUTH2`; requires the "
                            "`token` option",
                    }, <AllowedValueInfo>{
                        "value": ImapClient::ImapClient::AuthMechanismOauthBearer,
                        "display_name": "SASL OAUTHBEARER",
                        "desc": "OAuth2 bearer-token authentication with SASL `OAUTHBEARER` "
                            "([RFC 7628](https://datatracker.ietf.org/doc/html/rfc7628)); requires the "
                            "`token` option",
                    },
                ),
            },
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "The connect timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The connect timeout in milliseconds",
                "default_value": ImapClient::DefaultConnectTimeout.durationMilliseconds(),
            },
            "io_timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "The I/O timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The I/O timeout in milliseconds",
                "default_value": ImapClient::DefaultIoTimeout.durationMilliseconds(),
            },
            "max_response_size": <DataProviderOptionInfo>{
                "display_name": "Maximum Response Size",
                "short_desc": "The maximum accepted size of a single IMAP response in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The maximum accepted size of a single IMAP response in bytes.\n\n"
                    "This bounds memory use when talking to a broken or hostile server; raise it when "
                    "retrieving very large messages or attachments",
                "default_value": ImapClient::DefaultMaxResponseSize,
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The password for authentication; conflicts with the `token` option",
                "sensitive": True,
            },
            "tls": <DataProviderOptionInfo>{
                "display_name": "Use STARTTLS?",
                "short_desc": "Issue the STARTTLS command after connecting with insecure connections",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Issue the `STARTTLS` command after connecting with insecure connections; "
                    "ignored when the `imaps` scheme is used, since that connection is already secure",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "An OAuth2 bearer token for SASL XOAUTH2 / OAUTHBEARER logins",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An OAuth2 bearer token to use for authentication.\n\n"
                    "When set, the connection authenticates with a bearer-token SASL mechanism instead "
                    "of a password, which is what providers that have retired password logins "
                    "require.\n\n"
                    "**Requires the `username` option** and conflicts with the `password` option",
                "sensitive": True,
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "The username for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The username for authentication; always required",
                "required": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for an IMAP connection; must use an `imap://` or `imaps://` scheme",
                "required": True,
                "example_value": "imaps://imap.example.com",
            },
        };

        #! The IMAP client used by this provider
        ImapClient::ImapClient imap;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options,
            hash<string, hash<DataProviderOptionInfo>> con_opts = ConstructorOptions) {
        checkOptions("CONSTRUCTOR-ERROR", con_opts, options);
        imap = getImapClientConnection(options);
    }

    #! Creates the object from an existing IMAP client
    constructor(ImapClient::ImapClient imap) {
        self.imap = imap;
    }

    #! Returns the IMAP client used by this provider
    /** @return the IMAP client used by this provider

        Exposed so callers can close the connection or install a refreshed OAuth2 access token
        without recreating the provider.
    */
    ImapClient::ImapClient getImapClient() {
        return imap;
    }

    #! Returns an IMAP client from constructor options
    /** @param options the constructor options

        @return a configured IMAP client
    */
    ImapClient::ImapClient getImapClientConnection(hash<auto> options) {
        ImapClient::ImapClient client(options.url, options - "url");
        if (logger) {
            client.setLogger(logger);
        }
        return client;
    }
}
}
