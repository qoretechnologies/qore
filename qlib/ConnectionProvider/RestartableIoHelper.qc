# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore RestartableIoHelper class definition

/*  RestartableIoHelper.qc Copyright 2016 - 2025 Qore Technologies, s.r.o.

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

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
#! Class with helper methods to options in a hash
public class RestartableIoHelper inherits LoggerWrapper {
    public {
        #! Retryable I/O errors
        const RetryableIoErrors = {
            "SOCKET-CONNECT-ERROR": True,
            "SOCKET-SSL-ERROR": True,
            "SOCKET-TIMEOUT": True,
            "SOCKET-SEND-ERROR": True,
            "SOCKET-RECV-ERROR": True,
            "SOCKET-CLOSED": True,
            "SOCKET-HTTP-ERROR": True,
        };
    }

    private {
        date start;
        int count = 0;
        const DefaultDelayStart = 5s;
        const DefaultDelayFactor = 1.5;
        const DefaultDelayMax = 60s;
    }

    destructor() {
        resetIntern();
    }

    *string restartIo(hash<auto> ex) {
        while (True) {
            *string msg;
            if (msg = isTransientIoError(ex)) {
                if (!start) {
                    start = now_us();
                }
                date delay;
                ++count;
                if (count == 2) {
                    delay = DefaultDelayStart;
                } else if (count > 2) {
                    delay = min(DefaultDelayMax, seconds(DefaultDelayStart * (DefaultDelayFactor * (count - 2))));
                } else {
                    delay = 0s;
                }
                string errmsg = sprintf("Restartable I/O error encountered; count: %d; delay: %s", count,
                    delay ? sprintf("%y", delay) : "none");
                if (count > 2) {
                    errmsg += sprintf(" (elapsed time: %y)", now_us() - start);
                }
                errmsg += ": " + msg;
                error("%s", errmsg);
                if (delay) {
                    usleep(delay);
                }
                return errmsg;
            }
            if (!ex.next) {
                break;
            }
            ex = ex.next;
        }
    }

    *string isTransientIoError(hash<auto> ex) {
        return RetryableIoErrors{ex.err} ? sprintf("%s: %s", ex.err, ex.desc) : NOTHING;
    }

    int getCount() {
        return count;
    }

    reset() {
        resetIntern();
        count = 0;
    }

    private resetIntern() {
        if (!count) {
            return;
        }
        info("I/O operation recovered in %d attempts; elapsed time: %y", count + 1, now_us() - start);
        delete start;
    }

    #! Returns the logger interface for logging
    private *LoggerInterface getLogger() {
        return DataProvider::getGlobalLogger();
    }
}
}
