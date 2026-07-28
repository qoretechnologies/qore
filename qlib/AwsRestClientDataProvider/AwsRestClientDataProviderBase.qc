# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsRestClientDataProvider class definition

/** AwsRestClientDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated doContent-Typecumentation files (the "Software"),
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

#! contains all public definitions in the AwsRestClientDataProvider module
public namespace AwsRestClientDataProvider {
#! The AWS REST client base data provider class
public class AwsRestClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        AwsRestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object from a REST connection
    constructor(AwsRestClientIo rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! This provider opts out of the framework-injected generic API call child
    /** This is a generic AWS REST client provider whose own \c call child already makes arbitrary
        signed requests through the connection, exactly as the framework-injected
        @ref DataProvider::AbstractDataProvider::GenericApiCallChildName "__call__" child would; the
        same reasoning applies here as for
        @ref RestClientDataProvider::RestClientDataProviderBase "RestClientDataProviderBase" and the
        \c GenericRest app.

        @return @ref False

        @since AwsRestClientDataProvider 1.1
    */
    private bool hasGenericApiCallChildImpl() {
        return False;
    }
}
}

