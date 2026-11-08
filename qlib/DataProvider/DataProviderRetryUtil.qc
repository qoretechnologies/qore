# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderRetryUtil.qc defines the shared retry policy helpers for REST-based data providers

/*  DataProviderRetryUtil.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main DataProvider namespace
public namespace DataProvider {
#! Shared retry policy for data providers that retry failed REST requests
/** Data providers that call a remote API retry a request that failed with a transient transport error (a refused
    or reset connection, a timeout, a closed socket), usually after an increasing delay.  This class holds the parts
    of that policy that are the same for every provider:

    - a name that the resolver definitely cannot resolve is not retried (@ref isNameResolutionError()): a name that
      does not exist, has no address or is not a valid name, a name that is not a numeric address where one is
      required, and a service that is not known would give the same answer again, and the backoff would only delay
      the error.  The synchronous \c RestClient raises the resolver's error
      (\c QOREADDRINFO-GETINFO-ERROR) as it is, while the asynchronous \c RestClientIo raises
      \c HTTPCLIENT-CONNECT-ERROR with the resolver's error in the exception argument; both are matched by their
      error codes and the resolver's status, never by the text of the description.  A resolver failure that can be
      temporary, such as a DNS server that does not answer or fails (\c ETIMEOUT, \c ESERVFAIL), is retried like
      any other transport error
    - code that only builds example data, and falls back to fake data when the request fails, makes a single
      attempt (@ref runWithoutRetries(), @ref retriesSuppressed())
    - the delays between transport retries are multiples of one configurable unit
      (@ref setTransportRetryDelayUnit(), @ref waitTransportRetry()), so that a process can shorten them, for
      example to test a retry without waiting for the backoff; by default the unit is one second, and each
      provider waits as documented for it

    A provider's retry loop asks @ref isRetriableTransportError() whether a transport error is retried, waits with
    @ref waitTransportRetry(), and does not retry any other error (such as a rate-limited response) while
    @ref retriesSuppressed() is @ref True.

    @par Example
    @code{.py}
private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
    if (retries >= MaxRetries) {
        return False;
    }
    if (DataProviderRetryUtil::isRetriableTransportError(ex, RetrySet)) {
        ++retries;
        # waits 2, 4 and 8 seconds with the default unit
        DataProviderRetryUtil::waitTransportRetry(pow(2, retries));
        return True;
    }
    return False;
}
    @endcode

    @since DataProvider 3.11
*/
public class DataProviderRetryUtil {
    public {
        #! The default unit of transport retry delays
        const DefaultTransportRetryDelayUnit = 1s;

        #! The longest transport retry delay (see @ref getTransportRetryDelay())
        const MaxTransportRetryDelay = 24h;

        #! The thread-local data key that holds the number of active @ref runWithoutRetries() calls
        const SuppressRetriesKey = "_DataProvider_DataProviderRetryUtil_suppress_retries";

        #! The statuses of the resolver that are definite answers (see @ref isNameResolutionError())
        /** The status is in the argument of a \c QOREADDRINFO-GETINFO-ERROR exception (<tt>arg.status</tt>):
            - \c ENOTFOUND: the name does not exist
            - \c ENODATA: the name has no address
            - \c EBADNAME: the name is not a valid name, which the resolver refuses without a DNS query
            - \c ENONAME: the name is not a numeric address, although the lookup requires one (\c AI_NUMERICHOST)
            - \c ESERVICE: the service is not a known service name or a valid port number

            The last three are answers of the resolver itself, given without a DNS query.
        */
        const DefiniteResolverStatuses = {
            "ENOTFOUND": True,
            "ENODATA": True,
            "EBADNAME": True,
            "ENONAME": True,
            "ESERVICE": True,
        };
    }

    private:internal {
        #! The unit of transport retry delays in microseconds
        static int transport_retry_delay_unit_us = get_duration_microseconds(DefaultTransportRetryDelayUnit);
    }

    #! Returns @ref True if the exception, its argument or a chained exception is a definite name resolution failure
    /** A definite name resolution failure is a \c QOREADDRINFO-GETINFO-ERROR exception whose argument gives a status
        of the resolver in @ref DefiniteResolverStatuses: the name does not exist, has no address or is not a valid
        name, the name is not a numeric address where one is required, or the service is not known, which another
        lookup would answer again.  A failure that can be temporary (a DNS server that does not answer or fails, a
        lookup that was canceled, a resolver that could not be initialized) is not one.  The exception itself is
        checked, then its argument if that is an error hash (\c HTTPCLIENT-CONNECT-ERROR from \c RestClientIo and
        \c HttpClientIo carries the connection's error in its argument), then the chained exceptions in \c next.

        @param ex the exception or error hash to check; may be @ref nothing

        @return @ref True if a definite name resolution failure is found

        @since DataProvider 3.11
    */
    static bool isNameResolutionError(*hash<auto> ex) {
        while (ex) {
            if (ex.err == "QOREADDRINFO-GETINFO-ERROR" && ex.arg.typeCode() == NT_HASH
                    && ex.arg.status.typeCode() == NT_STRING && DefiniteResolverStatuses{ex.arg.status}) {
                return True;
            }
            if (ex.arg.typeCode() == NT_HASH && ex.arg.err.typeCode() == NT_STRING
                    && DataProviderRetryUtil::isNameResolutionError(ex.arg)) {
                return True;
            }
            ex = ex.next.typeCode() == NT_HASH ? ex.next : NOTHING;
        }
        return False;
    }

    #! Returns @ref True if a request that failed with the given transport error should be retried
    /** @param ex the exception raised by the request
        @param retry_set the error codes of the transport errors that the provider retries

        @return @ref True if the error code is in \a retry_set, the error is not a name resolution failure (see
        @ref isNameResolutionError()) and retries are not suppressed in this thread (see @ref retriesSuppressed())

        @since DataProvider 3.11
    */
    static bool isRetriableTransportError(hash<auto> ex, hash<auto> retry_set) {
        return ex.err.typeCode() == NT_STRING && retry_set{ex.err}
            && !DataProviderRetryUtil::isNameResolutionError(ex)
            && !DataProviderRetryUtil::retriesSuppressed();
    }

    #! Returns @ref True if failed requests are not retried in this thread
    /** Retries are suppressed while @ref runWithoutRetries() runs in this thread; a provider must then not retry any
        error, including a rate-limited response.

        @since DataProvider 3.11
    */
    static bool retriesSuppressed() {
        return get_thread_data(SuppressRetriesKey) > 0;
    }

    #! Runs the given code with retries suppressed in this thread and returns its result
    /** Used by code that only builds example data and falls back to fake data when a request fails: such a request
        is made once, and its failure is not delayed by retries.

        Calls can be nested; retries stay suppressed until the outermost call returns, and the suppression ends then
        also when the code raises an exception.  Other threads are not affected.

        @param c the code to run

        @return the value returned by \a c

        @since DataProvider 3.11
    */
    static auto runWithoutRetries(code c) {
        int count = get_thread_data(SuppressRetriesKey) ?? 0;
        save_thread_data(SuppressRetriesKey, count + 1);
        on_exit {
            if (count) {
                save_thread_data(SuppressRetriesKey, count);
            } else {
                delete_thread_data(SuppressRetriesKey);
            }
        }
        return c();
    }

    #! Sets the unit of transport retry delays for all data providers in this process
    /** Each provider documents the delays between its transport retries as multiples of this unit (with the default
        unit of 1 second, for example, 2, 4 and 8 seconds).  A smaller unit shortens all of them proportionally, and
        a unit of zero retries without waiting.  Delays advised by the server, such as the \c Retry-After header of a
        rate-limited response, are not affected.

        As the setting applies to the whole process, the caller must not be restricted with
        @ref Qore::PO_NO_PROCESS_CONTROL.

        @param unit the new unit as a relative date/time value (for example \c 10ms); must not be negative or
        longer than @ref DefaultTransportRetryDelayUnit

        @throw PERMISSION-DENIED the caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
        @throw TRANSPORT-RETRY-DELAY-ERROR the unit is an absolute date/time value, is negative, or is longer than
        @ref DefaultTransportRetryDelayUnit

        @since DataProvider 3.11
    */
    static setTransportRetryDelayUnit(date unit) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", "Cannot change the transport retry delay unit of the process from a context "
                "with restricted process control";
        }
        if (!unit.relative()) {
            throw "TRANSPORT-RETRY-DELAY-ERROR", sprintf("the transport retry delay unit must be a relative date/time "
                "value; got the absolute date/time value %y", unit);
        }
        int unit_us = get_duration_microseconds(unit);
        if (unit_us < 0) {
            throw "TRANSPORT-RETRY-DELAY-ERROR", sprintf("the transport retry delay unit must not be negative; got "
                "%y", unit);
        }
        if (unit_us > get_duration_microseconds(DefaultTransportRetryDelayUnit)) {
            throw "TRANSPORT-RETRY-DELAY-ERROR", sprintf("the transport retry delay unit must not be longer than "
                "the default unit of %y; got %y", DefaultTransportRetryDelayUnit, unit);
        }
        transport_retry_delay_unit_us = unit_us;
    }

    #! Returns the unit of transport retry delays
    /** @return the unit of transport retry delays; @ref DefaultTransportRetryDelayUnit unless changed with
        @ref setTransportRetryDelayUnit()

        @since DataProvider 3.11
    */
    static date getTransportRetryDelayUnit() {
        return microseconds(transport_retry_delay_unit_us);
    }

    #! Returns the delay for the given number of units of the transport retry delay
    /** @param units the delay in units of @ref getTransportRetryDelayUnit(); a negative number gives no delay

        @return the delay; at most @ref MaxTransportRetryDelay

        @since DataProvider 3.11
    */
    static date getTransportRetryDelay(number units) {
        if (units <= 0) {
            return 0s;
        }
        # the delay is limited, also so that a huge number of units cannot give a delay that cannot be represented,
        # which would wrap around to a negative delay
        number us = units * transport_retry_delay_unit_us;
        int max_us = get_duration_microseconds(MaxTransportRetryDelay);
        return microseconds(us >= max_us ? max_us : us.toInt());
    }

    #! Waits before a transport retry for the given number of units of the transport retry delay
    /** @param units the delay in units of @ref getTransportRetryDelayUnit(); with the default unit, the delay in
        seconds

        @since DataProvider 3.11
    */
    static waitTransportRetry(number units) {
        date delay = DataProviderRetryUtil::getTransportRetryDelay(units);
        if (delay > 0s) {
            usleep(delay);
        }
    }
}
}
