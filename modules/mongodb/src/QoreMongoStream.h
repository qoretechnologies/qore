/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMongoStream.h

    Qore mongodb module - Interruptible stream wrapper

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
*/

#ifndef _QORE_MODULE_MONGODB_QOREMONGOSTREAM_H
#define _QORE_MODULE_MONGODB_QOREMONGOSTREAM_H

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>

#include <mongoc/mongoc.h>

//! Stream initiator function for interruptible MongoDB streams
/** Connects a socket of the module's own and returns a stream on it (with libmongoc's TLS stream stacked on it for
    TLS connections) whose waits use qore_cancellable_poll(), so every blocking wait of the connection - connect,
    TLS handshake, reads, writes and libmongoc's stream polls - ends as soon as the thread is cancelled or its
    Program is interrupted, without a periodic timeout.  It should be set on the MongoDB client using
    mongoc_client_set_stream_initiator().

    @param uri The MongoDB URI
    @param host The host to connect to
    @param user_data User data (unused)
    @param error Error structure for reporting errors

    @return A new stream or nullptr on error
*/
DLLLOCAL mongoc_stream_t* qore_mongo_stream_initiator(
    const mongoc_uri_t* uri,
    const mongoc_host_list_t* host,
    void* user_data,
    bson_error_t* error);

//! Configure libmongoc logging for the Qore MongoDB module
DLLLOCAL void qore_mongo_set_log_handler();

//! Set up interruptible streams for a MongoDB client
/** This function configures the given MongoDB client to use interruptible
    streams for all I/O operations.

    @param client The MongoDB client to configure
*/
DLLLOCAL void qore_mongo_setup_interruptible_streams(mongoc_client_t* client);

#endif // _QORE_MODULE_MONGODB_QOREMONGOSTREAM_H
