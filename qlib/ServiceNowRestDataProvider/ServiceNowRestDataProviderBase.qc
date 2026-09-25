# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ServiceNowRestDataProvider module definition

/** ServiceNowRestDataProviderBase.qc Copyright 2020 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ServiceNowRestDataProvider module
public namespace ServiceNowRestDataProvider {
#! The ServiceNowRest data provider class
public class ServiceNowRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST client object for API calls
        ServiceNowRestClient::ServiceNowRestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    /** @param rest the REST client for API calls

        @since ServiceNowRestDataProvider 2.1
    */
    constructor(ServiceNowRestClient::ServiceNowRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object
    /** @param rest a synchronous REST client; API calls are made with a
        @ref ServiceNowRestClient::ServiceNowRestClientIo "ServiceNowRestClientIo" created from it with
        @ref getAsyncClient()
    */
    constructor(ServiceNowRestClient::ServiceNowRestClient rest) {
        self.rest = ServiceNowRestDataProviderBase::getAsyncClient(rest);
    }

    #! Returns the REST client for API calls
    /** @since ServiceNowRestDataProvider 2.1
    */
    ServiceNowRestClient::ServiceNowRestClientIo getRestClient() {
        return rest;
    }

    #! Returns an asynchronous REST client created with the options and the connection path of a synchronous one
    /** @param rest the synchronous REST client

        @return an asynchronous REST client created with the options and the connection path of \a rest

        @since ServiceNowRestDataProvider 2.1
    */
    static ServiceNowRestClient::ServiceNowRestClientIo getAsyncClient(ServiceNowRestClient::ServiceNowRestClient rest) {
        ServiceNowRestClient::ServiceNowRestClientIo rv(rest.getConstructorOptions());
        rv.setConnectionPath(rest.getConnectionPath());
        return rv;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }
}
}
