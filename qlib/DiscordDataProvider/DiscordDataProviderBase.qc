# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DiscordDataProvider module
public namespace DiscordDataProvider {
#! The Discord data provider base class
public class DiscordDataProviderBase inherits DataProvider::AbstractDataProvider {
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
        #! The connection object used to create the REST client
        *DiscordRestConnection conn;

        #! The REST client object for API calls
        *DiscordRestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(DiscordRestClient rest, *Logger::LoggerInterface logger) {
        self.rest = rest;
        if (logger) {
            setLogger(logger);
        }
    }

    #! Creates the object
    constructor(DiscordRestConnection conn, *Logger::LoggerInterface logger) {
        self.conn = conn;
        rest = conn.get();
        if (logger) {
            setLogger(logger);
        }
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Wait for a given period of time
    static threadWait(date wait_period) {
%ifdef PO_NO_PROCESS_CONTROL
        # workaround to wait in a context without process control
        Mutex m();
        Condition c();
        m.lock();
        on_exit m.unlock();
        c.wait(m, wait_period);
%else
        usleep(wait_period);
%endif
    }

    #! Retrieves info for the given guild; must be accessible to the calling user
    private hash<auto> getGuild(string id) {
        # get guild info; note that users cannot request info for a specific guild directly (AFAIK), only bots can for
        # some reason; so we request all guilds accessible for the user and try to find the one we want
        *hash<auto> guilds;
        hash<auto> info;
        try {
            guilds = map {$1.id: $1}, doRestCommand("GET", "users/@me/guilds", NOTHING, \info).body;
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            *hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }

        *hash<auto> guild = guilds{id};
        throw "GUILD-ERROR", sprintf("guild %y is not accessible to this user; accessible guilds: %y", id,
            keys guilds);
        return guild;
    }

    #! Makes a REST call and returns the response; handles rate limit responses
    private hash<auto> doRestCommand(string method, string path, auto body, reference<hash<auto>> info) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getURL(), path, body);
                return rest.doRequest(method, path, body, \info);
            } catch (hash<ExceptionInfo> ex) {
                # handle rate limit errors immediately
                if (info."response-headers".status_code == 429
                    && (info."response-headers"."x-ratelimit-remaining" == "0")
                    && (*softfloat r = info."response-headers"."x-ratelimit-reset")) {
                    int lt = r.toInt(); softint us = (r - lt) * 1000000;
                    date reset = localtime(lt, us);
                    date diff = reset - now_us();
                    # do something if the rate limit expiry time is in the future
                    if (diff > 0s) {
                        if (diff > 1m) {
                            throw "RATE-LIMIT-ERROR", sprintf("the reset time: %y is more than 1 minute in the "
                                "future (%y)", reset, diff);
                        }
                        warn("RATE LIMIT: connection sleeping for: %y", diff);
                        threadWait(diff);
                    }
                    continue;
                }
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (HTTP response code: %y x-ratelimit-remaining: %y x-ratelimit-reset: %y)",
                    ex.desc, info."response-headers".status_code, info."response-headers"."x-ratelimit-remaining",
                    info."response-headers"."x-ratelimit-reset");
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
