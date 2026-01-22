# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientDataProviderBase class definition

/** Pop3ClientDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Pop3ClientDataProvider module
public namespace Pop3ClientDataProvider {
#! The POP3 client data provider class, provides API classes as children
public class Pop3ClientDataProviderBase inherits AbstractDataProvider {
    public {
        #! Constructor arguments
        const ConstructorOptions = {
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "The connect timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The connect timeout in milliseconds",
                "default_value": Pop3Client::DefaultConnectTimeout.durationMilliseconds(),
            },
            "io_timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "The I/O timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The I/O timeout in milliseconds",
                "default_value": Pop3Client::DefaultIoTimeout.durationMilliseconds(),
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The password for authentication; conflicts with the `token` option",
                "sensitive": True,
            },
            "tls": <DataProviderOptionInfo>{
                "display_name": "Secure?",
                "short_desc": "Use the STLS command after connecting with insecure connections",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Use the `STLS` command after connecting with insecure connections",
                "sensitive": True,
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token for an SASL XOAUTH2 login",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Any authorization token to use for the connection; will be used for SASL XOAUTH2 "
                    "logins; conflicts with the `password` option",
                "sensitive": True,
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The username for authentication; always required",
                "required": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for an POP3 or POP3S connection; must use an `pop3://` or `pop3s://` scheme",
                "required": True,
            },
        };

        Pop3Client pop3;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<string, hash<DataProviderOptionInfo>> con_opts = ConstructorOptions) {
        checkOptions("CONSTRUCTOR-ERROR", con_opts, options);
        pop3 = getPop3ClientConnection(options);
    }

    #! Creates the object from an POP3 connection
    constructor(Pop3Client pop3) {
        self.pop3 = pop3;
    }

    #! Returns an POP3 connection from constructor options
    Pop3Client getPop3ClientConnection(hash<auto> options) {
        Pop3Client pop3(options.url, options - "url");
        if (logger) {
            pop3.setLogger(logger);
        }
        return pop3;
    }
}
}