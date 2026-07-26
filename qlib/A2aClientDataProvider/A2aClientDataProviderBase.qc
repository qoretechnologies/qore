# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2aClientDataProviderBase class definition

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {
#! Base class for A2A client data providers
public class A2aClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "a2a": <DataProviderOptionInfo>{
                "display_name": "A2A Client",
                "short_desc": "The A2A client object",
                "type": AbstractDataProviderType::get(new Type("A2aClient")),
                "desc": "The A2A client object to use for API calls",
            },
            "a2aconnection": <DataProviderOptionInfo>{
                "display_name": "A2A Connection",
                "short_desc": "The name of the A2A connection",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The name of the A2A connection to use",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the A2A agent",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the A2A agent (`a2a://` or `a2as://`)",
            },
        };
    }

    private {
        #! The A2A client
        A2aClient a2a;
    }

    #! Creates the object with the given A2A client
    constructor(A2aClient a2a) {
        @assert(a2a);
        self.a2a = a2a;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *A2aClient a2a = A2aClientDataProviderBase::getA2aClient(options);
        if (!a2a) {
            a2a = A2aClientDataProviderBase::getA2aClientFromConnection(options);
        }
        @assert(a2a);
        self.a2a = a2a;
    }

    #! Returns the A2A client
    A2aClient getA2aClient() {
        return a2a;
    }

    #! Returns the A2A client from options
    static *A2aClient getA2aClient(*hash<auto> options) {
        if (options.a2a) {
            return options.a2a;
        }
        if (options.url) {
            @debug("Creating A2A client from URL: %y", options.url);
            return new A2aClient(options.url);
        }
        return NOTHING;
    }

    #! Returns the A2A client from a connection
    static *A2aClient getA2aClientFromConnection(*hash<auto> options) {
        if (options.a2aconnection) {
            @debug("Getting A2A client from connection: %y", options.a2aconnection);
            AbstractConnection conn = get_connection(options.a2aconnection);
            return conn.get();
        }
        return NOTHING;
    }
}
}
