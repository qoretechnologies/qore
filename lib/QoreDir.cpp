/* -*- indent-tabs-mode: nil -*- */
/*
    QoreDir.cpp

    Directory functions

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
#include <qore/QoreSandboxManager.h>
#include "qore/intern/QoreDir.h"
#include <sstream>
#include <fstream>
#include <map>

#include "qore/intern/qore_qd_private.h"

const QoreEncoding* QoreDir::getEncoding() const {
    return priv->getEncoding();
}

QoreListNode* QoreDir::listModuleSources(ExceptionSink* xsink) const {
    QoreString regex(".+\\.(qc|ql)$");
    ReferenceHolder<QoreListNode> files(list(xsink, S_IFREG, &regex), xsink);
    if (*xsink) {
        return nullptr;
    }
    // Insertion into an ordered map keeps sorting cancellable, including large modules.
    std::map<std::string, size_t> order;
    for (size_t i = 0; i < files->size(); ++i) {
        if (!(i % 100) && qore_check_cancel(xsink, "module source ordering")) {
            return nullptr;
        }
        QoreStringValueHelper filename(files->retrieveEntry(i));
        order.emplace(filename->c_str(), i);
    }
    ReferenceHolder<QoreListNode> sorted(new QoreListNode(stringTypeInfo), xsink);
    size_t count = 0;
    for (const auto& entry : order) {
        if (!(count++ % 100) && qore_check_cancel(xsink, "module source ordering")) {
            return nullptr;
        }
        sorted->push(files->retrieveEntry(entry.second).refSelf(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return sorted.release();
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

// checks the sandbox filesystem policy, if any, for the given access to the directory, if one is set
static int check_dir_policy(const qore_qd_private& priv, int access_mode, ExceptionSink* xsink) {
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (!smh) {
        return 0;
    }
    SimpleRefHolder<QoreStringNode> dir(priv.get_dir_string());
    if (!dir) {
        return 0;
    }
    return smh->checkFilesystemAccess(dir->c_str(), access_mode, xsink) ? 0 : -1;
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
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (smh) {
        // If absolute path, check directly; otherwise, combine with current dir
        std::string path;
        if (q_absolute_path(ndir)) {
            path = ndir;
        } else {
            path = priv->getPath(ndir);
        }
        if (!smh->checkFilesystemAccess(path.c_str(), QSEC_READ, xsink)) {
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
#ifndef _Q_WINDOWS
//! creates the missing directories of a path under a sandbox policy, each in its parent as the policy checked it,
//! pinned against a concurrent change (qore#7518); the policy was checked with QoreDir::checkCreatePolicy()
/** @param xsink if not null, an error is raised here (DIR-CREATE-FAILURE, or the denial of the policy); if null, a
    denial fails with errno EACCES

    @return the number of directories created, or -1 for an error
*/
static int create_pinned(QoreSandboxManager& sm, const qore_qd_private& priv, int mode, ExceptionSink* xsink) {
    if (priv.getDirname().empty()) {
        if (xsink) {
            xsink->raiseException("DIR-CREATE-ERROR", "cannot create directory; no directory is set");
        }
        return -1;
    }
    name_vec_t missing;
    priv.getMissingDirs(missing);
    int cnt = 0;
    for (const std::string& path : missing) {
        ExceptionSink local_xsink;
        ExceptionSink* x = xsink ? xsink : &local_xsink;
        QoreSandboxPinnedPath pin;
        int rc = q_sandbox_pin_path(sm, path.c_str(), QSEC_CREATE, true, pin, x);
        if (!rc) {
            rc = mkdirat(pin.dirfd, pin.leaf.c_str(), mode);
        }
        if (rc) {
            if (local_xsink) {
                local_xsink.clear();
                errno = EACCES;
            } else if (xsink && !*xsink) {
                xsink->raiseErrnoException("DIR-CREATE-FAILURE", errno, "cannot mkdir '%s'", path.c_str());
            }
            return -1;
        }
        ++cnt;
    }
    return cnt;
}
#endif

int QoreDir::create(int mode, ExceptionSink* xsink) const {
    if (checkCreatePolicy(xsink)) {
        return -1;
    }
#ifndef _Q_WINDOWS
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (smh) {
        return create_pinned(*smh.get(), *priv, mode, xsink);
    }
#endif
    return priv->create(mode, xsink);
}

int QoreDir::checkCreatePolicy(ExceptionSink* xsink) const {
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (!smh) {
        return 0;
    }
    // the directory itself, also when it exists
    if (check_dir_policy(*priv, QSEC_CREATE, xsink)) {
        return -1;
    }
    // each parent directory that would be created, which a rule for the directory itself does not cover
    name_vec_t missing;
    priv->getMissingDirs(missing);
    for (const std::string& path : missing) {
        if (!smh->checkFilesystemAccess(path.c_str(), QSEC_CREATE, xsink)) {
            return -1;
        }
    }
    return 0;
}

// create the directory with all the parent directories if they do not exist
// return amount of created directories, -1 if error
int QoreDir::create(int mode) const {
#ifndef _Q_WINDOWS
    // under a policy, each missing directory is created in its parent as the policy checked it, pinned against a
    // concurrent change (qore#7518); the policy was checked with checkCreatePolicy()
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (smh) {
        return create_pinned(*smh.get(), *priv, mode, nullptr);
    }
#endif
    return priv->create(mode);
}

// list entries of the directory where d points to
// the filter will be applied to the file's mode for filtering.
// directories '.' and '..' will be skipped
QoreListNode* QoreDir::list(ExceptionSink* xsink, int stat_filter, const QoreString* regex, int regex_options, bool full) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return nullptr;
    }
    return priv->list(xsink, stat_filter, regex, regex_options, full, false);
}

QoreListNode* QoreDir::listReadable(ExceptionSink* xsink, int stat_filter, const QoreString* regex, int regex_options,
        bool full) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return nullptr;
    }
    return priv->list(xsink, stat_filter, regex, regex_options, full, true);
}

int QoreDir::mkdir(ExceptionSink* xsink, const char* subdir, int mode) const {
    // Check sandbox security restrictions
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (smh) {
        std::string path;
        if (q_absolute_path(subdir)) {
            path = subdir;
        } else {
            path = priv->getPath(subdir);
        }
        if (!smh->checkFilesystemAccess(path.c_str(), QSEC_CREATE, xsink)) {
            return -1;
        }
#ifndef _Q_WINDOWS
        // the directory is created in the directory that the policy checked, pinned against a concurrent change
        // (qore#7518)
        QoreSandboxPinnedPath pin;
        int rc = q_sandbox_pin_path(*smh.get(), path.c_str(), QSEC_CREATE, true, pin, xsink);
        if (!rc) {
            rc = mkdirat(pin.dirfd, pin.leaf.c_str(), mode);
        }
        if (rc && !*xsink) {
            xsink->raiseErrnoException("DIR-MKDIR-FAILURE", errno, "error creating directory '%s'", path.c_str());
        }
        return rc ? -1 : 0;
#endif
    }
    return priv->mkdir(subdir, mode, xsink);
}

int QoreDir::rmdir(const char* subdir, ExceptionSink* xsink) const {
    // Check sandbox security restrictions
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (smh) {
        std::string path;
        if (q_absolute_path(subdir)) {
            path = subdir;
        } else {
            path = priv->getPath(subdir);
        }
        if (!smh->checkFilesystemAccess(path.c_str(), QSEC_DELETE, xsink)) {
            return -1;
        }
#ifndef _Q_WINDOWS
        // the directory is removed from the directory that the policy checked, pinned against a concurrent change
        // (qore#7518)
        QoreSandboxPinnedPath pin;
        int rc = q_sandbox_pin_path(*smh.get(), path.c_str(), QSEC_DELETE, true, pin, xsink);
        if (!rc) {
            rc = unlinkat(pin.dirfd, pin.leaf.c_str(), AT_REMOVEDIR);
        }
        if (rc && !*xsink) {
            xsink->raiseErrnoException("DIR-RMDIR-FAILURE", errno, "error removing directory '%s'", path.c_str());
        }
        return rc ? -1 : 0;
#endif
    }
    return priv->rmdir(subdir, xsink);
}

std::string QoreDir::getPath(const char* sub) const {
    return priv->getPath(sub);
}

int QoreDir::checkPath() const {
    return priv->checkPath();
}

int QoreDir::checkPath(ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return -1;
    }
    return priv->checkPath();
}

int QoreDir::chmod(int mode, ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_WRITE, xsink)) {
        return -1;
    }
#ifndef _Q_WINDOWS
    // the mode of the directory that the policy checked is changed, pinned against a concurrent change (qore#7518)
    {
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
        std::string path = priv->getDirname();
        if (smh && !path.empty()) {
            QoreSandboxPinnedPath pin;
            int rc = q_sandbox_pin_path(*smh.get(), path.c_str(), QSEC_WRITE, false, pin, xsink);
            if (!rc) {
                rc = q_fchmodat_nofollow(pin.dirfd, pin.leaf.c_str(), mode);
            }
            if (rc && !*xsink) {
                xsink->raiseErrnoException("DIR-CHMOD-FAILURE", errno, "error in Dir::chmod() on '%s'", path.c_str());
            }
            return rc ? -1 : 0;
        }
    }
#endif
    return priv->chmod(mode, xsink);
}

#ifdef HAVE_PWD_H
int QoreDir::chown(uid_t uid, gid_t gid, ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_WRITE, xsink)) {
        return -1;
    }
#ifndef _Q_WINDOWS
    // the ownership of the directory that the policy checked is changed, pinned against a concurrent change
    // (qore#7518)
    {
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
        std::string path = priv->getDirname();
        if (smh && !path.empty()) {
            QoreSandboxPinnedPath pin;
            int rc = q_sandbox_pin_path(*smh.get(), path.c_str(), QSEC_WRITE, false, pin, xsink);
            if (!rc) {
                rc = fchownat(pin.dirfd, pin.leaf.c_str(), uid, gid, AT_SYMLINK_NOFOLLOW);
            }
            if (rc && !*xsink) {
                xsink->raiseErrnoException("DIR-CHOWN-FAILURE", errno, "error in Dir::chown() on '%s'", path.c_str());
            }
            return rc ? -1 : 0;
        }
    }
#endif
    return priv->chown(uid, gid, xsink);
}
#endif

QoreListNode *QoreDir::stat(ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return nullptr;
    }
    return priv->stat(xsink);
}

QoreHashNode *QoreDir::hstat(ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return nullptr;
    }
    return priv->hstat(xsink);
}

#ifdef Q_HAVE_STATVFS
QoreHashNode *QoreDir::statvfs(ExceptionSink* xsink) const {
    if (check_dir_policy(*priv, QSEC_READ, xsink)) {
        return nullptr;
    }
    return priv->statvfs(xsink);
}
#endif
