# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RestartableTransaction.qc Restart policy for recoverable database transactions

/** RestartableTransaction.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace SqlUtil {

#! PostgreSQL SQLSTATEs that require a complete transaction restart
const PgsqlRestartTransactionCodes = {
    "08000": True,
    "08003": True,
    "08006": True,
    "08001": True,
    "08004": True,
    "08007": True,
    "08P01": True,
};

#! Returns a diagnostic for a driver error requiring transaction restart
/** @param driver database driver name
    @param ex one exception in a caught exception chain
    @return a language-independent diagnostic when the complete transaction
        may be restarted, otherwise @ref NOTHING

    @since SqlUtil 3.2
*/
public *string sub get_restart_transaction_reason(string driver,
        hash<ExceptionInfo> ex) {
    switch (driver) {
        case "oracle": {
            *string code = (ex.desc =~
                x/ORA-(01041|2540[1-9]|0311[34]|00060)/)[0];
            if (code) {
                return sprintf("recoverable Oracle transaction error "
                    "ORA-%s: %s: %s", code, ex.err, ex.desc);
            }
            return NOTHING;
        }
        case "pgsql": {
            if (ex.err == "DBI:PGSQL:CONNECTION-ERROR"
                    || (ex.err == "DBI:PGSQL:ERROR"
                        && (PgsqlRestartTransactionCodes{
                                ex.arg.alterr.toString()}
                            || ex.desc =~ /Connection refused/
                            || ex.desc =~ /in a read-only transaction/))) {
                return sprintf("recoverable PostgreSQL transaction error "
                    "(SQLSTATE %s): %s: %s",
                    ex.arg.alterr.val() ? ex.arg.alterr.toString()
                        : "not available", ex.err, ex.desc);
            }
            return NOTHING;
        }
        case "mysql":
            return NOTHING;
    }
    return NOTHING;
}

#! Implements a bounded restart policy for recoverable database transactions
/** The caller owns the transaction body and retries it only when
    restartTransaction() returns a diagnostic string.  The policy recognizes
        generic driver failover errors through
        @ref get_restart_transaction_reason() and
    datasource-pool failover/timeout errors.  It never retries an unrelated
    exception and never retries more than the configured maximum.

    @par Example
    A restartable ingestion batch owns its transaction boundary and retries
    only a complete idempotent batch:
    @code{.py}
RestartableTransaction policy(table.getDriverName(), 5, 2s, 1);
while (True) {
    try {
        on_error table.rollback();
        on_success table.commit();
        table.beginTransaction();
        map table.upsert($1), rows;
        policy.reset();
        break;
    } catch (hash<ExceptionInfo> ex) {
        if (!policy.restartTransaction(ex)) {
            rethrow;
        }
    }
}
    @endcode

    @since SqlUtil 3.2
*/
public class RestartableTransaction {
    private {
        string driver;
        int count = 0;
        int max_retries;
        date retry_delay;
        int immediate_retries;
        *code<nothing(string)> log_callback;
    }

    #! Creates a bounded restart policy
    /** @param driver database driver name
        @param max_retries maximum recoverable retries
        @param retry_delay delay after the immediate retry allowance
        @param immediate_retries number of retries with no delay
        @param log optional diagnostic callback

        @throw RESTARTABLE-TRANSACTION-ARG-ERROR if the driver is empty or
            retry bounds are invalid
    */
    constructor(string driver, int max_retries = 10,
            date retry_delay = 10s, int immediate_retries = 2,
            *code<nothing(string)> log) {
        if (!driver.val()) {
            throw "RESTARTABLE-TRANSACTION-ARG-ERROR",
                "driver must not be empty";
        }
        if (max_retries < 0 || max_retries > 1000) {
            throw "RESTARTABLE-TRANSACTION-ARG-ERROR",
                "max_retries must be between 0 and 1000";
        }
        if (retry_delay < 0s) {
            throw "RESTARTABLE-TRANSACTION-ARG-ERROR",
                "retry_delay must not be negative";
        }
        if (immediate_retries < 0 || immediate_retries > max_retries) {
            throw "RESTARTABLE-TRANSACTION-ARG-ERROR",
                "immediate_retries must be between 0 and max_retries";
        }
        self.driver = driver;
        self.max_retries = max_retries;
        self.retry_delay = retry_delay;
        self.immediate_retries = immediate_retries;
        log_callback = log;
    }

    #! Returns a restart diagnostic for a recoverable transaction exception
    /** @param original original exception chain
        @return a diagnostic when the caller may restart the complete
            transaction body, otherwise @ref NOTHING

        @note The complete transaction body must be idempotent.  The caller
            must roll back failed attempts before calling this method.
    */
    *string restartTransaction(hash<ExceptionInfo> original) {
        *string reason = recoverableReason(original);
        if (!reason || count >= max_retries) {
            return NOTHING;
        }
        ++count;
        date delay = count > immediate_retries ? retry_delay : 0s;
        string diagnostic = sprintf(
            "transaction restart %d of %d%s: %s", count, max_retries,
            delay ? sprintf(" after %y", delay) : "", reason);
        if (log_callback) {
            log_callback(diagnostic);
        }
        if (delay) {
            sleep(delay);
        }
        return diagnostic;
    }

    #! Returns the number of restart decisions made
    /** @return number of recoverable retry decisions since construction or
            the last @ref reset()

        @since SqlUtil 3.2
    */
    int getCount() {
        return count;
    }

    #! Resets the policy after a successful transaction
    /** @since SqlUtil 3.2
    */
    reset() {
        count = 0;
    }

    private *string recoverableReason(hash<ExceptionInfo> original) {
        hash<ExceptionInfo> ex = original;
        while (True) {
            if (ex.err == "DATASOURCEPOOL-PROCESS-ERROR") {
                return "datasource pool server failover";
            }
            if (ex.err == "DATASOURCEPOOL-TIMEOUT") {
                return "datasource pool access timeout";
            }
            if (*string reason = get_restart_transaction_reason(driver, ex)) {
                return reason;
            }
            if (!ex.next) {
                return NOTHING;
            }
            ex = ex.next;
        }
    }
}

} # namespace SqlUtil
