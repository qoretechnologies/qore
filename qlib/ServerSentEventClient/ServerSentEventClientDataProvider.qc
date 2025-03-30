# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientDataProvider.qc SSE connection client

/*  ServerSentEventClientDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! the ServerSentEventClient namespace contains all the objects in the ServerSentEventClient module
public namespace ServerSentEventClient {
#! Event-based data provider for ServerSentEvent messaging
/**
    The event ID raised for ServerSentEvent data events is \c "sse-data-event" (see @ref EVENT_SSE_DATA), and, when the
    connection is closed, a \c "sse-closed-event" (see @ref EVENT_SSE_CLOSED) is raised.
*/
public class ServerSentEventClientDataProvider inherits ServerSentEventClientDataProviderBase, DataProvider::Observable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ServerSentEventClientDataProvider",
        } + ServerSentEventClientDataProviderBase::ProviderInfo;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ServerSentEventClientDataProviderBase(options) {
        client.observersReady();
    }

    #! Creates the object from constructor + template options
    constructor(hash<auto> template_options, *hash<auto> options)
            : ServerSentEventClientDataProviderBase(
                ServerSentEventClientDataProviderBase::getOptionsFromTemplate(template_options, options)
            ) {
        client.observersReady();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = ProviderInfo;
        string safe_url = client.getSafeUrl();
        safe_url =~ s/^http/sse/;
        rv.desc = sprintf("ServerSentEvent client to URL %y", safe_url);
        rv.supports_auto_reconnect = client.getReconnect();
        return rv;
    }
}
}