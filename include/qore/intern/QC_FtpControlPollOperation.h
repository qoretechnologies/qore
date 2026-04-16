/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_FtpControlPollOperation.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_FTPCONTROLPOLLOPERATION_H

#define _QORE_CLASS_FTPCONTROLPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/QoreSocketObject.h"

#include <string>

//! FTP control channel I/O states
enum class FtpCtrlState : int {
    CONNECTING = 0,     //!< TCP connect in progress
    RECV_LINE,          //!< Reading CRLF-terminated response line(s)
    SENDING,            //!< Sending command data
    SSL_UPGRADE,        //!< TLS handshake on control socket
    READY,              //!< Logged in, available for commands
    CLOSED              //!< Terminal state
};

//! FTP protocol phase — tracks position in the login/command sequence
enum class FtpCtrlPhase : int {
    GREETING = 0,       //!< Waiting for server greeting (220)
    AUTH_TLS,           //!< Sent AUTH TLS, waiting for 234
    PBSZ,              //!< Sent PBSZ 0, waiting for 200
    PROT,              //!< Sent PROT P, waiting for 200
    USER,              //!< Sent USER, waiting for 230/331
    PASS,              //!< Sent PASS, waiting for 230
    COMMAND,           //!< User-submitted command awaiting response
};

//! C++ poll operation for FTP control channel: connect + login + command execution
/** State machine: CONNECTING → RECV_LINE (greeting) → [AUTH TLS → SSL_UPGRADE → PBSZ → PROT →]
    USER → [PASS →] READY → [SENDING → RECV_LINE →]* READY → CLOSED

    Handles the entire FTP control channel lifecycle:
    1. TCP connect to the FTP server
    2. Receive and validate the 220 greeting
    3. (FTPS) AUTH TLS, SSL upgrade, PBSZ 0, PROT P
    4. USER/PASS login
    5. Arbitrary command execution via submitCommand()

    goalReached() returns true when:
    - Login completes (READY state with login_complete)
    - A submitted command receives its response (command_complete)
    - Connection is closed

    Thread safety: continuePoll() runs on the I/O thread. submitCommand() must
    be called only when the operation is NOT in the async controller (between
    goalReached() and re-submission).

    @since %Qore 2.3
*/
class FtpControlPollOperationPriv : public SocketPollOperationBase {
public:
    //! Creates the operation with a TCP connect already in progress
    /** @param self the QoreObject wrapping this private data
        @param ctrl_sock the control socket (ref'd by caller, ownership transferred)
        @param connect_op the in-progress connect operation (ref'd by caller, ownership transferred)
        @param secure true for FTPS (AUTH TLS on control)
        @param secure_data true for encrypted data channel (PBSZ + PROT)
        @param user FTP username (nullptr for "anonymous")
        @param pass FTP password (nullptr for "qore@nohost.com")
    */
    DLLLOCAL FtpControlPollOperationPriv(QoreObject* self, QoreSocketObject* ctrl_sock,
        SocketPollOperationBase* connect_op,
        bool secure, bool secure_data,
        const char* user, const char* pass);

    DLLLOCAL virtual ~FtpControlPollOperationPriv();

    //! Releases inner op and control socket on last ref
    DLLLOCAL void deref(ExceptionSink* xsink);

    // --- SocketPollOperationBase interface ---
    DLLLOCAL bool goalReached() const override;
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    //! Submit a command for execution on the control channel
    /** Must be called when the operation is in READY state (login_complete == true).
        After calling this, re-submit the operation to the async controller; the
        next continuePoll() will send the command and read the response.

        @param cmd FTP command (e.g. "TYPE", "EPSV", "RETR")
        @param arg command argument (may be nullptr)
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitCommand(const char* cmd, const char* arg, ExceptionSink* xsink);

    //! Submit a receive-only operation (no command sent)
    /** Used to read an unsolicited server response, e.g. the 226 completion
        response after a data transfer.
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitRecvResponse(ExceptionSink* xsink);

    //! Returns true if login is complete and operation is in READY state
    DLLLOCAL bool isReady() const { return login_complete && ctrl_state == FtpCtrlState::READY; }

    //! Returns true if the connection is closed
    DLLLOCAL bool isClosed() const { return ctrl_state == FtpCtrlState::CLOSED; }

    //! Returns the last FTP response code
    DLLLOCAL int getLastResponseCode() const { return last_code; }

    //! Returns the control socket (not ref'd — caller must not deref)
    DLLLOCAL QoreSocketObject* getControlSocket() const { return ctrl_sock; }

    //! Returns true if the control channel uses TLS
    DLLLOCAL bool isSecure() const { return secure; }

    //! Returns true if data channel should use TLS
    DLLLOCAL bool isSecureData() const { return secure_data; }

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* ctrl_sock;                    //!< control socket (ref'd)
    SocketPollOperationBase* current_op = nullptr;  //!< current inner operation (ref'd)

    FtpCtrlState ctrl_state = FtpCtrlState::CONNECTING;
    FtpCtrlPhase phase = FtpCtrlPhase::GREETING;

    // Connection parameters
    bool secure;
    bool secure_data;
    std::string ftp_user;
    std::string ftp_pass;

    // Response state
    int last_code = 0;
    QoreString response_accum;                      //!< accumulates multi-line responses
    mutable SimpleRefHolder<QoreStringNode> last_response;

    // Goal tracking
    bool login_complete = false;
    bool command_complete = false;

    // Pending command (set by submitCommand, consumed by continuePoll)
    std::string pending_cmd;

    // Sync-blocking infrastructure (for sync callers using async controller)
    QoreThreadLock sync_lock;
    QoreCondition sync_cond;
    bool sync_done = false;

public:
    //! Signal sync caller that the operation has completed
    /** Called from onComplete() (worker thread) to wake the blocking sync thread.
    */
    DLLLOCAL void signalCompletion() {
        AutoLocker al(sync_lock);
        sync_done = true;
        sync_cond.signal();
    }

    //! Block until the operation signals completion or timeout
    /** @param timeout_ms maximum wait time in milliseconds
        @param xsink exception sink
        @return 0 on success, -1 on timeout or error
    */
    DLLLOCAL int waitForCompletion(int timeout_ms, ExceptionSink* xsink) {
        AutoLocker al(sync_lock);
        while (!sync_done) {
            int rc = sync_cond.wait(sync_lock, timeout_ms);
            if (rc == ETIMEDOUT) {
                xsink->raiseException("SOCKET-TIMEOUT", "FTP operation timed out after %d ms", timeout_ms);
                return -1;
            }
        }
        sync_done = false;  // reset for next use
        return 0;
    }

    //! Submit this operation to the global async I/O controller
    /** @param xsink exception sink
        @param owner owner string for controller cache (default: "ftp-ctrl")
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitToController(ExceptionSink* xsink, const char* owner = "ftp-ctrl");

private:
    // --- Helper methods ---
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
    DLLLOCAL int startSendData(const char* data, size_t len, ExceptionSink* xsink);
    DLLLOCAL int startSendCommand(const char* cmd, const char* arg, ExceptionSink* xsink);
    DLLLOCAL int startRecvLine(ExceptionSink* xsink);
    DLLLOCAL int startSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL void setClosed();

    // --- State handlers (called from continuePoll) ---
    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSending(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvLine(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslUpgrade(ExceptionSink* xsink);

    // --- Phase dispatch (called when a complete FTP response is received) ---
    DLLLOCAL QoreHashNode* dispatchResponse(ExceptionSink* xsink);

    // --- Login-phase transition helpers ---
    DLLLOCAL QoreHashNode* startAuthTls(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* startUserLogin(ExceptionSink* xsink);
};

DLLLOCAL extern qore_classid_t CID_FTPCONTROLPOLLOPERATION;
DLLLOCAL extern QoreClass* QC_FTPCONTROLPOLLOPERATION;
DLLLOCAL QoreClass* initFtpControlPollOperationClass(QoreNamespace& ns);

//! Composite poll operation for FTP PORT mode: accept + optional SSL upgrade
/** State machine: ACCEPTING → [SSL_UPGRADE →] COMPLETE
    Follows the same pattern as HttpAcceptPollOperationPriv.
    @since %Qore 2.3
*/
class FtpPortAcceptPollOperationPriv : public SocketPollOperationBase {
public:
    enum class AcceptState {
        ACCEPTING,      //!< Waiting for incoming TCP connection on data port
        SSL_UPGRADE,    //!< TLS handshake on accepted socket
        COMPLETE        //!< Accept (+ optional SSL) finished
    };

    //! Creates the operation
    /** @param self QoreObject wrapper
        @param accept_op the SocketAcceptPollOperation (ref'd, ownership transferred)
        @param listener the listener socket (not ref'd — raw pointer, owned by "sock" member)
        @param secure_data true to upgrade accepted socket to SSL
    */
    DLLLOCAL FtpPortAcceptPollOperationPriv(QoreObject* self,
        SocketPollOperationBase* accept_op, QoreSocketObject* listener, bool secure_data);

    DLLLOCAL virtual ~FtpPortAcceptPollOperationPriv();
    DLLLOCAL void deref(ExceptionSink* xsink);

    DLLLOCAL bool goalReached() const override { return state == AcceptState::COMPLETE; }
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    //! Submit this operation to the global async I/O controller
    DLLLOCAL int submitToController(ExceptionSink* xsink, const char* owner = "ftp-port-accept");

    // Sync-blocking infrastructure (same pattern as FtpControlPollOperationPriv)
    QoreThreadLock sync_lock;
    QoreCondition sync_cond;
    bool sync_done = false;

    DLLLOCAL void signalCompletion() {
        AutoLocker al(sync_lock);
        sync_done = true;
        sync_cond.signal();
    }

    DLLLOCAL int waitForCompletion(int timeout_ms, ExceptionSink* xsink) {
        AutoLocker al(sync_lock);
        while (!sync_done) {
            int rc = sync_cond.wait(sync_lock, timeout_ms);
            if (rc == ETIMEDOUT) {
                xsink->raiseException("SOCKET-TIMEOUT",
                    "FTP PORT accept timed out after %d ms", timeout_ms);
                return -1;
            }
        }
        sync_done = false;
        return 0;
    }

    //! Returns the accepted socket (not ref'd — caller must ref if needed)
    DLLLOCAL QoreSocketObject* getAcceptedSocket() const { return client_sock; }

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    SocketPollOperationBase* current_op = nullptr; //!< current inner op (ref'd)
    QoreSocketObject* listener = nullptr;          //!< listener socket (raw, not ref'd)
    QoreSocketObject* client_sock = nullptr;       //!< accepted client socket (ref'd)
    mutable ReferenceHolder<QoreObject> client_sock_obj;  //!< QoreObject wrapper (ref'd)
    AcceptState state = AcceptState::ACCEPTING;
    bool secure_data;

    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleAcceptComplete(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslComplete(ExceptionSink* xsink);
};

DLLLOCAL extern qore_classid_t CID_FTPPORTACCEPTPOLLOPERATION;
DLLLOCAL extern QoreClass* QC_FTPPORTACCEPTPOLLOPERATION;
DLLLOCAL QoreClass* initFtpPortAcceptPollOperationClass(QoreNamespace& ns);

#endif // _QORE_CLASS_FTPCONTROLPOLLOPERATION_H
