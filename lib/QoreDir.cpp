/* -*- indent-tabs-mode: nil -*- */
/*
    QoreDir.cpp

    Directory functions

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

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/QoreDir.h"
#include <sstream>
#include <fstream>

#include "qore/intern/qore_qd_private.h"

const QoreEncoding* QoreDir::getEncoding() const {
    return priv->getEncoding();
}

/* static */
bool QoreDir::file_exists(const QoreString& filePath) {
    struct stat buffer;
    if (::stat(filePath.c_str(), &buffer) == 0) {
        return S_ISREG(buffer.st_mode);
    }
    return false;
}

/* static */
bool QoreDir::folder_exists(const QoreString& folderPath, ExceptionSink& xsink, const QoreEncoding* cs) {
    QoreDir dir(&xsink, cs, folderPath.c_str());
    assert(!xsink);
    return !xsink && !dir.checkPath();
}

/* static */
std::string QoreDir::get_file_content(const QoreString& fullPath) {
    if (!file_exists(fullPath.c_str())) {
      return "";
    }

    std::ifstream file(fullPath.c_str());
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

QoreDir::QoreDir(ExceptionSink* xsink, const QoreEncoding* cs, const char* dir) : priv(new qore_qd_private(xsink, cs, dir)) {}

QoreDir::QoreDir(ExceptionSink* xsink, const QoreDir &old) : priv(new qore_qd_private(xsink, *old.priv)) {
}

QoreDir::~QoreDir() {
    delete priv;
}

// return the actual dirname
QoreStringNode *QoreDir::dirname() const {
    return priv->get_dir_string();
}

// change directory from current location on
// return 0 if directory exists and is openable
int QoreDir::chdir(const char* ndir, ExceptionSink* xsink) {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        // If absolute path, check directly; otherwise, combine with current dir
        std::string path;
        if (q_absolute_path(ndir)) {
            path = ndir;
        } else {
            path = priv->getPath(ndir);
        }
        if (!sm->checkFilesystemAccess(path.c_str(), QSEC_READ, xsink)) {
            return -1;
        }
    }
    return priv->chdir(ndir, xsink);
}

// change directory from current location on
// return 0 if directory exists and is openable
int QoreDir::chdir(const char* ndir) {
    return priv->chdir(ndir);
}

// create the directory with all the parent directories if they do not exist
// return amount of created directories, -1 if error
int QoreDir::create(int mode, ExceptionSink* xsink) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        // Get the directory name
        QoreStringNode* dir = priv->get_dir_string();
        if (dir) {
            if (!sm->checkFilesystemAccess(dir->c_str(), QSEC_CREATE, xsink)) {
                dir->deref();
                return -1;
            }
            dir->deref();
        }
    }
    return priv->create(mode, xsink);
}

// create the directory with all the parent directories if they do not exist
// return amount of created directories, -1 if error
int QoreDir::create(int mode) const {
    return priv->create(mode);
}

// list entries of the directory where d points to
// the filter will be applied to the file's mode for filtering.
// directories '.' and '..' will be skipped
QoreListNode* QoreDir::list(ExceptionSink* xsink, int stat_filter, const QoreString* regex, int regex_options, bool full) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        QoreStringNode* dir = priv->get_dir_string();
        if (dir) {
            if (!sm->checkFilesystemAccess(dir->c_str(), QSEC_READ, xsink)) {
                dir->deref();
                return nullptr;
            }
            dir->deref();
        }
    }
    return priv->list(xsink, stat_filter, regex, regex_options, full);
}

int QoreDir::mkdir(ExceptionSink* xsink, const char* subdir, int mode) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        std::string path;
        if (q_absolute_path(subdir)) {
            path = subdir;
        } else {
            path = priv->getPath(subdir);
        }
        if (!sm->checkFilesystemAccess(path.c_str(), QSEC_CREATE, xsink)) {
            return -1;
        }
    }
    return priv->mkdir(subdir, mode, xsink);
}

int QoreDir::rmdir(const char* subdir, ExceptionSink* xsink) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        std::string path;
        if (q_absolute_path(subdir)) {
            path = subdir;
        } else {
            path = priv->getPath(subdir);
        }
        if (!sm->checkFilesystemAccess(path.c_str(), QSEC_DELETE, xsink)) {
            return -1;
        }
    }
    return priv->rmdir(subdir, xsink);
}

std::string QoreDir::getPath(const char* sub) const {
    return priv->getPath(sub);
}

int QoreDir::checkPath() const {
    return priv->checkPath();
}

int QoreDir::chmod(int mode, ExceptionSink* xsink) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        QoreStringNode* dir = priv->get_dir_string();
        if (dir) {
            if (!sm->checkFilesystemAccess(dir->c_str(), QSEC_WRITE, xsink)) {
                dir->deref();
                return -1;
            }
            dir->deref();
        }
    }
    return priv->chmod(mode, xsink);
}

#ifdef HAVE_PWD_H
int QoreDir::chown(uid_t uid, gid_t gid, ExceptionSink* xsink) const {
    // Check sandbox security restrictions
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        QoreStringNode* dir = priv->get_dir_string();
        if (dir) {
            if (!sm->checkFilesystemAccess(dir->c_str(), QSEC_WRITE, xsink)) {
                dir->deref();
                return -1;
            }
            dir->deref();
        }
    }
    return priv->chown(uid, gid, xsink);
}
#endif

QoreListNode *QoreDir::stat(ExceptionSink* xsink) const {
    return priv->stat(xsink);
}

QoreHashNode *QoreDir::hstat(ExceptionSink* xsink) const {
    return priv->hstat(xsink);
}

#ifdef Q_HAVE_STATVFS
QoreHashNode *QoreDir::statvfs(ExceptionSink* xsink) const {
    return priv->statvfs(xsink);
}
#endif
