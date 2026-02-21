/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AsyncIoControllerPriv.h

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

#ifndef _QORE_ASYNCIOCONTROLLERPRIV_H

#define _QORE_ASYNCIOCONTROLLERPRIV_H

#include <qore/Qore.h>
#include <qore/QoreQueue.h>
#include <qore/QoreAbstractLoggerInterface.h>
#include "qore/intern/QoreEventLoop.h"
#include "qore/intern/QoreEventNotifier.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class QoreObject;
class QoreSocketObject;
class SocketPollOperationBase;

// hashdecl pointers
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollOperationInfo;
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollResultInfo;

//! C++ async I/O controller - direct translation of AsyncSocketIoController.qc
/** This class implements the core async I/O event loop used by HTTP/2 server,
    HTTP/2 client multiplexing, WebSocket, and SSE.

    Thread safety: all public methods are thread-safe. Internal state is protected
    by a single mutex with careful lock ordering to avoid deadlocks.

    @since %Qore 2.3
*/
class AsyncIoControllerPriv : public AbstractPrivateData {
public:
    //! Default I/O operation timeout (30 seconds)
    static constexpr int64 DEFAULT_IO_TIMEOUT_US = 30000000LL;

    //! Creates the controller
    /** @param autostop if true, the I/O thread will stop when all operations complete
    */
    DLLLOCAL AsyncIoControllerPriv(bool autostop = true);

    //! Destructor
    DLLLOCAL virtual ~AsyncIoControllerPriv();

    //! Submit an async operation
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
        @param xsink for exception handling
        @return a Queue object for receiving the result (or nullptr if callback was provided)
    */
    DLLLOCAL QoreObject* submit(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink);

    //! Execute an async operation synchronously
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
        @param xsink for exception handling
        @return the result hash
    */
    DLLLOCAL QoreHashNode* exec(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink);

    //! Cancel an operation by socket
    /** @param sock the socket to cancel
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancel(QoreSocketObject* sock, ExceptionSink* xsink);

    //! Cancel an operation by key
    /** @param key the operation key
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancelByKey(const QoreStringNode* key, ExceptionSink* xsink);

    //! Cancel all operations for an owner
    /** @param owner the owner identifier
        @param xsink for exception handling
        @return the number of operations canceled
    */
    DLLLOCAL int cancelByOwner(const QoreStringNode* owner, ExceptionSink* xsink);

    //! Wake the I/O thread to force re-poll
    DLLLOCAL void wake();

    //! Start the I/O thread
    /** @param xsink for exception handling
    */
    DLLLOCAL void start(ExceptionSink* xsink);

    //! Stop the I/O thread gracefully
    /** @param xsink for exception handling
    */
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Stop and clear all operations
    /** @param xsink for exception handling
    */
    DLLLOCAL void stopClear(ExceptionSink* xsink);

    //! Wait for the I/O thread to stop
    DLLLOCAL bool waitStop(ExceptionSink* xsink);

    //! Wait for the I/O thread to be ready
    /** @param timeout_ms timeout in ms; 0 = wait forever
        @param xsink for exception handling
        @return true if the I/O thread is ready
    */
    DLLLOCAL bool waitReady(int timeout_ms, ExceptionSink* xsink);

    //! Returns true if the I/O thread is running
    DLLLOCAL bool running() const;

    //! Returns the number of cached operations
    DLLLOCAL int getCacheSize() const;

    //! Sets the autostop flag
    DLLLOCAL void setAutostop(bool autostop);

    //! Returns the autostop flag
    DLLLOCAL bool getAutostop() const;

    //! Returns info about the controller state
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

    //! Sets the logger
    /** @param logger_obj the LoggerInterface Qore object (or nullptr to clear)
        @param xsink for exception handling
    */
    DLLLOCAL void setLogger(QoreObject* logger_obj, ExceptionSink* xsink);

    //! Custom deref with ExceptionSink
    DLLLOCAL virtual void deref(ExceptionSink* xsink);

private:
    //! I/O thread commands
    enum class IoCommand {
        Add,
        Cancel,
        CancelOwner,
        Quit,
        Wake,
    };

    //! Command queue entry
    struct Command {
        IoCommand cmd;
        std::string key;                //!< For Cancel: the cache key
        std::string owner;              //!< For CancelOwner: the owner string
        QoreCondition* done_cond;       //!< For CancelOwner: signaled when all canceled
    };

    //! Internal poll info (mirrors Qore Priv::PollInfo)
    struct PollInfo {
        int64 timeout_date_us;          //!< Absolute timeout (microseconds since epoch), 0 = not set
        QoreObject* sock_obj;           //!< Socket QoreObject (referenced)
        QoreSocketObject* sock;         //!< Socket private data
        QoreObject* spop_obj;           //!< AbstractPollOperation QoreObject (referenced)
        QoreHashNode* poll_info;        //!< Last SocketPollInfo hash (referenced, or nullptr)
        int64 timeout_us;               //!< Timeout in microseconds (negative = no timeout)
        std::string owner;              //!< Owner identifier
        QoreHashNode* other;            //!< Free-form data (referenced, or nullptr)
        Queue* queue;                   //!< Result queue (referenced, or nullptr)
        ResolvedCallReferenceNode* callback; //!< Completion callback (referenced, or nullptr)

        DLLLOCAL PollInfo() : timeout_date_us(0), sock_obj(nullptr), sock(nullptr),
            spop_obj(nullptr), poll_info(nullptr), timeout_us(DEFAULT_IO_TIMEOUT_US),
            other(nullptr), queue(nullptr), callback(nullptr) {
        }

        DLLLOCAL ~PollInfo() {
            // NOTE: caller must have already dereferenced all objects via cleanup()
        }

        //! Clean up all references
        DLLLOCAL void cleanup(ExceptionSink* xsink);
    };

    //! Deferred delivery info
    struct DeferredDelivery {
        std::string key;
        Queue* queue;                      //!< Referenced or nullptr
        ResolvedCallReferenceNode* callback; //!< Referenced or nullptr
        QoreHashNode* result;              //!< Referenced
    };

    //! Logger bridge that wraps a QoreObject implementing LoggerInterface
    class LoggerBridge : public QoreAbstractLoggerInterface {
    public:
        DLLLOCAL LoggerBridge(QoreObject* logger_obj);
        DLLLOCAL virtual ~LoggerBridge();

        DLLLOCAL virtual void logArgs(int level, const QoreStringNode* msg,
            const QoreListNode* args, ExceptionSink* xsink) override;
        DLLLOCAL virtual bool isEnabledFor(int level) const override;

        DLLLOCAL virtual void deref(ExceptionSink* xsink);

    private:
        QoreObject* logger_obj;          //!< Referenced
        const QoreMethod* logArgsMethod; //!< logArgs method pointer
        const QoreMethod* isEnabledForMethod; //!< isEnabledFor(int) method pointer
    };

    // --- Mutex-protected state ---
    mutable QoreThreadLock m;
    std::unordered_map<std::string, PollInfo> cache;
    int tid;                              //!< I/O thread ID (0 if not running)
    bool autostop_flag;
    bool shutting_down;
    bool force_poll;
    std::unordered_map<std::string, int> socket_refcounts;
    std::deque<Command> cmdq;
    std::unordered_map<std::string, QoreCondition*> cancel_cond_map;
    QoreCondition io_cond;
    bool io_waiting;
    bool io_exiting;
    bool ready_flag;
    LoggerBridge* logger;                 //!< Referenced or nullptr

    // --- I/O thread only state ---
    QoreEventLoop* loop;
    QoreEventNotifier* notifier;

    // Registered socket tracking (I/O thread only, but updated under lock in phase 3)
    std::unordered_map<std::string, QoreObject*> registered_sockets; //!< key -> Socket obj
    std::unordered_map<std::string, int> registered_events;          //!< sock hash -> current events
    std::unordered_map<std::string, int> key_events;                 //!< key -> events for this key
    std::unordered_map<std::string, std::unordered_set<std::string>> sock_hash_to_keys; //!< reverse index

    // --- Internal methods ---

    //! Start the I/O thread (caller must hold lock)
    DLLLOCAL void startIntern(ExceptionSink* xsink);

    //! The I/O thread main function
    DLLLOCAL static void ioThreadEntry(ExceptionSink* xsink, void* arg);

    //! The I/O thread main loop
    DLLLOCAL void ioThread(ExceptionSink* xsink);

    //! Process commands from the command queue
    /** @return true if the I/O thread should exit
    */
    DLLLOCAL bool processCommands(ExceptionSink* xsink);

    //! Cancel an operation internally (delivers result, called from I/O thread)
    DLLLOCAL void doCancelIntern(PollInfo& pinfo, ExceptionSink* xsink);

    //! Update EventLoop registration for an operation
    /** @param key the operation key
        @param socket the new socket to register (nullptr to unregister)
        @param sock_hash the socket's unique hash
        @param events the events to monitor
        @param xsink for exception handling
    */
    DLLLOCAL void updateEventLoopRegistration(const std::string& key, QoreObject* socket,
        const std::string& sock_hash, int events, ExceptionSink* xsink);

    //! Unregister an operation from the EventLoop
    DLLLOCAL void unregisterFromEventLoop(const std::string& key, ExceptionSink* xsink);

    //! Compute the event union for a socket
    DLLLOCAL int computeEventUnion(const std::string& sock_hash) const;

    //! Apply the event union for a socket
    DLLLOCAL void applyEventUnion(QoreObject* socket, const std::string& sock_hash, ExceptionSink* xsink);

    //! Enqueue a command (caller must hold lock)
    /** @return true if the notifier should be signaled
    */
    DLLLOCAL bool enqueueCmdLocked(IoCommand cmd, const std::string& key = std::string(),
        const std::string& owner = std::string(), QoreCondition* done_cond = nullptr);

    //! Wait for a cancel to complete
    DLLLOCAL void waitCancel(const std::string& key);

    //! Log a message
    DLLLOCAL void log(int level, const char* fmt, ...) const;

    //! Get the socket unique hash from a QoreSocketObject
    DLLLOCAL static std::string getSocketHash(QoreSocketObject* sock);

    //! Get socket and poll info from a SocketPollInfo hash
    DLLLOCAL static QoreObject* getSocketFromPollInfo(QoreHashNode* poll_info, std::string& sock_hash,
        int& events, ExceptionSink* xsink);

    //! Call continuePoll on an AbstractPollOperation object
    DLLLOCAL static QoreHashNode* callContinuePoll(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Call abort on an AbstractPollOperation object
    DLLLOCAL static void callAbort(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Build a result hash
    DLLLOCAL static QoreHashNode* buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink);
};

#endif // _QORE_ASYNCIOCONTROLLERPRIV_H
