/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreDir.h

    thread-safe Directory object with a private implementation

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

#ifndef _QORE_QOREDIR_H

#define _QORE_QOREDIR_H

#include <dirent.h>
#include <string>
#include <vector>

#ifndef HAVE_DIRENT_D_TYPE
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

//! provides controlled access to the filesystem through directories
/** Each QoreDir object has a default character encoding associated with it.  The class is thread-safe.
    @see QoreEncoding
*/
class QoreDir {
public:
    //! creates the object and sets the default encoding
    /**
        @param xsink if an out of memory error occurs in the constructor
        @param cs the encoding to use for this directory
        @param dir the initial directory; 0 = the current directory
    */
    DLLEXPORT QoreDir(ExceptionSink* xsink, const QoreEncoding* cs = QCS_DEFAULT, const char* dir = nullptr);

    //! copies the object
    /** @param xsink a Qore-language exception can be raised only on out of memory
        @param old the old object to copy
    */
    DLLEXPORT QoreDir(ExceptionSink* xsink, const QoreDir& old);

    //! closes the directory and frees all memory allocated to the object
    DLLEXPORT ~QoreDir();

    //! checks whether file exists
    /**
        @param filePath file path
        @return true if file exists
     */
    DLLEXPORT static bool file_exists(const QoreString& filePath);

    //! checks whether folder exists
    /**
        @param folderPath folder path
        @param xsink a Qore-language exception can be raised only on out of memory
        @param cs the encoding to use for this directory
        return 0 = OK (folder exists) non-zero = errno returned by opendir()
     */
    DLLEXPORT static bool folder_exists(const QoreString& folderPath, ExceptionSink& xsink, const QoreEncoding* cs = QCS_DEFAULT);

    //! reads file content
    /**
        @param fullPath file name
        @return file content
     */
    DLLEXPORT static std::string get_file_content(const QoreString& fullPath);

    //! look up files
    /**
        @param path path where files should be looked up
        @param xsink a Qore-language exception can be raised only on out of memory
        @param regex filter
        @param cs the encoding to use for this directory
        @return list of files
     */
    DLLEXPORT static QoreListNode* get_files(const QoreString& path, ExceptionSink& xsink, QoreString* regex = nullptr, const QoreEncoding* cs = QCS_DEFAULT);

    //! changes the directory in relation to the current
    /**
        @param dir the directory to change to. can include .. and .
        @param xsink if the path is relative and no current directory is set, a qore-language exception is thrown
        @return 0 for success
    */
    DLLEXPORT int chdir(const char* dir, ExceptionSink* xsink);

    //! changes the directory in relation to the current
    /**
        @param dir the directory to change to. can include .. and .

        @return 0 for success, -1 for error
    */
    DLLEXPORT int chdir(const char* dir);

    //! creates a subdirectory of the current directory
    /** @param xsink if an error occurs, qore-language exception information is added here
        @param subdir the subdirectory name to create
        @param mode the mode of the directory to create (default = 0777)
        @return 0 = OK, -1 = an exception was raised
    */
    DLLEXPORT int mkdir(ExceptionSink* xsink, const char* subdir, int mode = 0777) const;

    //! removes a subdirectory of the current directory
    /** @param subdir the subdirectory name to remove
        @param xsink if errors occur, qore-language exception information is added here
        @return 0 = OK, -1 = an exception was raised
    */
    DLLEXPORT int rmdir(const char* subdir, ExceptionSink* xsink) const;

    //! changes the mode of the current directory
    /** @param mode the moddoe to change the directory to
        @param xsink if errors occur, qore-language exception information is added here
        @return 0 = OK, -1 = an exception was raised
    */
    DLLEXPORT int chmod(int mode, ExceptionSink* xsink) const;

#ifdef HAVE_PWD_H
    //! changes the user and/or group owner for the current directory
    /** @param uid the UID to change to (-1 = leave the same)
        @param gid the GID to change to (-1 = leave the same)
        @param xsink if errors occur, qore-language exception information is added here
        @return 0 = OK, -1 = an exception was raised
    */
    DLLEXPORT int chown(uid_t uid, gid_t gid, ExceptionSink* xsink) const;
#endif

    //! creates the current directory, including all parent directories, if they do not exist
    /** @param mode the mode for any directoreis created
        @param xsink if errors occur, qore-language exception information is added here
        @return the number of directories created, -1 = error
    */
    DLLEXPORT int create(int mode, ExceptionSink* xsink) const;

    //! creates the current directory, including all parent directories, if they do not exist
    /** @param mode the mode for any directoreis created

        @return the number of directories created, -1 = error

        @since %Qore 0.9.5
    */
    DLLEXPORT int create(int mode) const;

    //! returns 0 = OK (path exists), non-zero = errno returned by opendir()
    /** @return 0 = OK (path exists), non-zero = errno returned by opendir()
    */
    DLLEXPORT int checkPath() const;

    //! returns the current directory name or 0 if none is set
    /** @return the current directory name or 0 if none is set
    */
    DLLEXPORT QoreStringNode* dirname() const;

    //! returns a complete path with the argument appended to the current directory name
    /** @param sub the subdirectory name to append to the current directory name
        @return a complete path with the argument appended to the current directory name
    */
    DLLEXPORT std::string getPath(const char* sub) const;

    //! returns the encoding used for the filesystem
    /** @return the encoding used for the filesystem
    */
    DLLEXPORT const QoreEncoding* getEncoding() const;

    //! returns a new file in this directory
    /**
        @param xsink if errors occur, qore-language exception information is added here
        @param fn the name of the file to be opened
        @param flags the flags to use when opening the file
        @param mode the mode mask to use when opening the file
        @param cs the encoding to use for the file
        @return the File class, throws an exception on error
    */
    DLLEXPORT QoreFile openFile(ExceptionSink* xsink, const char* fn, int flags = O_RDONLY, int mode = 0777, const QoreEncoding* cs = QCS_DEFAULT);

    //! returns a list of files in the current directory, taking an option regular expression filter
    /** @param xsink if errors occur, qore-language exception information is added here
        @param stat_filter set to -1 to get everything, otherwise use S_* constants from stat() to filter for particular file types
        @param regex an optional regular expression to filter the resulting list
        @param regex_options optional regular expression options
        @param full if false a list of file names is returned, if true then a list of DirStatInfo hashes is returned
        @return a typed list of the results
    */
    DLLEXPORT QoreListNode* list(ExceptionSink* xsink, int stat_filter, const QoreString* regex = nullptr, int regex_options = 0, bool full = false) const;

    //! returns a QoreListNode with directory status information
    /** @param xsink if an error occurs, the Qore-language exception info will be added here
        @return a QoreListNode with directory status information
    **/
    DLLEXPORT QoreListNode* stat(ExceptionSink* xsink) const;

    //! returns a QoreHashNode with directory status information
    /** @param xsink if an error occurs, the Qore-language exception info will be added here
        @return a QoreHashNode with directory status information
    **/
    DLLEXPORT QoreHashNode* hstat(ExceptionSink* xsink) const;

    //! returns a QoreHashNode with filesystem status information
    /** @param xsink if an error occurs, the Qore-language exception info will be added here
        @return a QoreHashNode with filesystem status information
    **/
    DLLEXPORT QoreHashNode* statvfs(ExceptionSink* xsink) const;

private:
    //! private implementation
    class qore_qd_private *priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreDir(const QoreDir&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreDir& operator=(const QoreDir&);
};

#endif  // _QORE_QOREDIR_H
