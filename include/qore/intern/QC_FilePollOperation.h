/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_FilePollOperation.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_FILEPOLLOPERATION_H

#define _QORE_CLASS_FILEPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/qore_qf_private.h"
#include "qore/QoreFile.h"

#include <memory>

// states: none -> opened -> reading -> read-done
constexpr int FPS_NONE = 0;
constexpr int FPS_READING = 1;
constexpr int FPS_READ_DONE = 2;

class FileReadPollOperationBase : public SocketPollOperationBase {
public:
    DLLLOCAL FileReadPollOperationBase(File* file, bool to_string) : file(file), to_string(to_string) {
    }

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                AutoLocker al(file->priv->m);
                file->priv->clearNonBlock(xsink);
            }
            file->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == FPS_READ_DONE;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case FPS_NONE: return "none";
            case FPS_READING: return "reading";
            case FPS_READ_DONE: return "read-done";
        }
        assert(false);
        return "error";
    }

    DLLLOCAL virtual QoreValue getOutput() const {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) {
        if (set_non_block) {
            set_non_block = false;
            AutoLocker al(file->priv->m);
            file->priv->clearNonBlock(xsink);
            file->priv->close_intern();
            state = FPS_NONE;
        }
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

protected:
    std::unique_ptr<AbstractPollState> poll_state;
    SimpleRefHolder<SimpleValueQoreNode> data;
    File* file;
    bool to_string;
    bool set_non_block = false;
    int state = FPS_NONE;

    DLLLOCAL int initIntern(ExceptionSink* xsink);
};

class FileReadPollOperation : public FileReadPollOperationBase {
public:
    DLLLOCAL FileReadPollOperation(ExceptionSink* xsink, File* file, const char* path, ssize_t size,
            bool to_string);

private:
    std::string path;
    ssize_t size;
};

DLLLOCAL QoreClass* initFilePollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_FILEPOLLOPERATION_H
