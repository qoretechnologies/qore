/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp3ClientConnection.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include "qore/intern/QoreChannel.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/QC_Http3ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QuicSession.h"

#include <cstdio>

#include <sys/socket.h>
#include <netdb.h>

// Happy-eyeballs Connection Attempt Delay (RFC 8305 §8).  250ms is the
// RFC-recommended default; the secondary address-family attempt holds
// its first packet until this many nanoseconds after the primary's
// submission, giving the preferred family a head start on the handshake
// without blocking fallback when the preferred path is a black hole.
static constexpr int64_t HE_CONNECTION_ATTEMPT_DELAY_NS = 250'000'000LL;

// QPP-generated symbols — forward-declared for the single-compilation-unit
extern QoreClass* QC_HTTP3CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP3CLIENTPOLLOPERATIONBASE;

//! Resolve the target hostname and return a list of candidate address
//! families to attempt for happy-eyeballs (RFC 8305).
/** Probes the hostname with @c AI_NUMERICHOST first (fast path for literal
    IPs); falls back to a full resolution when the target is a name.  Returns
    at most one AF_INET6 and one AF_INET entry in the order mandated by
    RFC 8305 §4 (v6 before v4), so the primary attempt is v6 when both
    exist — matching the TCP path's @c sortAddressesHappyEyeballs behavior.
    Returns a single-entry vector when only one family resolves, and a
    fallback @c AF_INET entry when resolution fails entirely (preserves
    the legacy default — subsequent @c SocketQuicClientPollOperation will
    re-resolve and surface any getaddrinfo error).
*/
static std::vector<int> resolveCandidateFamilies(const char* host) {
    std::vector<int> out;
    bool have_v6 = false;
    bool have_v4 = false;

    // Fast path: literal IP address — only its own family is reachable.
    {
        struct addrinfo hints{};
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;
        struct addrinfo* res = nullptr;
        int rc = getaddrinfo(host, nullptr, &hints, &res);
        if (rc == 0 && res) {
            out.push_back(res->ai_family);
            freeaddrinfo(res);
            return out;
        }
        if (res) {
            freeaddrinfo(res);
        }
    }

    // Slow path: hostname — enumerate all resolved families and mark
    // v6/v4 availability for interleaving.
    {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        struct addrinfo* res = nullptr;
        int rc = getaddrinfo(host, nullptr, &hints, &res);
        if (rc == 0) {
            for (struct addrinfo* p = res; p; p = p->ai_next) {
                if (p->ai_family == AF_INET6) {
                    have_v6 = true;
                } else if (p->ai_family == AF_INET) {
                    have_v4 = true;
                }
            }
        }
        if (res) {
            freeaddrinfo(res);
        }
    }

    // RFC 8305 §4: v6 first, v4 second.
    if (have_v6) {
        out.push_back(AF_INET6);
    }
    if (have_v4) {
        out.push_back(AF_INET);
    }
    if (out.empty()) {
        out.push_back(AF_INET);  // fallback — preserves legacy default
    }
    return out;
}

Http3ClientConnection::Http3ClientConnection(const char* target_host, int target_port,
        int max_concurrent_streams, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr,
        int ssl_verify_mode,
        bool ssl_accept_all_certs,
        QoreSSLCertificate* client_cert,
        QoreSSLPrivateKey* client_key)
    : HttpClientConnectionBase(target_host, target_port, /* ssl_required (always for H3) */ true),
      max_concurrent_streams_(max_concurrent_streams),
      ssl_verify_mode_(ssl_verify_mode),
      ssl_accept_all_certs_(ssl_accept_all_certs),
      client_cert_(client_cert),
      client_key_(client_key) {
    if (client_cert_) {
        client_cert_->ref();
    }
    if (client_key_) {
        client_key_->ref();
    }
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmit(xsink)) {
        return;
    }
}

Http3ClientConnection::~Http3ClientConnection() {
    ExceptionSink xsink;
    closeConnection(&xsink);
    if (poll_op_obj) {
        poll_op_obj->deref(&xsink);
        poll_op_obj = nullptr;
    }
    if (sock_obj) {
        sock_obj->deref(&xsink);
        sock_obj = nullptr;
    }
    if (client_cert_) {
        client_cert_->deref();
        client_cert_ = nullptr;
    }
    if (client_key_) {
        client_key_->deref();
        client_key_ = nullptr;
    }
    poll_op_priv = nullptr;
    sock_priv = nullptr;
    xsink.clear();
}

int Http3ClientConnection::buildAttempt(int family, int64_t not_before_ns_abs,
        ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // 1. Create UDP socket.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;

    // 1a. Bind UDP socket to an ephemeral port in the requested family.
    const char* bind_addr = (family == AF_INET6) ? "::" : "0.0.0.0";
    int bind_rc = sock_priv_raw->bindINET(bind_addr, "0", true, family, SOCK_DGRAM,
        0, xsink);
    if (bind_rc < 0 || *xsink) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "failed to bind UDP socket for QUIC connection to %s:%d",
                target_host.c_str(), target_port);
        }
        return -1;
    }

    // 1b. Propagate HTTPClient's SSL config onto the fresh UDP socket —
    //     QuicSession::createClient reads ssl_verify_mode /
    //     accept_all_certs / cert / pk directly from this socket during
    //     the TLS 1.3 handshake that is integrated into QUIC.  Without
    //     this, H3 auto-upgrade always handshakes with SSL_VERIFY_NONE,
    //     breaking mTLS (clientCertTest) and other verify-mode paths.
    if (ssl_verify_mode_) {
        sock_priv_raw->setSslVerifyMode(ssl_verify_mode_);
    }
    if (ssl_accept_all_certs_) {
        sock_priv_raw->acceptAllCertificates(true);
    }
    if (client_cert_ && client_key_) {
        client_cert_->ref();
        client_key_->ref();
        sock_priv_raw->setCertificateAndPrivateKey(client_cert_, client_key_);
    } else {
        if (client_cert_) {
            client_cert_->ref();
            sock_priv_raw->setCertificate(client_cert_);
        }
        if (client_key_) {
            client_key_->ref();
            sock_priv_raw->setPrivateKey(client_key_);
        }
    }

    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // 2. Create the SocketQuicClientPollOperation (QUIC handshake inner op).
    sock_priv_raw->ref();
    ReferenceHolder<SocketQuicClientPollOperation> inner_op(
        new SocketQuicClientPollOperation(xsink, sock_priv_raw,
            target_host.c_str(), static_cast<uint16_t>(target_port), family,
            /* handshake_timeout_ns */ 0,
            /* not_before_ns_abs */ not_before_ns_abs),
        xsink);
    if (*xsink) {
        return -1;
    }

    // 3. Create the Http3ClientPollOperationPriv.
    sock_priv_raw->ref();
    SocketQuicClientPollOperation* inner_ptr = inner_op.release();
    ReferenceHolder<Http3ClientPollOperationPriv> priv_holder(
        new Http3ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* inner */ inner_ptr,
            /* conn_priv */ this),
        xsink);
    Http3ClientPollOperationPriv* priv_raw = *priv_holder;

    // 4. Wrap in QoreObject.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP3CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // 5. Set self references.
    priv_raw->setSelf(*poll_obj_holder);
    inner_ptr->setSelf(*poll_obj_holder);

    // 6. Set QoreObject members.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http3_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // 7. Record the attempt so the caller can submit it (or tear it down
    //    on error) — submission is deferred until the full list of
    //    attempts is known so that onInnerHandshakeReady/Failed callbacks
    //    firing from the I/O thread see a complete attempts_ vector.
    auto a = std::make_unique<Attempt>();
    a->sock_priv = sock_priv_raw;
    a->sock_obj = sock_obj_holder.release();
    a->poll_op_priv = priv_raw;
    a->poll_op_obj = poll_obj_holder.release();
    a->family = family;

    // Arm the HE observer BEFORE submission so a racing I/O thread finds
    // the owner set on the first handshake outcome.
    priv_raw->armHappyEyeballsOwner(this);

    std::lock_guard<std::mutex> lk(attempts_mu_);
    int idx = static_cast<int>(attempts_.size());
    attempts_.push_back(std::move(a));
    return idx;
}

int Http3ClientConnection::submitAttempt(Attempt& a, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> ctl_obj_holder(
        qore_get_async_io_controller_obj(xsink), xsink);
    if (*xsink || !ctl_obj_holder) {
        return -1;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
        static_cast<AsyncIoControllerPriv*>(
            ctl_obj_holder->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)),
        xsink);
    if (*xsink || !ctl_priv_holder) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "failed to get AsyncIoController singleton");
        }
        return -1;
    }

    char owner_buf[64];
    const char* owner_to_use;
    if (!owner_str.empty()) {
        // When the caller provided an owner string, suffix the attempt's
        // family so concurrent HE submissions hit distinct cache keys.
        snprintf(owner_buf, sizeof(owner_buf), "%s-%s",
            owner_str.c_str(),
            (a.family == AF_INET6) ? "v6" : "v4");
        owner_to_use = owner_buf;
    } else {
        snprintf(owner_buf, sizeof(owner_buf), "http3-cpp-conn-%p-%s",
            static_cast<const void*>(&a),
            (a.family == AF_INET6) ? "v6" : "v4");
        owner_to_use = owner_buf;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", a.sock_obj->refSelf(), xsink);
    info->setKeyValue("spop", a.poll_op_obj->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_to_use), xsink);
    // Disable controller-level timeout; the H3 poll op manages its own idle timeout.
    info->setKeyValue("to", -1, xsink);
    if (*xsink) {
        return -1;
    }

    // submit() takes ownership of `info` via the ReferenceHolder in
    // AsyncIoControllerPriv::submit; use .release() not *info.
    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, info.release(), false, xsink), xsink);
    if (*xsink) {
        return -1;
    }
    a.submitted = true;
    return 0;
}

void Http3ClientConnection::clearAttempt(Attempt& a, ExceptionSink* xsink) {
    // Disarm both back-pointers on the poll op so the subsequent abort()
    // does NOT call setClosed on the connection (we're a losing attempt —
    // the connection may still be racing or already committed to a
    // different winner).
    if (a.poll_op_priv) {
        a.poll_op_priv->disarmConnectionPriv();
    }

    // Cancel strategy: call abort() directly instead of the controller's
    // cancel() path.  cancel() erases the entry from the I/O thread's
    // cache, which is UNSAFE to do from within continuePoll because the
    // caller's ops_to_poll batch still holds a raw pointer to the erased
    // PollInfo's spop_base — the next batch iteration would dereference a
    // freed pointer and segfault.  abort() instead just transitions the
    // poll op to CLOSED (h3_state.store + inner_op->abort which closes
    // the UDP socket); the controller's Phase3 logic picks up the CLOSED
    // state via needsCloseOnComplete() on the next iteration and cleans
    // up the cache entry safely after the batch has finished.
    //
    // Safe from any thread including I/O thread:
    //   - abort() takes loser's own sock_priv->m and stream_lock; these
    //     are distinct from the winner's locks, so no self-deadlock when
    //     called from within the winner's continuePoll.
    //   - After disarmConnectionPriv, connection_priv and h3_owner_ are
    //     nullptr, so abort's setClosed-path becomes a no-op — no state
    //     transition on the enclosing connection.
    if (a.poll_op_priv) {
        a.poll_op_priv->abort(xsink);
    }

    // Deref the QoreObject wrappers (our construction-time refs).  The
    // controller still holds its own ref via the PollInfo until it
    // processes the CLOSED state, so these derefs just drop OUR ref —
    // the objects stay alive until the controller's cleanup.
    if (a.poll_op_obj) {
        a.poll_op_obj->deref(xsink);
        a.poll_op_obj = nullptr;
    }
    if (a.sock_obj) {
        a.sock_obj->deref(xsink);
        a.sock_obj = nullptr;
    }
    a.poll_op_priv = nullptr;
    a.sock_priv = nullptr;
    a.submitted = false;
    a.finished = true;
}

int Http3ClientConnection::buildAndSubmit(ExceptionSink* xsink) {
    // Resolve all candidate address families per RFC 8305.  For a typical
    // dual-stack hostname this returns {AF_INET6, AF_INET} — two parallel
    // QUIC handshake attempts, the secondary staggered by
    // HE_CONNECTION_ATTEMPT_DELAY_NS.  For literal addresses or single-
    // family hosts, exactly one attempt is built and behavior matches the
    // pre-HE single-attempt path.
    std::vector<int> families = resolveCandidateFamilies(target_host.c_str());

    int64_t not_before = 0;
    std::vector<int> built_indices;
    built_indices.reserve(families.size());
    for (size_t i = 0; i < families.size(); ++i) {
        int idx = buildAttempt(families[i], not_before, xsink);
        if (idx < 0 || *xsink) {
            // One family failed to build (e.g., kernel rejected the bind).
            // If we already have a usable attempt, silently drop this one
            // and proceed with the remaining candidates — HE degrades
            // gracefully to N-1 attempts.  Otherwise surface the error.
            if (!built_indices.empty()) {
                xsink->clear();
                continue;
            }
            return -1;
        }
        built_indices.push_back(idx);
        // Subsequent attempts get staggered per RFC 8305 §8.
        if (not_before == 0) {
            not_before = QuicSession::timestamp() + HE_CONNECTION_ATTEMPT_DELAY_NS;
        } else {
            not_before += HE_CONNECTION_ATTEMPT_DELAY_NS;
        }
    }
    if (built_indices.empty()) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "failed to build any QUIC handshake attempt for %s:%d",
                target_host.c_str(), target_port);
        }
        return -1;
    }

    // Submit all built attempts to the controller.  Each attempt's
    // SocketQuicClientPollOperation honors its own not_before_ns_ gate, so
    // the secondary stays silent on the wire until its deadline elapses.
    //
    // Snapshot raw Attempt pointers under the lock, then submit outside —
    // submit() is a cross-thread cmd enqueue that can block briefly, so
    // doing it under attempts_mu_ would serialize unnecessarily and could
    // deadlock if the I/O thread tries to call onInnerHandshakeFailed
    // before the submission loop finishes.
    std::vector<Attempt*> to_submit;
    {
        std::lock_guard<std::mutex> lk(attempts_mu_);
        for (int idx : built_indices) {
            to_submit.push_back(attempts_[idx].get());
        }
    }
    int submitted_count = 0;
    for (Attempt* a : to_submit) {
        ExceptionSink submit_xsink;
        if (submitAttempt(*a, &submit_xsink) == 0) {
            ++submitted_count;
        } else {
            submit_xsink.clear();
            ExceptionSink clr_xsink;
            clearAttempt(*a, &clr_xsink);
            clr_xsink.clear();
        }
    }
    if (submitted_count == 0) {
        xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
            "no QUIC handshake attempts could be submitted for %s:%d",
            target_host.c_str(), target_port);
        return -1;
    }

    // Publish the primary attempt's tuple on the connection members so
    // closeConnection (pre-winner) has something to cancel.  The winner
    // selection in onInnerHandshakeReady may re-point these to a
    // different attempt.
    //
    // The submit loop above ran without attempts_mu_ held, so the I/O
    // thread may have already run onInnerHandshakeReady (winner committed
    // and attempts_ cleared) or onInnerHandshakeFailed (all-failed
    // teardown: attempts_ moved out and connection members nulled).  In
    // either of those cases, attempts_[built_indices.front()] is null or
    // out-of-bounds and the connection members are already in their final
    // state — re-publishing would either segfault on a null Attempt* or
    // overwrite the winner's tuple with stale pointers.  Guard against
    // both by checking winner_idx_ and attempts_ first.
    {
        std::lock_guard<std::mutex> lk(attempts_mu_);
        if (winner_idx_ < 0 && !attempts_.empty()) {
            Attempt* primary = attempts_[built_indices.front()].get();
            if (primary) {
                sock_priv = primary->sock_priv;
                sock_obj = primary->sock_obj;
                poll_op_priv = primary->poll_op_priv;
                poll_op_obj = primary->poll_op_obj;
                submitted_to_controller = true;
            }
        } else if (winner_idx_ >= 0) {
            // Winner committed by onInnerHandshakeReady before we
            // re-acquired the lock.  sock_priv et al. already point at
            // the winner's tuple; just flag the controller as holding
            // our op so a subsequent closeConnection cancels it via the
            // controller path.
            submitted_to_controller = true;
        }
        // else: onInnerHandshakeFailed cleared attempts_ and set
        // submitted_to_controller=false on its way to setClosed().  The
        // connection is already torn down — leave it that way.
    }
    return 0;
}

void Http3ClientConnection::onInnerHandshakeReady(Http3ClientPollOperationPriv* inner) {
    std::unique_lock<std::mutex> lk(attempts_mu_);

    // Already committed a winner? Losing attempt — just let clearAttempt
    // (from the original winner path) finish us off; don't transition the
    // base-class state again.
    if (winner_idx_ >= 0) {
        return;
    }

    // Locate this inner in attempts_ and commit as the winner.
    int my_idx = -1;
    for (size_t i = 0; i < attempts_.size(); ++i) {
        if (attempts_[i] && attempts_[i]->poll_op_priv == inner) {
            my_idx = static_cast<int>(i);
            break;
        }
    }
    if (my_idx < 0) {
        // Not found — must have been torn down already.  Nothing to do;
        // base-class state is still CONNECTING or already CLOSED.
        return;
    }
    winner_idx_ = my_idx;
    attempts_[my_idx]->finished = true;

    // Publish the winner's tuple as the primary so submitRequest / close
    // paths use it henceforth.  The members may currently point at a
    // different attempt (the "primary" chosen at build time); reassigning
    // here is safe because we're still in CONNECTING — no request has
    // been dispatched.  The loser attempts' QoreObject refs are cleaned
    // up below via clearAttempt, which disarms their back-pointers so
    // their abort() does NOT call setClosed on us.
    //
    // Null the winner's Attempt members as we transfer — its unique_ptr
    // will be destroyed below when we clear attempts_, and the Attempt
    // destructor asserts on non-null QoreObject pointers (see
    // QoreHttp3ClientConnection.h) to catch future double-ownership bugs.
    Attempt* w = attempts_[my_idx].get();
    sock_priv = w->sock_priv;        w->sock_priv = nullptr;
    sock_obj = w->sock_obj;          w->sock_obj = nullptr;
    poll_op_priv = w->poll_op_priv;  w->poll_op_priv = nullptr;
    poll_op_obj = w->poll_op_obj;    w->poll_op_obj = nullptr;

    // Move the losers out of attempts_ into a local vector so we can
    // release the mutex before calling clearAttempt (which invokes
    // ctl_priv_holder->cancel — a potentially blocking cross-thread
    // operation that must not be held under attempts_mu_).
    std::vector<std::unique_ptr<Attempt>> losers;
    const size_t winner_idx_sz = static_cast<size_t>(my_idx);
    for (size_t i = 0; i < attempts_.size(); ++i) {
        if (i != winner_idx_sz && attempts_[i]) {
            losers.push_back(std::move(attempts_[i]));
        }
    }
    // Release the winner from attempts_ so the destructor doesn't double-
    // free its QoreObjects — the connection members hold them now.
    attempts_.clear();
    lk.unlock();

    ExceptionSink xsink;
    for (auto& loser : losers) {
        if (loser) {
            clearAttempt(*loser, &xsink);
        }
    }
    xsink.clear();

    // Finally, signal the base-class condition variable so the caller's
    // waitForReadyOrError returns success.
    onConnectionReady();
}

void Http3ClientConnection::onInnerHandshakeFailed(Http3ClientPollOperationPriv* inner,
        const char* err, const char* desc) {
    std::unique_lock<std::mutex> lk(attempts_mu_);

    // Record the error details so that, if all attempts fail, the caller
    // sees the most recent failure rather than a generic "closed" message.
    last_err_ = err ? err : "HTTP3-CONNECT-ERROR";
    last_desc_ = desc ? desc : "QUIC handshake failed";

    // Mark this attempt finished.  If the winner has already been chosen,
    // this is just a stale losing-attempt failure — ignore.
    if (winner_idx_ >= 0) {
        return;
    }
    int remaining = 0;
    for (auto& a : attempts_) {
        if (a && a->poll_op_priv == inner) {
            a->finished = true;
        }
        if (a && !a->finished) {
            ++remaining;
        }
    }
    if (remaining > 0) {
        // Other attempts are still racing — keep the connection CONNECTING.
        return;
    }

    // All attempts have failed.  Move them out of attempts_ under the
    // lock, then tear down outside the lock.  This (a) releases the UDP
    // sockets promptly instead of waiting for closeConnection or the
    // destructor, and (b) nulls the connection's primary-attempt raw
    // pointers before calling setClosed() — the post-close error
    // retrieval path (getReferencedErrorInfo → poll_op_priv->getErrorInfo)
    // still has last_err_/last_desc_ via the base-class error surface,
    // so callers observing the CLOSED transition see a meaningful
    // diagnostic.
    std::vector<std::unique_ptr<Attempt>> losers = std::move(attempts_);
    attempts_.clear();
    // Null the primary-attempt pointers on the connection — the primary
    // is in losers now and about to be torn down, so the raw members
    // would otherwise dangle.
    sock_priv = nullptr;
    sock_obj = nullptr;
    poll_op_priv = nullptr;
    poll_op_obj = nullptr;
    submitted_to_controller = false;
    lk.unlock();

    ExceptionSink xsink;
    for (auto& a : losers) {
        if (a) {
            clearAttempt(*a, &xsink);
        }
    }
    xsink.clear();

    setClosed();
}

int Http3ClientConnection::getActiveStreamCount() const {
    // MethodGuard: block concurrent closeConnection from tearing down
    // poll_op_priv while we read it.  If the connection has already
    // been invalidated, report 0 (same as the existing !poll_op_priv
    // branch — this is a count accessor, not a state transition).
    MethodGuard g(const_cast<Http3ClientConnection*>(this));
    if (!g.acquired() || !poll_op_priv) {
        return 0;
    }
    return poll_op_priv->getActiveStreamCount();
}

QoreHashNode* Http3ClientConnection::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return nullptr;
    }
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is closed");
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready");
        return nullptr;
    }

    // Return {future, stream_id} shape to match H1/H2 so
    // HttpClientConnectionManagerBase::request can await the response
    // uniformly via q_future_get_blocking.  The earlier variant blocked
    // on the Future internally and returned the response hash directly,
    // which mgr.request rejected with "submitRequest result missing
    // 'future' key".
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    ReferenceHolder<QoreObject> future_obj(
        new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release()), xsink);

    PromiseAction* action = new PromiseAction(promise_raw, /* promise_obj */ nullptr);

    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return nullptr;
    }

    releaseStreamReservation();

    // Wake the I/O controller
    if (sock_obj) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }

    // Flush pending QUIC writes so request frames are on the wire
    poll_op_priv->flushPendingWrites(xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    result->setKeyValue("future", future_obj.release(), xsink);
    promise_holder.release()->deref(xsink);
    return result.release();
}

int64_t Http3ClientConnection::submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return -1;
    }
    if (!poll_op_priv || isClosed()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is closed");
        return -1;
    }
    if (!isReady()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready");
        return -1;
    }

    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller
    if (sock_obj) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }

    // Flush pending QUIC writes
    poll_op_priv->flushPendingWrites(xsink);

    return stream_id;
}

int64_t Http3ClientConnection::submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection has been closed");
        return -1;
    }
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is closed");
        return -1;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is not ready");
        return -1;
    }

    // Create Channel for incremental response delivery
    ReferenceHolder<QoreChannel> ch_holder(new QoreChannel(-1), xsink);
    QoreChannel* ch = *ch_holder;

    ChannelAction* action = new ChannelAction(ch);

    // streaming=false: `streaming` at this layer means request-body streaming
    // (caller pushes body via sendStreamData).  submitRequestStreaming uses a
    // one-shot body and only streams the response, so HEADERS must carry
    // END_STREAM (via nghttp3 with drp=nullptr) when the body is empty —
    // otherwise the server waits for body data that never arrives.
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller
    if (sock_obj) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }

    // Flush pending QUIC writes
    poll_op_priv->flushPendingWrites(xsink);

    ch->ref();
    channel_out = ch;
    return stream_id;
}

QoreHashNode* Http3ClientConnection::submitRequestStreamingSend(const char* method,
        const char* path, const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection has been closed");
        return nullptr;
    }
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection is closed");
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection is not ready");
        return nullptr;
    }

    AbstractAsyncAction* action = nullptr;
    ReferenceHolder<QoreObject> future_obj(xsink);
    ReferenceHolder<QoreChannel> ch_holder(xsink);

    if (streaming_recv) {
        ch_holder = new QoreChannel(-1);
        action = new ChannelAction(*ch_holder);
    } else {
        ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
        QorePromise* promise_raw = *promise_holder;
        ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        QoreProgram* pgm = getProgram();
        future_obj = new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release());
        action = new PromiseAction(promise_raw, nullptr);
        promise_holder.release()->deref(xsink);
    }

    // Submit with streaming=true (no END_STREAM — bidirectional streaming)
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        nullptr, 0, /* streaming */ true, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return nullptr;
    }

    // Store stream_id for subsequent pushSendData/setTrailers calls
    streaming_send_stream_id = stream_id;

    releaseStreamReservation();

    // Wake the I/O controller
    if (sock_obj) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }

    // Flush pending QUIC writes
    poll_op_priv->flushPendingWrites(xsink);

    // Build result
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    if (streaming_recv) {
        QoreChannel* ch = *ch_holder;
        ch->ref();
        channel_out = ch;
    } else {
        result->setKeyValue("future", future_obj.release(), xsink);
    }
    return result.release();
}

void Http3ClientConnection::pushSendData(const void* data, size_t len, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has been closed");
        return;
    }
    if (!poll_op_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: no streaming send in progress");
        return;
    }
    if (!poll_op_priv->isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection is not ready");
        return;
    }

    std::shared_ptr<QuicSession> session = poll_op_priv->getInnerOp()->getSession();
    if (!session) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: QUIC session not available");
        return;
    }

    bool end_stream = (!data || !len);
    if (end_stream) {
        // End-of-body: send empty DATA with end_stream flag
        session->sendStreamData(streaming_send_stream_id, nullptr, 0, true, xsink);
        streaming_send_stream_id = -1;
    } else {
        session->sendStreamData(streaming_send_stream_id, data, len, false, xsink);
    }

    // Flush pending QUIC writes
    if (!*xsink) {
        poll_op_priv->flushPendingWrites(xsink);
    }

    // Wake the I/O controller
    if (sock_obj && !*xsink) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }
}

void Http3ClientConnection::setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has been closed");
        return;
    }
    if (!poll_op_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: no streaming send in progress");
        return;
    }

    std::shared_ptr<QuicSession> session = poll_op_priv->getInnerOp()->getSession();
    if (!session) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: QUIC session not available");
        return;
    }

    // Convert QoreHashNode trailers to strcase_str_map_t
    strcase_str_map_t trailer_map;
    if (trailers) {
        ConstHashIterator hi(trailers);
        while (hi.next()) {
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                trailer_map[hi.getKey()] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    session->submitTrailers(streaming_send_stream_id, trailer_map, xsink);

    // Flush pending QUIC writes
    if (!*xsink) {
        poll_op_priv->flushPendingWrites(xsink);
    }
}

void Http3ClientConnection::closeConnection(ExceptionSink* xsink) {
    // Lifetime barrier: atomically mark the connection invalidated (so
    // any NEW method calls arriving after this point are refused via
    // MethodGuard.acquired()==false and return a clean Qore exception)
    // and wait for any CURRENTLY in-flight method calls to finish.  See
    // HttpClientConnection.h for the full design rationale.
    //
    // After drainInFlight() returns, no call is dereferencing the
    // protocol-specific members below and none can start, so the
    // teardown that follows is race-free with Qore-visible API callers.
    markInvalidated();
    drainInFlight();

    // Happy-eyeballs: if we are still racing (no winner chosen yet),
    // tear down every in-flight attempt.  Move them out of attempts_
    // under the lock and clear outside it.
    //
    // IMPORTANT — dangling-pointer guard: while racing, the connection's
    // @c poll_op_priv / @c sock_priv / @c poll_op_obj / @c sock_obj
    // members point into the PRIMARY attempt's tuple (published during
    // buildAndSubmit so pre-winner closeConnection has something to
    // cancel).  clearAttempt derefs the loser's QoreObjects, which may
    // drop the LAST ref and destroy the underlying priv — including its
    // @c stream_lock mutex.  If the connection's member pointers still
    // alias that destroyed object when the post-loop code below calls
    // @c poll_op_priv->disarmConnectionPriv(), the mutex lock asserts
    // under debug / segfaults under release.  Null the members BEFORE
    // dropping into the clear loop so the post-loop path skips the
    // dangling-pointer branch.
    bool was_racing;
    std::vector<std::unique_ptr<Attempt>> to_clear;
    {
        std::lock_guard<std::mutex> lk(attempts_mu_);
        was_racing = !attempts_.empty();
        to_clear = std::move(attempts_);
        attempts_.clear();
    }
    if (was_racing) {
        poll_op_priv = nullptr;
        poll_op_obj = nullptr;
        sock_priv = nullptr;
        sock_obj = nullptr;
        submitted_to_controller = false;
    }
    for (auto& a : to_clear) {
        if (a) {
            clearAttempt(*a, xsink);
        }
    }

    if (!poll_op_priv) {
        // Either we never committed a winner (all attempts already torn
        // down above) or closeConnection was called twice — transition
        // the base-class state to CLOSED so waiters see the error.
        setClosed();
        return;
    }

    // Disarm the raw connection_priv back-pointer BEFORE cancel — same
    // pattern as Http1/Http2ClientConnection::closeConnection.
    poll_op_priv->disarmConnectionPriv();

    // Cancel the op in the global AsyncIoController — this synchronously
    // waits until the I/O thread stops processing the operation.  The I/O
    // thread's cancel processing calls abort() on the poll op via
    // doCancelIntern → callAbort, so we must NOT call abort() again here.
    //
    // NOTE: we must always cancel even if isClosed() is true.  The I/O
    // thread's setError() calls connection_priv->setClosed() (which sets
    // the state to CLOSED) before nulling connection_priv.  If we skip
    // the cancel based on isClosed(), the poll op remains in the I/O
    // controller's cache with a stale connection_priv pointer — the I/O
    // thread would access freed memory when it later processes the entry.
    if (submitted_to_controller && sock_priv) {
        ExceptionSink cancel_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&cancel_xsink), &cancel_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &cancel_xsink)),
                &cancel_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->cancel(sock_priv, &cancel_xsink);
            }
        }
        cancel_xsink.clear();
        submitted_to_controller = false;
    } else if (!isClosed()) {
        // Not submitted to the I/O controller — abort directly.
        poll_op_priv->abort(xsink);
    }

    setClosed();
}

QoreHashNode* Http3ClientConnection::getReferencedErrorInfo() {
    if (poll_op_priv) {
        return poll_op_priv->getErrorInfo();
    }
    // Happy-eyeballs all-fail path: attempts_ has been torn down and
    // poll_op_priv nulled.  Surface the cached error from the most
    // recent handshake failure.  Use ReferenceHolder so a bad_alloc
    // thrown by the intermediate QoreStringNode allocations does not
    // leak the hash.
    std::lock_guard<std::mutex> lk(attempts_mu_);
    if (last_err_.empty() && last_desc_.empty()) {
        return nullptr;
    }
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xsink);
    h->setKeyValue("err",
        new QoreStringNode(last_err_.empty()
            ? "HTTP3-CONNECT-ERROR" : last_err_.c_str()),
        &xsink);
    h->setKeyValue("desc",
        new QoreStringNode(last_desc_.empty()
            ? "QUIC handshake failed" : last_desc_.c_str()),
        &xsink);
    xsink.clear();
    return h.release();
}
