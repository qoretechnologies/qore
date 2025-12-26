# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! The EmpathicBuilding data provider class
public class EmpathicBuildingDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls
        RestClient::RestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(RestClient::RestClient rest, *Logger::LoggerInterface logger) {
        self.rest = rest;
        if (logger) {
            setLogger(logger);
        }
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Update any connection used by the data provider
    /** @param conn the new AbstractConnection object
        @param connobj the new connection object itself (ex: RestClient for a "rest" connection)
    */
    updateConnectionObjects(object conn, object connobj) {
        if (!(connobj instanceof RestClient)) {
            throw "UPDATE-CONNECTION-ERROR", sprintf("Data provider %y requires a \"RestClient\" object; got "
                "%y instead", self.className(), connobj.className());
        }
        rest = cast<RestClient>(connobj);
        rest.setLogger(logger);
    }

    #! Converts a Haltian ms epoch offset to a date/time value
    static date getTimestamp(int epoch_ms) {
        int epoch = epoch_ms / 1000;
        int ms = epoch_ms- epoch * 1000;
        return localtime(epoch) + milliseconds(ms);
    }

    #! Makes a REST call and returns the response
    private hash<auto> doRestCommand(string method, string path, auto body, reference<hash<auto>> info) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getURL(), path, body);
                return rest.doRequest(method, path, body, \info);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        return False;
    }
}
}