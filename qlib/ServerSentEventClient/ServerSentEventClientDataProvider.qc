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
public class ServerSentEventClientDataProvider inherits ServerSentEventClientDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ServerSentEventClientDataProvider",
        } + ServerSentEventClientDataProviderBase::ProviderInfo;

        #! Max time to wait for example data
        const ExampleTimeout = 10s;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ServerSentEventClientDataProviderBase(options) {
    }

    #! Creates the object from constructor + template options
    constructor(hash<auto> template_options, *hash<auto> options)
            : ServerSentEventClientDataProviderBase(
                ServerSentEventClientDataProviderBase::getOptionsFromTemplate(template_options, options)
            ) {
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = ProviderInfo;
        string safe_url = client.getSafeUrl();
        safe_url =~ s/^http/sse/;
        rv.desc = sprintf("ServerSentEvent client to URL %y", safe_url);
        rv.supports_auto_reconnect = client.getReconnect();
        return rv;
    }

    #! Returns example data for the given observable event
    /** @param event_id the event ID of the event

        @return example data for the given observable event

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.0
    */
    private auto getExampleEventDataImpl(string event_id) {
        ExampleObserver observer();
        client.registerObserver(observer);
        client.observersReady();
        return observer.waitMessage(ExampleTimeout) ?? AbstractDataProvider::getExampleEventDataImpl(event_id);
    }
}
}

namespace Priv {
class ExampleObserver inherits Observer {
    private {
        Queue q();
    }

    *hash<SseMessageInfo> waitMessage(*timeout to) {
        try {
            return q.get(to);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QUEUE-TIMEOUT") {
                return;
            }
            rethrow;
        }
    }

    update(string event_id, hash<auto> data_) {
        if (event_id == EVENT_SSE_DATA) {
            q.push(data_);
        }
    }
}
}