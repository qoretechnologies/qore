# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
            "HTTPCLIENT-CONNECT-ERROR": True,
            "HTTPCLIENT-STREAM-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The connection object used to create the REST client
        *DiscordRestConnection conn;

        #! The REST client object for API calls
        *DiscordRestClientIo rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    /** @param rest the REST client for API calls
        @param logger an optional logger

        @since DiscordDataProvider 2.1
    */
    constructor(DiscordRestClientIo rest, *Logger::LoggerInterface logger) {
        self.rest = rest;
        if (logger) {
            setLogger(logger);
        }
    }

    #! Creates the object
    /** @param rest a synchronous REST client; API calls are made with a @ref DiscordRestClient::DiscordRestClientIo
        "DiscordRestClientIo" created from it with @ref getAsyncClient()
        @param logger an optional logger
    */
    constructor(DiscordRestClient rest, *Logger::LoggerInterface logger) {
        self.rest = DiscordDataProviderBase::getAsyncClient(rest);
        if (logger) {
            setLogger(logger);
        }
    }

    #! Creates the object
    constructor(DiscordRestConnection conn, *Logger::LoggerInterface logger) {
        self.conn = conn;
        rest = conn.getAsync();
        if (logger) {
            setLogger(logger);
        }
    }

    #! Returns the REST client for API calls
    /** @since DiscordDataProvider 2.1
    */
    *DiscordRestClientIo getRestClient() {
        return rest;
    }

    #! Returns an asynchronous REST client created with the options and the connection path of a synchronous one
    /** @param rest the synchronous REST client

        @return an asynchronous REST client created with the options and the connection path of \a rest

        @since DiscordDataProvider 2.1
    */
    static DiscordRestClientIo getAsyncClient(DiscordRestClient rest) {
        DiscordRestClientIo rv(rest.getConstructorOptions());
        rv.setConnectionPath(rest.getConnectionPath());
        return rv;
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
        # some reason; so we request all guilds accessible for the user and try to find the one we want; the
        # exception argument of an error response includes the response body
        *hash<auto> guilds = map {$1.id: $1}, doRestCommand("GET", "users/@me/guilds").body;

        *hash<auto> guild = guilds{id};
        if (!guild) {
            throw "GUILD-ERROR", sprintf("guild %y is not accessible to this user; accessible guilds: %y", id,
                keys guilds);
        }
        return guild;
    }

    #! Makes a REST call and returns the response; handles rate limit responses
    /** @param method the HTTP method
        @param path the request path
        @param body the optional request body

        @return the response

        @throw REST-RESPONSE-ERROR an error response was received; the exception argument contains the response
        @throw RATE-LIMIT-ERROR the rate limit resets more than a minute in the future
    */
    private hash<RestResponseInfo> doRestCommand(string method, string path, auto body) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getUrl(), path, body);
                return rest.restDoRequest(method, path, body);
            } catch (hash<ExceptionInfo> ex) {
                # the status code and headers of an error response
                *hash<auto> resp;
                if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.typeCode() == NT_HASH) {
                    resp = ex.arg{"status_code", "headers"};
                }
                # handle rate limit errors immediately
                if (resp.status_code == 429
                    && (resp.headers."x-ratelimit-remaining" == "0")
                    && (*softfloat r = resp.headers."x-ratelimit-reset")) {
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
                # only append response info that is actually present; on non-response errors (ex: OAuth2 token
                # refresh failures) there is no response, so the bare exception is rethrown unchanged
                # instead of a useless "(HTTP response code: null x-ratelimit-remaining: null ...)" suffix
                list<string> parts;
                if (exists resp.status_code) {
                    parts += sprintf("HTTP response code: %y", resp.status_code);
                }
                if (exists resp.headers."x-ratelimit-remaining") {
                    parts += sprintf("x-ratelimit-remaining: %y", resp.headers."x-ratelimit-remaining");
                }
                if (exists resp.headers."x-ratelimit-reset") {
                    parts += sprintf("x-ratelimit-reset: %y", resp.headers."x-ratelimit-reset");
                }
                if (parts) {
                    rethrow ex.err, sprintf("%s (%s)", ex.desc, join(" ", parts));
                }
                rethrow;
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        return False;
    }
}
}
