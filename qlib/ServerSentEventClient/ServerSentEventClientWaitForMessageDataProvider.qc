# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientWaitForMessageDataProvider.qc SSE connection client

/*  ServerSentEventClientWaitForMessageDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! ServerSentEvent client wait for message from server API data provider
public class ServerSentEventClientWaitForMessageDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ServerSentEventClientWaitForMessageDataProvider",
            "supports_request": True,
            "desc": "Waits for a ServerSentEvent message to be received from the server within a timeout period and "
                "returns the message if one is received in the timeout perdiod, or an exception is thrown",
        };

        #! Request type
        const RequestType = ServerSentEventClientWaitForMessageRequestDataType;

        #! Response type
        const ResponseType = AbstractDataProviderTypeMap."any";
    }

    private {
        ServerSentEventClientDataProviderBase base;
    }

    #! Creates the object
    constructor(ServerSentEventClientDataProviderBase base) {
        base.register(self);
        self.base = base;
    }

    #! Deletes the object
    destructor() {
        if (base) {
            base.deregister();
        }
    }

    #! Returns the data provider name
    string getName() {
        return "wait-for-message";
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        Queue q = base.getQueue();
        on_error if ($1.err == "QUEUE-TIMEOUT") {
            rethrow "SSE-RECV-TIMEOUT", sprintf("timed out waiting for ServerSentEvent message: %s: %s", $1.err,
                $1.desc);
        }
        return q.get(req."timeout");
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}