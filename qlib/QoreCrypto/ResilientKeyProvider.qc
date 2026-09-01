# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ResilientKeyProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ResilientKeyProvider
public namespace QoreCrypto {

#! Circuit breaker closed: calls pass through to the delegate
public const KEY_PROVIDER_CIRCUIT_CLOSED = "closed";

#! Circuit breaker open: calls fail immediately without reaching the delegate
public const KEY_PROVIDER_CIRCUIT_OPEN = "open";

#! Circuit breaker half-open: a single trial call is allowed through
public const KEY_PROVIDER_CIRCUIT_HALF_OPEN = "half-open";

#! Circuit name used for provider-level calls that name no key generation
public const KEY_PROVIDER_CIRCUIT_PROVIDER = "*provider*";

#! Retry and circuit breaker policy for @ref ResilientKeyProvider
public hashdecl KeyProviderResilienceOptions {
    #! Total attempts per operation, including the first; must be >= 1
    int max_attempts = 3;
    #! Delay before the second attempt, in milliseconds
    int initial_delay_ms = 50;
    #! Ceiling for the backoff delay, in milliseconds
    int max_delay_ms = 2000;
    #! Multiplier applied to the delay after each failed attempt; must be >= 1.0
    float backoff_multiplier = 2.0;
    #! Consecutive transient failures that open the circuit; must be >= 1
    int failure_threshold = 5;
    #! How long the circuit stays open before a trial call, in milliseconds
    int open_ms = 30000;
    #! Classifies an exception as transient (retryable) or deterministic
    /** Defaults to @ref ResilientKeyProvider::isTransientError().
    */
    *code<bool(hash<ExceptionInfo>)> transient_predicate;
    #! Applies the backoff delay; defaults to usleep()
    /** Injected by tests so that retry behavior can be asserted without
        real elapsed time.
    */
    *code<nothing(int)> delay_handler;
    #! Returns the current time; defaults to now_us()
    /** Injected by tests so that circuit breaker timing can be asserted
        without real elapsed time.
    */
    *code<date()> clock;
}

#! Circuit breaker state for one key generation
public hashdecl KeyProviderCircuitState {
    #! The key generation, or @ref KEY_PROVIDER_CIRCUIT_PROVIDER
    string key_id;
    #! One of @ref KEY_PROVIDER_CIRCUIT_CLOSED, @ref KEY_PROVIDER_CIRCUIT_OPEN,
    #! or @ref KEY_PROVIDER_CIRCUIT_HALF_OPEN
    string state;
    #! @ref True while a half-open trial call is in flight
    bool trial_in_flight = False;
    #! Consecutive transient failures since the last success
    int consecutive_failures = 0;
    #! Total transient failures observed
    int total_failures = 0;
    #! Total successful calls observed
    int total_successes = 0;
    #! Number of times the circuit has opened
    int open_count = 0;
    #! When the circuit last opened
    *date opened;
    #! When a trial call will next be allowed
    *date retry_at;
}

#! Key provider decorator adding bounded retry and a circuit breaker
/** Wraps another @ref AbstractKeyProvider so that transient key provider
    failures - a KMS timeout, a dropped connection, a rate limit - are retried a
    bounded number of times, and so that a provider that is down stops absorbing
    the latency of every caller.

    @par Fails closed
    Every failure path throws.  This class never returns plaintext, never
    substitutes a different key, and never allows a caller to write unencrypted
    data because a provider was unavailable.  When the circuit is open, calls
    fail immediately with \c QORE-CRYPTO-PROVIDER-UNAVAILABLE rather than
    falling back to anything.

    @par Only transient failures are retried
    A deterministic cryptographic failure - the wrong key, mismatched AAD, a
    tampered record - is not retried and does not count toward opening the
    circuit.  Retrying it would be pointless, and letting a tampered record trip
    the breaker would turn one bad row into a provider-wide outage.  The default
    classifier treats every \c QORE-CRYPTO-* error as deterministic and
    everything else as transient; supply
    @ref KeyProviderResilienceOptions::transient_predicate to match a specific
    provider's error taxonomy.

    @par Per-key circuits
    Circuit state is tracked per key generation, so a key that has been revoked
    or is not yet replicated cannot take down access to the rest of the keyring.

    @par Thread safety
    Safe to share between threads.  The delegate and the policy are immutable
    after construction; circuit state is guarded by an internal lock that is
    never held across a call into the delegate.

    @par Example
    @code{.py}
ResilientKeyProvider provider(kms_provider, <KeyProviderResilienceOptions>{
    "max_attempts": 4,
    "failure_threshold": 3,
});
EnvelopeCipher cipher(provider);
    @endcode

    @see @ref qorecrypto_resilience
*/
public class ResilientKeyProvider inherits AbstractKeyProvider {
    private {
        #! the wrapped provider
        AbstractKeyProvider delegate;
        #! effective policy
        hash<KeyProviderResilienceOptions> opts;
        #! transient error classifier
        code<bool(hash<ExceptionInfo>)> is_transient;
        #! backoff delay handler
        code<nothing(int)> delay_handler;
        #! time source
        code<date()> clock;
        #! guards circuits
        Mutex lck();
        #! circuit breaker state by key generation
        hash<string, hash<KeyProviderCircuitState>> circuits;
    }

    #! Creates the decorator
    /** @param delegate the key provider to wrap
        @param options optional retry and circuit breaker policy

        @throw QORE-CRYPTO-RESILIENCE-ERROR the policy is invalid
    */
    constructor(AbstractKeyProvider delegate, *hash<KeyProviderResilienceOptions> options) {
        self.delegate = delegate;
        opts = options ?? <KeyProviderResilienceOptions>{};
        validateOptions();

        is_transient = opts.transient_predicate ?? bool sub (hash<ExceptionInfo> ex) {
            return ResilientKeyProvider::isTransientError(ex);
        };
        delay_handler = opts.delay_handler ?? nothing sub (int delay_ms) {
            usleep(delay_ms * 1000);
        };
        clock = opts.clock ?? date sub () {
            return now_us();
        };
    }

    #! Returns the wrapped provider's identifier
    /** Delegated directly; @ref AbstractKeyProvider requires this to be a pure
        local accessor with no failure modes, so there is nothing to retry.
    */
    string getProviderId() {
        return delegate.getProviderId();
    }

    #! Returns the wrapped provider's current key identifier
    /** Guarded by the @ref KEY_PROVIDER_CIRCUIT_PROVIDER circuit, since a
        provider that resolves its current key remotely can fail here too.
    */
    string getCurrentKeyId() {
        return callWithResilience(KEY_PROVIDER_CIRCUIT_PROVIDER, string sub () {
            return delegate.getCurrentKeyId();
        });
    }

    hash<WrappedKeyInfo> wrapKey(binary dek, *string key_id, string aad) {
        string effective_key_id = key_id ?? getCurrentKeyId();
        return callWithResilience(effective_key_id, hash<WrappedKeyInfo> sub () {
            return delegate.wrapKey(dek, effective_key_id, aad);
        });
    }

    binary unwrapKey(hash<WrappedKeyInfo> info, string aad) {
        return callWithResilience(info.key_id, binary sub () {
            return delegate.unwrapKey(info, aad);
        });
    }

    #! Returns the effective policy
    hash<KeyProviderResilienceOptions> getOptions() {
        return opts;
    }

    #! Returns circuit breaker state for every key generation seen so far
    hash<string, hash<KeyProviderCircuitState>> getCircuits() {
        AutoLock al(lck);
        return circuits;
    }

    #! Returns circuit breaker state for one key generation
    /** @param key_id the key generation; defaults to
        @ref KEY_PROVIDER_CIRCUIT_PROVIDER

        @return the circuit state; a generation with no recorded activity
        reports as closed
    */
    hash<KeyProviderCircuitState> getCircuitState(*string key_id) {
        string effective_key_id = key_id ?? KEY_PROVIDER_CIRCUIT_PROVIDER;
        AutoLock al(lck);
        return getCircuitLocked(effective_key_id);
    }

    #! Clears all circuit breaker state
    /** Intended for administrative recovery after a provider outage has been
        resolved out of band.
    */
    resetCircuits() {
        AutoLock al(lck);
        circuits = {};
    }

    #! Default transient error classifier
    /** Treats every \c QORE-CRYPTO-* error as a deterministic cryptographic
        failure that must not be retried, and every other error as transient.

        @param ex the exception raised by the delegate

        @return @ref True if the error should be retried
    */
    static bool isTransientError(hash<ExceptionInfo> ex) {
        string err = ex.err.toString();
        return !(err =~ /^QORE-CRYPTO-/);
    }

    #! Runs an operation with bounded retry under a circuit breaker
    private auto callWithResilience(string circuit_key, code<auto()> operation) {
        checkCircuit(circuit_key);
        # release the half-open trial slot on every exception path, including one
        # raised by an injected delay handler; recordSuccess() and recordFailure()
        # release it on the paths they own, and clearTrial() is then a no-op
        on_error clearTrial(circuit_key);

        int attempt = 0;
        int delay_ms = opts.initial_delay_ms;
        while (True) {
            ++attempt;
            try {
                auto rv = operation();
                recordSuccess(circuit_key);
                return rv;
            } catch (hash<ExceptionInfo> ex) {
                # a deterministic cryptographic failure is never retried and must
                # not open the circuit; one tampered record is not an outage
                # a deterministic failure says nothing about provider health, so the
                # trial slot is released (by on_error) and the next caller retries it
                if (!is_transient(ex)) {
                    rethrow;
                }

                bool opened = recordFailure(circuit_key);
                if (attempt >= opts.max_attempts || opened) {
                    throw "QORE-CRYPTO-PROVIDER-UNAVAILABLE",
                        sprintf("key provider %y is unavailable for key %y after %d attempt%s "
                            "(circuit %s): %s: %s", getProviderId(), circuit_key, attempt,
                            attempt == 1 ? "" : "s", opened ? KEY_PROVIDER_CIRCUIT_OPEN
                                : KEY_PROVIDER_CIRCUIT_CLOSED, ex.err, ex.desc), ex;
                }

                delay_handler(delay_ms);
                delay_ms = (delay_ms * opts.backoff_multiplier).toInt();
                if (delay_ms > opts.max_delay_ms) {
                    delay_ms = opts.max_delay_ms;
                }
            }
        }
    }

    #! Throws if the circuit for the given key generation is not accepting calls
    /** Admits at most one half-open trial call at a time.  Without that limit
        every thread blocked on a failing provider would be released at the same
        moment the cooldown elapsed, which is the load the circuit breaker exists
        to prevent.
    */
    private checkCircuit(string circuit_key) {
        AutoLock al(lck);
        hash<KeyProviderCircuitState> circuit = getCircuitLocked(circuit_key);

        if (circuit.state == KEY_PROVIDER_CIRCUIT_OPEN) {
            date now = clock();
            if (exists circuit.retry_at && now < circuit.retry_at) {
                throw "QORE-CRYPTO-PROVIDER-UNAVAILABLE",
                    sprintf("key provider %y circuit is open for key %y until %y; failing closed",
                        getProviderId(), circuit_key, circuit.retry_at);
            }
            # the cooldown has elapsed: admit a trial call
            circuit.state = KEY_PROVIDER_CIRCUIT_HALF_OPEN;
        }

        if (circuit.state == KEY_PROVIDER_CIRCUIT_HALF_OPEN) {
            if (circuit.trial_in_flight) {
                throw "QORE-CRYPTO-PROVIDER-UNAVAILABLE",
                    sprintf("key provider %y circuit is half-open for key %y with a trial call "
                        "already in flight; failing closed", getProviderId(), circuit_key);
            }
            circuit.trial_in_flight = True;
            circuits{circuit_key} = circuit;
        }
    }

    #! Releases a half-open trial slot without recording a health verdict
    private clearTrial(string circuit_key) {
        AutoLock al(lck);
        *hash<KeyProviderCircuitState> circuit = circuits{circuit_key};
        if (!exists circuit || !circuit.trial_in_flight) {
            return;
        }
        circuit.trial_in_flight = False;
        circuits{circuit_key} = circuit;
    }

    #! Records a successful call, closing the circuit
    private recordSuccess(string circuit_key) {
        AutoLock al(lck);
        hash<KeyProviderCircuitState> circuit = getCircuitLocked(circuit_key);
        circuit.state = KEY_PROVIDER_CIRCUIT_CLOSED;
        circuit.trial_in_flight = False;
        circuit.consecutive_failures = 0;
        ++circuit.total_successes;
        circuit.opened = NOTHING;
        circuit.retry_at = NOTHING;
        circuits{circuit_key} = circuit;
    }

    #! Records a transient failure
    /** @return @ref True if this failure opened the circuit
    */
    private bool recordFailure(string circuit_key) {
        AutoLock al(lck);
        hash<KeyProviderCircuitState> circuit = getCircuitLocked(circuit_key);
        ++circuit.consecutive_failures;
        ++circuit.total_failures;

        # a failed trial call re-opens immediately; the provider is still down
        bool opened = circuit.state == KEY_PROVIDER_CIRCUIT_HALF_OPEN
            || circuit.consecutive_failures >= opts.failure_threshold;
        circuit.trial_in_flight = False;
        if (opened) {
            date now = clock();
            circuit.state = KEY_PROVIDER_CIRCUIT_OPEN;
            circuit.opened = now;
            circuit.retry_at = now + milliseconds(opts.open_ms);
            ++circuit.open_count;
        }
        circuits{circuit_key} = circuit;
        return opened;
    }

    #! Returns circuit state for a key generation, creating it if needed
    /** The caller must hold @ref lck.
    */
    private hash<KeyProviderCircuitState> getCircuitLocked(string circuit_key) {
        *hash<KeyProviderCircuitState> circuit = circuits{circuit_key};
        if (exists circuit) {
            return circuit;
        }
        return <KeyProviderCircuitState>{
            "key_id": circuit_key,
            "state": KEY_PROVIDER_CIRCUIT_CLOSED,
        };
    }

    #! Validates the retry and circuit breaker policy
    private validateOptions() {
        if (opts.max_attempts < 1) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("max_attempts must be at least 1, got %d", opts.max_attempts);
        }
        if (opts.initial_delay_ms < 0) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("initial_delay_ms must not be negative, got %d", opts.initial_delay_ms);
        }
        if (opts.max_delay_ms < opts.initial_delay_ms) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("max_delay_ms (%d) must not be less than initial_delay_ms (%d)",
                    opts.max_delay_ms, opts.initial_delay_ms);
        }
        if (opts.backoff_multiplier < 1.0) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("backoff_multiplier must be at least 1.0, got %y", opts.backoff_multiplier);
        }
        if (opts.failure_threshold < 1) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("failure_threshold must be at least 1, got %d", opts.failure_threshold);
        }
        if (opts.open_ms < 0) {
            throw "QORE-CRYPTO-RESILIENCE-ERROR",
                sprintf("open_ms must not be negative, got %d", opts.open_ms);
        }
    }
}

}
