# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file MllpConnection.qc ConnectionProvider support for MLLP

/*  MllpConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MllpUtil {
    #! ConnectionProvider connection object for MLLP endpoints
    public class MllpConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
        public {
            #! Connection entry info
            const ConnectionScheme = <ConnectionSchemeInfo>{
                "display_name": "MLLP Connection",
                "short_desc": "A connection to an MLLP endpoint",
                "desc": "A TCP or TLS connection to an MLLP endpoint for HL7 v2 message exchange",
                "cls": Class::forName("MllpConnection"),
                "schemes": {
                    "mllp": True,
                    "mllps": True,
                },
                "options": {
                    "connect_timeout": <ConnectionOptionInfo>{
                        "display_name": "Connect Timeout",
                        "short_desc": "The connect timeout in milliseconds",
                        "type": "timeout",
                        "desc": "The connection timeout to use in milliseconds (default: 30 seconds)",
                        "default_value": 30000,
                    },
                    "timeout": <ConnectionOptionInfo>{
                        "display_name": "I/O Timeout",
                        "short_desc": "The read/write timeout in milliseconds",
                        "type": "timeout",
                        "desc": "The socket read/write timeout to use in milliseconds (default: 30 seconds)",
                        "default_value": 30000,
                    },
                    "max_frame_size": <ConnectionOptionInfo>{
                        "display_name": "Maximum Frame Size",
                        "short_desc": "The maximum inbound MLLP payload size in bytes",
                        "type": "int",
                        "desc": "Rejects inbound MLLP frames larger than this byte count",
                        "default_value": DefaultMaxFrameSize,
                    },
                    "encoding": <ConnectionOptionInfo>{
                        "display_name": "Payload Encoding",
                        "short_desc": "The HL7 payload character encoding",
                        "type": "string",
                        "desc": "The character encoding used to decode and encode HL7 payloads",
                        "default_value": "UTF-8",
                    },
                },
            };
        }

        #! Creates the MLLP connection object
        constructor(string name, string description, string url, hash attributes = {}, hash options = {})
                : AbstractConnectionWithInfo(name, description, url, attributes, options) {
        }

        #! Creates the MLLP connection object
        constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
        }

        #! returns \c "mllp"
        string getType() {
            return "mllp";
        }

        #! returns an @ref MllpUtil::MllpClient "MllpClient" object
        private MllpClient getImpl(bool connect = True, *hash<auto> rtopts) {
            hash<auto> client_opts = opts + (rtopts ?? {}) + {
                "url": url,
                "no_connect": !connect,
            };
            return new MllpClient(client_opts);
        }

        #! Returns the ConnectionSchemeInfo hash for this object
        private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
            return ConnectionScheme;
        }
    }
}
