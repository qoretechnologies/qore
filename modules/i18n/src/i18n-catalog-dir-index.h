/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  i18n-catalog-dir-index.h

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

#ifndef I18N_CATALOG_DIR_INDEX_H
#define I18N_CATALOG_DIR_INDEX_H

#include "qore/Qore.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

//! Snapshot of the catalog directories consulted by native catalog discovery
/** Discovering catalog files for one domain and locale probes a small number of candidate paths and scans one
    fragment directory for every search root and locale-fallback entry.  Building catalogs for many domains and
    locales repeats those reads against the same directories; this index reads each directory at most once and
    answers every later query for it from the snapshot.

    The index deliberately has no invalidation: how stale its view of the filesystem can become is bounded by its
    lifetime, which is why it is owned by the caller and scoped to one operation instead of being a process-global
    filesystem cache.

    The class performs no access control.  Callers apply exactly the same sandbox checks they apply without an
    index, and skip the index for directories that a sandbox manager does not allow them to read as a whole.

    The class is thread-safe.
*/
class I18nCatalogDirIndex : public AbstractPrivateData {
public:
    //! Result of an indexed directory-entry query
    enum EntryQuery {
        //! the directory could not be read; the caller must fall back to a direct path check
        EQ_UNAVAILABLE = -1,
        //! the entry does not exist or is not a regular file
        EQ_ABSENT = 0,
        //! the entry is a regular file
        EQ_REGULAR = 1,
    };

    //! Returns whether \a dir + \a name is a regular file according to the snapshot
    /** @param dir directory holding the entry
        @param name entry name
        @param xsink Qore-language exception sink

        @return @ref EQ_REGULAR, @ref EQ_ABSENT, or @ref EQ_UNAVAILABLE when the directory cannot be read as a
            whole; a directory that does not exist yields @ref EQ_ABSENT rather than @ref EQ_UNAVAILABLE
    */
    DLLLOCAL EntryQuery checkRegularFile(const std::string& dir, const std::string& name, ExceptionSink* xsink);

    //! Appends the names of all regular \c ".json" files in \a dir to \a names in filename order
    /** @param dir directory to list
        @param names sorted names are appended here
        @param xsink Qore-language exception sink

        @return @c false if the directory cannot be indexed and the caller must scan it directly; the directory not
            existing is not a failure and yields no names
    */
    DLLLOCAL bool listJsonFiles(const std::string& dir, std::vector<std::string>& names, ExceptionSink* xsink);

    //! Returns the errno recorded for a failed scan of \a dir, or 0 if the directory was read
    DLLLOCAL int getScanError(const std::string& dir, ExceptionSink* xsink);

    //! Returns the number of directories read from disk since the index was created or last cleared
    DLLLOCAL int64 getDirectoryScanCount() const;

    //! Returns the number of directories currently held in the snapshot
    DLLLOCAL int64 getDirectoryCount() const;

    //! Discards the snapshot; the scan count is reset as well
    DLLLOCAL void clear();

private:
    //! Directory entry type; unknown types are resolved with stat() on first use
    enum EntryType : unsigned char {
        ET_UNKNOWN = 0,
        ET_REGULAR,
        ET_OTHER,
    };

    //! One directory as read from disk
    struct DirSnapshot {
        //! errno from a failed scan, or 0 when the directory was read successfully
        int scan_errno = 0;
        //! entry name -> type, in filename order
        std::map<std::string, EntryType> entries;
    };

    mutable QoreThreadLock m;
    std::unordered_map<std::string, DirSnapshot> dirs;
    int64 scan_count = 0;

    //! Returns the snapshot for \a dir, reading the directory if it is not present; the lock must be held
    /** @return the snapshot, or @c nullptr if a Qore-language exception was raised
    */
    DLLLOCAL DirSnapshot* getSnapshot(const std::string& dir, ExceptionSink* xsink);

    //! Reads \a dir into \a snap; the lock must be held
    DLLLOCAL bool readDir(const std::string& dir, DirSnapshot& snap, ExceptionSink* xsink);

    //! Resolves an unknown entry type with stat(); the lock must be held
    DLLLOCAL EntryType resolveType(const std::string& dir, const std::string& name, EntryType type);
};

#endif
