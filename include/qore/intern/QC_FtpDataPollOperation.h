/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_FtpDataPollOperation.h

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

#ifndef _QORE_CLASS_FTPDATAPOLLOPERATION_H

#define _QORE_CLASS_FTPDATAPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/QoreSocketObject.h"
#include "qore/InputStream.h"
#include "qore/OutputStream.h"

#include <string>

//! FTP data channel transfer direction
enum class FtpDataDirection : int {
    RECEIVE = 0,        //!< RETR or LIST — receive data until EOF
    SEND,               //!< STOR — send data then close
};

//! FTP data channel I/O states
enum class FtpDataState : int {
    CONNECTING = 0,     //!< TCP connect to PASV/EPSV port
    SSL_UPGRADE,        //!< TLS handshake on data socket
    RECEIVING,          //!< Reading data until server closes connection (RETR/LIST)
    SENDING,            //!< Sending data (STOR)
    DONE,               //!< Transfer complete
    CLOSED              //!< Aborted / error
};

//! C++ poll operation for FTP data channel: connect + optional TLS + transfer
/** Handles a single FTP data transfer on a dedicated data socket:
    - TCP connect to the PASV/EPSV port
    - Optional TLS upgrade (if secure_data)
    - Data transfer: receive until EOF (RETR/LIST) or send all data (STOR)

    The operation creates its own QoreSocketObject for the data channel.
    goalReached() returns true when the transfer is complete.
    getOutput() returns the received data (for RETR/LIST) as binary.

    @since %Qore 2.3
*/
class FtpDataPollOperationPriv : public SocketPollOperationBase {
public:
    //! Creates a receive (RETR/LIST) data poll operation
    /** @param self the QoreObject wrapping this private data
        @param data_sock the data socket (ref'd by caller, ownership transferred)
        @param connect_op the in-progress connect operation (ref'd by caller, ownership transferred)
        @param secure_data true to upgrade data channel to TLS after connect
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        SocketPollOperationBase* connect_op, bool secure_data);

    //! Creates a send (STOR) data poll operation
    /** @param self the QoreObject wrapping this private data
        @param data_sock the data socket (ref'd by caller, ownership transferred)
        @param connect_op the in-progress connect operation (ref'd by caller, ownership transferred)
        @param secure_data true to upgrade data channel to TLS after connect
        @param send_data the data to send (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        SocketPollOperationBase* connect_op, bool secure_data,
        BinaryNode* send_data);

    //! Creates a receive-to-stream data poll operation
    /** @param self the QoreObject wrapping this private data
        @param data_sock the data socket (ref'd by caller, ownership transferred)
        @param connect_op the in-progress connect operation (ref'd by caller, ownership transferred)
        @param secure_data true to upgrade data channel to TLS after connect
        @param recv_output_stream the output stream to receive into (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        SocketPollOperationBase* connect_op, bool secure_data,
        OutputStream* recv_output_stream);

    //! Creates a send-from-stream data poll operation
    /** @param self the QoreObject wrapping this private data
        @param data_sock the data socket (ref'd by caller, ownership transferred)
        @param connect_op the in-progress connect operation (ref'd by caller, ownership transferred)
        @param secure_data true to upgrade data channel to TLS after connect
        @param send_input_stream the input stream to send (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        SocketPollOperationBase* connect_op, bool secure_data,
        InputStream* send_input_stream);

    //! Creates a receive data poll operation on an already-connected socket (adopt-socket)
    /** Used for PORT mode where the socket comes from SocketAcceptPollOperation.
        Starts directly in RECEIVING state (no CONNECTING phase).
        @param self the QoreObject wrapping this private data
        @param data_sock the already-connected data socket (not ref'd — raw pointer)
        @param secure_data true to upgrade to TLS before transfer
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        bool secure_data, bool recv_mode);

    //! Creates a send data poll operation on an already-connected socket (adopt-socket)
    /** @param self the QoreObject wrapping this private data
        @param data_sock the already-connected data socket (not ref'd — raw pointer)
        @param secure_data true to upgrade to TLS before transfer
        @param send_data the data to send (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        bool secure_data, BinaryNode* send_data);

    //! Creates a receive-to-stream data poll operation on an already-connected socket (adopt-socket)
    /** @param self the QoreObject wrapping this private data
        @param data_sock the already-connected data socket (not ref'd — raw pointer)
        @param secure_data true to upgrade to TLS before transfer
        @param recv_output_stream the output stream to receive into (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        bool secure_data, OutputStream* recv_output_stream);

    //! Creates a send-from-stream data poll operation on an already-connected socket (adopt-socket)
    /** @param self the QoreObject wrapping this private data
        @param data_sock the already-connected data socket (not ref'd — raw pointer)
        @param secure_data true to upgrade to TLS before transfer
        @param send_input_stream the input stream to send (ref'd by caller, ownership transferred)
    */
    DLLLOCAL FtpDataPollOperationPriv(QoreObject* self, QoreSocketObject* data_sock,
        bool secure_data, InputStream* send_input_stream);

    DLLLOCAL virtual ~FtpDataPollOperationPriv();

    //! Releases inner op, data socket, and any accumulated data on last ref
    DLLLOCAL void deref(ExceptionSink* xsink);

    // --- SocketPollOperationBase interface ---
    DLLLOCAL bool goalReached() const override;
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    //! Returns true when the transfer is complete
    DLLLOCAL bool isDone() const { return data_state == FtpDataState::DONE; }

    //! Returns the data socket (not ref'd; caller must not deref)
    DLLLOCAL QoreSocketObject* getDataSocket() const { return data_sock; }

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* data_sock;                    //!< data socket (ref'd)
    SocketPollOperationBase* current_op = nullptr;  //!< current inner operation (ref'd)

    FtpDataState data_state = FtpDataState::CONNECTING;
    FtpDataDirection direction;
    bool secure_data;

    // Receive buffer (RETR/LIST)
    SimpleRefHolder<BinaryNode> recv_data;
    SimpleRefHolder<OutputStream> recv_output_stream;

    // Send buffer (STOR)
    SimpleRefHolder<BinaryNode> send_data;
    SimpleRefHolder<InputStream> send_input_stream;
    size_t send_offset = 0;

public:
    //! Submit this operation to the global async I/O controller
    DLLLOCAL QoreObject* submitToController(ExceptionSink* xsink, const char* owner = "ftp-data",
        int timeout_ms = -1, bool replace = false);

private:
    // --- Helper methods ---
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
    DLLLOCAL void setClosed();

    // --- State handlers ---
    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleReceiving(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSending(ExceptionSink* xsink);

    //! Start receiving data (creates a SocketRecvDataPollOperation)
    DLLLOCAL int startRecvData(ExceptionSink* xsink);
    //! Start sending data (creates a SocketSendPollOperation)
    DLLLOCAL int startSendData(ExceptionSink* xsink);
};

DLLLOCAL extern qore_classid_t CID_FTPDATAPOLLOPERATION;
DLLLOCAL extern QoreClass* QC_FTPDATAPOLLOPERATION;
DLLLOCAL QoreClass* initFtpDataPollOperationClass(QoreNamespace& ns);

#endif // _QORE_CLASS_FTPDATAPOLLOPERATION_H
