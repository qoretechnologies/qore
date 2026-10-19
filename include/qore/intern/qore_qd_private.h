/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_qd_private.h

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

#ifndef _QORE_QORE_QD_PRIVATE_H

#define _QORE_QORE_QD_PRIVATE_H

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _Q_WINDOWS
static int mkdir(const char* path, mode_t mode) {
    return mkdir(path);
}
#endif

class qore_qd_private {
protected:
    const QoreEncoding* enc;
    std::string dirname;
    mutable QoreThreadLock m;

    DLLLOCAL static bool is_dir_sep(const std::string& str) {
        for (std::string::size_type i = 0, e = str.size(); i < e; ++i) {
#ifdef _Q_WINDOWS
            if (str[i] != '\\' && str[i] != '/')
                return false;
#else
            if (str[i] != QORE_DIR_SEP)
                return false;
#endif
        }
        return true;
    }

    DLLLOCAL static std::string::size_type get_first_non_dir_sep(const std::string& str, std::string::size_type pos = 0) {
        for (std::string::size_type i = pos, e = str.size(); i < e; ++i) {
#ifdef _Q_WINDOWS
            if (str[i] != '\\' && str[i] != '/')
                return i;
#else
            if (str[i] != QORE_DIR_SEP)
                return i;
#endif
        }
        return std::string::npos;
    }

    DLLLOCAL static std::string::size_type get_first_dir_sep(const std::string& str, std::string::size_type pos = 0) {
        for (std::string::size_type i = pos, e = str.size(); i < e; ++i) {
#ifdef _Q_WINDOWS
            if (str[i] == '\\' || str[i] == '/')
                return i;
#else
            if (str[i] == QORE_DIR_SEP)
                return i;
#endif
        }
        return std::string::npos;
    }

    // tokenize the strings by directory separators
    DLLLOCAL static void tokenize(const std::string& str, name_vec_t& tokens) {
        // accommodate case when the string consists of only the delimiter (ex: "/")
        if (is_dir_sep(str)) {
            tokens.push_back(QORE_DIR_SEP_STR);
            return;
        }

        // Skip delimiters at beginning.
        std::string::size_type lastPos = get_first_non_dir_sep(str, 0);
        // Find first "non-delimiter".
        std::string::size_type pos     = get_first_dir_sep(str, lastPos);

        while (std::string::npos != pos || std::string::npos != lastPos) {
            // Found a token, add it to the vector.
            tokens.push_back(str.substr(lastPos, pos - lastPos));
            // Skip delimiters.  Note the "not_of"
            lastPos = get_first_non_dir_sep(str, pos);
            // Find next "non-delimiter"
            pos = get_first_dir_sep(str, lastPos);
        }
    }

    // check if the given directory is accessible
    // return errno of opendir function
    // unlocked
    DLLLOCAL static int verifyDirectory(const std::string &dir) {
        DIR* dptr;
        dptr = opendir(dir.c_str());
        if (!dptr)
            return errno;

        // free again
        closedir(dptr);
        return 0;
    }

    // unlocked
    DLLLOCAL std::string getPathIntern(const char* sub) const {
        if (!dirname.empty())
            return dirname + QORE_DIR_SEP_STR + std::string(sub);
        return std::string(sub);
    }

    // check if the path in dirname exists
    // return 0 if the path exists
    // return errno of the opendir function
    // unlocked
    DLLLOCAL int checkPathIntern() const {
        return dirname.empty() ? -1 : verifyDirectory(dirname);
    }

    // returns the directories of the path in dirname that do not exist, from the top of the path down, and the
    // number of leading UNC path components skipped on Windows (always 0 on other platforms)
    // unlocked
    DLLLOCAL int getMissingDirsIntern(name_vec_t& missing) const {
        assert(!dirname.empty());
        int skipped = 0;
#ifdef _Q_WINDOWS
        // flag UNC paths for special processing, because the first two components of UNC paths designate the server
        // location and cannot be created with mkdir()
        bool unc = (dirname[0] == '/' || dirname[0] == '\\')
            && (dirname[1] == '/' || dirname[1] == '\\')
            && (dirname[2] != '/' && dirname[2] != '\\');
        // issue #2529: we have to use q_absolute_path_windows() here and not in the loop
        bool abs = q_absolute_path_windows(dirname.c_str());
#endif

        // split the directory in its subdirectories tree
        name_vec_t dirs;
        tokenize(dirname, dirs);

        std::string path;
        for (name_vec_t::iterator it = dirs.begin(); it < dirs.end(); it++) {
#ifdef _Q_WINDOWS
            if (it == dirs.begin() && abs)
                path += *it;
            else
#endif
            path += QORE_DIR_SEP_STR + (*it); // the actual path
#ifdef _Q_WINDOWS
            // ignore the first two components of UNC paths
            if (unc && skipped < 2) {
                ++skipped;
                continue;
            }
#endif
            if (verifyDirectory(path)) { // not existing
                missing.push_back(path);
            }
        }
        return skipped;
    }

public:
    DLLLOCAL qore_qd_private(ExceptionSink* xsink, const QoreEncoding* cs, const char* dir) : enc(cs) {
        if (dir) {
            dirname = dir;
            return;
        }

        // set the directory to the cwd
        char* cwd = (char*)malloc(sizeof(char)*QORE_PATH_MAX);
        if (!cwd) { // error in malloc
            xsink->outOfMemory();
            return;
        }
        ON_BLOCK_EXIT(free, cwd);

        if (getcwd(cwd, (size_t)QORE_PATH_MAX))
            dirname = cwd;
    }

    DLLLOCAL qore_qd_private(ExceptionSink* xsink, const qore_qd_private &old) {
        AutoLocker al(old.m);
        enc = old.enc;
        dirname = old.dirname;
    }

    DLLLOCAL QoreStringNode* get_dir_string() const {
        AutoLocker al(m);
        return !dirname.empty() ? new QoreStringNode(dirname, enc) : 0;
    }

    DLLLOCAL std::string getPath(const char* sub) const {
        AutoLocker al(m);

        return getPathIntern(sub);
    }

    DLLLOCAL int checkPath() const {
        AutoLocker al(m);

        return checkPathIntern();
    }

    // returns 0 = OK, > 0 = dir does not exist, -1 = error
    DLLLOCAL int chdir(const char* ndir, ExceptionSink* xsink = nullptr) {
        assert(ndir);

        //printd(5, "qore_qd_private::chdir() ndir: '%s' dirname: '%s'\n", ndir, dirname.c_str());

        // if changing to the current directory, then ignore
        if (ndir[0] == '.') {
            const char* p = ndir + 1;
#ifdef _Q_WINDOWS
            while (*p && (*p == '\\' || *p == '/'))
#else
            while (*p && *p == QORE_DIR_SEP)
#endif
                ++p;
            if (!*p)
                return 0;
        }

        // if relative path then join with the old path and strip the path
        std::string ds;

        AutoLocker al(m);
        if (!q_absolute_path(ndir)) {
            if (dirname.empty()) {
                if (xsink) {
                    xsink->raiseException("DIR-CHDIR-ERROR", "cannot change to relative directory because no current directory is set");
                }
                return -1;
            }

            ds = dirname + QORE_DIR_SEP + std::string(ndir);
        } else
            ds = ndir;

        ds = normalizePath(ds);
        dirname = ds;

        //printd(5, "qore_qd_private::chdir() ndir: '%s' ds: '%s'\n", ndir, ds.c_str());

        return checkPathIntern();
    }

    DLLLOCAL int mkdir(const char* subdir, int mode, ExceptionSink* xsink) const {
        assert(subdir);
        AutoLocker al(m);

        std::string path = getPathIntern(subdir);
        if (::mkdir(path.c_str(), mode)) {
            xsink->raiseErrnoException("DIR-MKDIR-FAILURE", errno, "error creating directory '%s'", path.c_str());
            return -1;
        }
        return 0;
    }

    DLLLOCAL int rmdir(const char* subdir, ExceptionSink* xsink) const {
        assert(subdir);
        AutoLocker al(m);

        std::string path = getPathIntern(subdir);
        if (::rmdir(path.c_str())) {
            xsink->raiseErrnoException("DIR-RMDIR-FAILURE", errno, "error removing directory '%s'", path.c_str());
            return -1;
        }

        return 0;
    }

    // omit_unreadable: leave out the entries that the sandbox policy, if any, does not allow to be read
    DLLLOCAL QoreListNode* list(ExceptionSink* xsink, int stat_filter, const QoreString* regex, int regex_options,
            bool full, bool omit_unreadable) const;

    DLLLOCAL int create(int mode, ExceptionSink* xsink = nullptr) const {
        AutoLocker al(m);

        if (dirname.empty()) {
            if (xsink) {
                xsink->raiseException("DIR-CREATE-ERROR", "cannot create directory; no directory is set");
            }
            return -1;
        }

        // create all the directories of the path that do not exist; the skipped UNC path components are counted as
        // before
        name_vec_t missing;
        int cnt = getMissingDirsIntern(missing);
        for (const std::string& path : missing) {
            if (::mkdir(path.c_str(), mode)) { // failed
                if (xsink) {
                    xsink->raiseErrnoException("DIR-CREATE-FAILURE", errno, "cannot mkdir '%s'", path.c_str());
                }
                return -1;
            }
            cnt++;
        }

        return cnt;
    }

    //! returns the directories that create() would create, from the top of the path down
    DLLLOCAL void getMissingDirs(name_vec_t& missing) const {
        AutoLocker al(m);
        if (!dirname.empty()) {
            getMissingDirsIntern(missing);
        }
    }

    DLLLOCAL int chmod(int mode, ExceptionSink* xsink) const {
        AutoLocker al(m);

        if (dirname.empty()) {
            xsink->raiseException("DIR-CHMOD-ERROR", "cannot change directory mode; no directory is set");
            return -1;
        }

        if (::chmod(dirname.c_str(), mode)) {
            xsink->raiseErrnoException("DIR-CHMOD-FAILURE", errno, "error in Dir::chmod() on '%s'", dirname.c_str());
            return -1;
        }

        return 0;
    }

#ifdef HAVE_PWD_H
    DLLLOCAL int chown(uid_t uid, gid_t gid, ExceptionSink* xsink) const {
        AutoLocker al(m);

        if (dirname.empty()) {
            xsink->raiseException("DIR-CHOWN-ERROR", "cannot change directory ownership; no directory is set");
            return -1;
        }

        if (::chown(dirname.c_str(), uid, gid)) {
            xsink->raiseErrnoException("DIR-CHOWN-FAILURE", errno, "error in Dir::chown() on '%s'", dirname.c_str());
            return 0;
        }

        return 0;
    }
#endif

    DLLLOCAL QoreListNode* stat(ExceptionSink* xsink) const {
        AutoLocker al(m);

        if (dirname.empty()) {
            xsink->raiseException("DIR-STAT-ERROR", "cannot stat; no directory is set");
            return 0;
        }

        struct stat sbuf;
        if (::stat(dirname.c_str(), &sbuf)) {
            xsink->raiseErrnoException("DIR-STAT-FAILURE", errno, "stat() call failed on '%s'", dirname.c_str());
            return 0;
        }

        return stat_to_list(sbuf);
    }

    DLLLOCAL QoreHashNode* hstat(ExceptionSink* xsink) const;

#ifdef Q_HAVE_STATVFS
    DLLLOCAL QoreHashNode* statvfs(ExceptionSink* xsink) const {
        AutoLocker al(m);

        if (dirname.empty()) {
            xsink->raiseException("DIR-STATVFS-ERROR", "cannot execute File::statvfs(); no directory is set");
            return nullptr;
        }

        struct statvfs vfs;
        if (::statvfs(dirname.c_str(), &vfs)) {
            xsink->raiseErrnoException("DIR-STATVFS-FAILURE", errno, "statvfs() call failed on '%s'", dirname.c_str());
            return nullptr;
        }

        return statvfs_to_hash(vfs);
    }
#endif

   const QoreEncoding* getEncoding() const {
      return enc;
   }

#ifdef HAVE_LSTAT
   DLLLOCAL static int lstat(const char* str, struct stat& buf, ExceptionSink* xsink) {
      int rc = ::lstat(str, &buf);
      if (rc) {
         xsink->raiseErrnoException("DIR-READ-FAILURE", errno, "lstat() failed on '%s'", str);
         return -1;
      }
      return 0;
   }
#endif

   DLLLOCAL static int stat(const char* str, struct stat& buf, ExceptionSink* xsink) {
      int rc = ::stat(str, &buf);
      if (rc) {
         xsink->raiseErrnoException("DIR-READ-FAILURE", errno, "stat() failed on '%s'", str);
         return -1;
      }
      return 0;
   }

    // tokenizes the string (path) and recreates it
    DLLLOCAL static const std::string normalizePath(const std::string& odir) {
#ifdef _Q_WINDOWS
        // flag UNC paths for special processing, because otherwise they will be normalized to a single leading backslash
        bool unc = (odir[0] == '/' || odir[0] == '\\')
            && (odir[1] == '/' || odir[1] == '\\')
            && (odir[2] != '/' && odir[2] != '\\');
#endif

        // tokenize the string
        name_vec_t ptoken, dirs;
        tokenize(odir, ptoken);

        // push them to the new path
        for (name_vec_t::iterator it = ptoken.begin(), et = ptoken.end(); it != et; ++it) {
            std::string d = *it;
            if (d == "." || d == "") // ignore
                continue;

            if (d == ".." && !dirs.empty()) // step back one step
                dirs.pop_back();
            else
                dirs.push_back(d);
        }

        // create string out of rest..
        std::string ret;
#ifdef _Q_WINDOWS
        if (unc)
            ret += '\\';
        // issue #2529: we have to use q_absolute_path_windows() here and not in the loop
        bool abs = q_absolute_path_windows(odir.c_str());
#endif
        for (name_vec_t::iterator it = dirs.begin(), et = dirs.end(); it != et; ++it) {
#ifdef _Q_WINDOWS
            if (it == dirs.begin() && abs)
                ret += *it;
            else
#endif
            ret += QORE_DIR_SEP_STR + (*it);
        }

        //printd(5, "qore_qd_private::normalizePath() odir: '%s' ret: '%s' unc: %d\n", odir.c_str(), ret.c_str(), unc);
        return ret;
    }
};

#endif
