# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AsyncApiDataProvider module definition

/** AsyncApiDataProviderCommon.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AsyncApiDataProvider module
public namespace AsyncApiDataProvider {
#! The AsyncAPI data provider common base class
public class AsyncApiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    private {
        #! The AsyncAPI schema
        AsyncApi::AsyncApi3Schema schema;

        #! The transport for message operations
        *AbstractAsyncApiTransport transport;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object with a transport
    constructor(AbstractAsyncApiTransport transport) {
        self.transport = transport;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (transport) {
            transport.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the AsyncAPI schema
    AsyncApi::AsyncApi3Schema getSchema() {
        return schema;
    }

    #! Returns the transport
    /** @return the transport object or NOTHING if not set
    */
    *AbstractAsyncApiTransport getTransport() {
        return transport;
    }

    #! Sets the transport
    /** @param transport the transport to use for message operations
    */
    setTransport(AbstractAsyncApiTransport transport) {
        self.transport = transport;
    }
}
}
