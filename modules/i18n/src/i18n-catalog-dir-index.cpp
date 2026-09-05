/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  i18n-catalog-dir-index.cpp

  Qore i18n catalog directory index

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

#include "i18n-catalog-dir-index.h"

#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace {

struct I18nDirIndexCloser {
    void operator()(DIR* dir) const {
        closedir(dir);
    }
};

static std::string i18n_dir_index_join(const std::string& dir, const std::string& name) {
    if (dir.empty()) {
        return name;
    }

    char last = dir[dir.size() - 1];
    if (last == '/' || last == '\\') {
        return dir + name;
    }
    return dir + "/" + name;
}

} // namespace

I18nCatalogDirIndex::EntryQuery I18nCatalogDirIndex::checkRegularFile(const std::string& dir,
        const std::string& name, ExceptionSink* xsink) {
    AutoLocker al(m);
    DirSnapshot* snap = getSnapshot(dir, xsink);
    if (!snap) {
        return EQ_UNAVAILABLE;
    }
    if (snap->scan_errno) {
        if (snap->scan_errno == ENOENT || snap->scan_errno == ENOTDIR) {
            // nothing can exist below a directory that does not exist
            return EQ_ABSENT;
        }
        // the directory could not be read as a whole; a directory that grants search but not read access still
        // resolves individual paths, so the caller must check the path directly
        return EQ_UNAVAILABLE;
    }

    std::map<std::string, EntryType>::iterator i = snap->entries.find(name);
    if (i == snap->entries.end()) {
        return EQ_ABSENT;
    }
    i->second = resolveType(dir, name, i->second);
    return i->second == ET_REGULAR ? EQ_REGULAR : EQ_ABSENT;
}

bool I18nCatalogDirIndex::listJsonFiles(const std::string& dir, std::vector<std::string>& names,
        ExceptionSink* xsink) {
    AutoLocker al(m);
    DirSnapshot* snap = getSnapshot(dir, xsink);
    if (!snap) {
        return false;
    }
    if (snap->scan_errno) {
        // ENOENT and ENOTDIR are normal: a locale usually has no fragment directory.  Any other error is reported
        // by the caller, which owns the error message and the fallback decision.
        return snap->scan_errno == ENOENT || snap->scan_errno == ENOTDIR;
    }

    size_t count = 0;
    for (std::map<std::string, EntryType>::iterator i = snap->entries.begin(), e = snap->entries.end(); i != e;
            ++i) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "listing i18n catalog fragment files")) {
            return false;
        }
        ++count;

        const std::string& name = i->first;
        if (name.size() <= 5 || name.compare(name.size() - 5, 5, ".json")) {
            continue;
        }
        i->second = resolveType(dir, name, i->second);
        if (i->second == ET_REGULAR) {
            names.push_back(name);
        }
    }
    return true;
}

bool I18nCatalogDirIndex::listDirectories(const std::string& dir, std::vector<std::string>& names,
        ExceptionSink* xsink) {
    AutoLocker al(m);
    DirSnapshot* snap = getSnapshot(dir, xsink);
    if (!snap) {
        return false;
    }
    if (snap->scan_errno) {
        return snap->scan_errno == ENOENT || snap->scan_errno == ENOTDIR;
    }

    size_t count = 0;
    for (std::map<std::string, EntryType>::iterator i = snap->entries.begin(), e = snap->entries.end(); i != e;
            ++i) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "listing i18n catalog locale directories")) {
            return false;
        }
        ++count;

        if (i->first == "." || i->first == "..") {
            continue;
        }
        i->second = resolveType(dir, i->first, i->second);
        if (i->second == ET_DIRECTORY) {
            names.push_back(i->first);
        }
    }
    return true;
}

int I18nCatalogDirIndex::getScanError(const std::string& dir, ExceptionSink* xsink) {
    AutoLocker al(m);
    DirSnapshot* snap = getSnapshot(dir, xsink);
    return snap ? snap->scan_errno : 0;
}

int64 I18nCatalogDirIndex::getDirectoryScanCount() const {
    AutoLocker al(m);
    return scan_count;
}

int64 I18nCatalogDirIndex::getDirectoryCount() const {
    AutoLocker al(m);
    return static_cast<int64>(dirs.size());
}

void I18nCatalogDirIndex::clear() {
    AutoLocker al(m);
    dirs.clear();
    scan_count = 0;
}

I18nCatalogDirIndex::DirSnapshot* I18nCatalogDirIndex::getSnapshot(const std::string& dir, ExceptionSink* xsink) {
    std::unordered_map<std::string, DirSnapshot>::iterator i = dirs.find(dir);
    if (i != dirs.end()) {
        return &i->second;
    }

    DirSnapshot snap;
    if (!readDir(dir, snap, xsink)) {
        return nullptr;
    }
    ++scan_count;
    return &dirs.insert(std::make_pair(dir, std::move(snap))).first->second;
}

bool I18nCatalogDirIndex::readDir(const std::string& dir, DirSnapshot& snap, ExceptionSink* xsink) {
    DIR* raw_dir = nullptr;
    while (!(raw_dir = opendir(dir.c_str()))) {
        int err = errno;
        if (err == EINTR) {
            if (qore_check_cancel(xsink, "opening an i18n catalog directory")) {
                return false;
            }
            continue;
        }
        snap.scan_errno = err;
        return true;
    }
    std::unique_ptr<DIR, I18nDirIndexCloser> dir_handle(raw_dir);

    size_t count = 0;
    while (true) {
        errno = 0;
        dirent* entry = readdir(dir_handle.get());
        if (!entry) {
            int err = errno;
            if (!err) {
                break;
            }
            if (err == EINTR) {
                if (qore_check_cancel(xsink, "reading an i18n catalog directory")) {
                    return false;
                }
                continue;
            }
            snap.scan_errno = err;
            snap.entries.clear();
            return true;
        }
        if (count && !(count % 100) && qore_check_cancel(xsink, "indexing an i18n catalog directory")) {
            return false;
        }
        ++count;

        EntryType type = ET_UNKNOWN;
#ifdef DT_REG
        // symbolic links are left unknown so that stat() resolves the target, exactly as a direct check would
        switch (entry->d_type) {
            case DT_REG:
                type = ET_REGULAR;
                break;
            case DT_DIR:
                type = ET_DIRECTORY;
                break;
            case DT_FIFO:
            case DT_SOCK:
            case DT_CHR:
            case DT_BLK:
                type = ET_OTHER;
                break;
            default:
                break;
        }
#endif
        snap.entries[entry->d_name] = type;
    }
    return true;
}

I18nCatalogDirIndex::EntryType I18nCatalogDirIndex::resolveType(const std::string& dir, const std::string& name,
        EntryType type) {
    if (type != ET_UNKNOWN) {
        return type;
    }

    struct stat sbuf;
    std::string path = i18n_dir_index_join(dir, name);
    if (stat(path.c_str(), &sbuf)) {
        return ET_OTHER;
    }
    if (S_ISREG(sbuf.st_mode)) {
        return ET_REGULAR;
    }
    return S_ISDIR(sbuf.st_mode) ? ET_DIRECTORY : ET_OTHER;
}
