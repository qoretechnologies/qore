/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  glob.h

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

#ifndef _QORE_INTERN_GLOB_H
#define _QORE_INTERN_GLOB_H

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

#include "qore/intern/QoreRegex.h"

typedef int (*glob_error_t)(const char *, int);

#ifdef _WIN32

// some glob options
#define GLOB_NONE 0
#define GLOB_NOSORT (1 << 0)

class QoreGlobWin {
protected:
   typedef std::vector<std::string> names_t;
   names_t names;

public:
   size_t gl_pathc;
   const char** gl_pathv;

   DLLLOCAL QoreGlobWin() : gl_pathc(0), gl_pathv(0) {
   }

   DLLLOCAL ~QoreGlobWin() {
      reset();
   }

   DLLLOCAL int set(const char* pattern, int flags, glob_error_t errfunc) {
      assert(!flags);
      assert(!errfunc);
      reset();

      // normalize the path
      QoreString path(pattern);

      // save the original dir name
      QoreString orig_dir((const char*)q_dirname(path.c_str()));
      //printd(5, "glob() dir: '%s'\n", orig_dir.c_str());
      if (orig_dir == ".")
         orig_dir.clear();
      else
         orig_dir.concat('\\');

      q_normalize_path(path);
      char* dirp = q_dirname(path.c_str());
      unsigned len = strlen(dirp);
      QoreString dir(dirp, len, len + 1, QCS_DEFAULT);

      // set the pattern to get all files in the directory, and then match according to glob() rules
      dir.concat("\\*.*");
      WIN32_FIND_DATA pfd;
      HANDLE h = ::FindFirstFile(dir.getBuffer(), &pfd);
      ON_BLOCK_EXIT(::FindClose, h);

      // remove wildcard to reuse directory name for matches
      if (len > 1)
         dir.terminate(dir.size() - 3);
      else
         dir.clear();

      // make regex pattern
      QoreString str(q_basenameptr(path.c_str()));

      // check if we should get files that start with a "."
      bool get_dot = (str[0] == '.');
      //printd(5, "QoreGlobWin::set() path: '%s' dir: '%s' str: '%s' get_dot: %d\n", path.c_str(), dir.c_str(), str.c_str());

      str.replaceAll(".", "\\.");
      str.replaceAll("?", ".");
      str.replaceAll("*", ".*");
      str.prepend("^");
      str.concat("$");

      ExceptionSink xsink;
      QoreRegex qrn(&str, PCRE2_CASELESS, &xsink);
      if (xsink)
	 return -1;

      while (FindNextFile(h, &pfd)) {
         if (pfd.cFileName[0] == '.' && !get_dot)
            continue;
	 if (qrn.exec(pfd.cFileName, strlen(pfd.cFileName))) {
            QoreString str(orig_dir);
            str.concat(pfd.cFileName);
	    names.push_back(str.c_str());
	    //printd(5, "QoreGlobWin::set(pattern='%s') dir='%s' regex='%s' %s MATCHED\n", pattern, dir.getBuffer(), str.getBuffer(), pfd.cFileName);
	 }
      }

      if (names.size()) {
         if (!(flags & GLOB_NOSORT))
            std::sort(names.begin(), names.end());

	 gl_pathc = names.size();

	 gl_pathv = (const char**)malloc(sizeof(char*) * names.size());
	 for (unsigned i = 0; i < names.size(); ++i) {
	    gl_pathv[i] = names[i].c_str();
	 }
      }

      return 0;
   }

   DLLLOCAL int reset() {
      names.clear();
      gl_pathc = 0;
      if (gl_pathv) {
	 free(gl_pathv);
	 gl_pathv = 0;
      }
      return 0;
   }
};

typedef QoreGlobWin glob_t;
#else
#error no glob implementation for this platform
#endif

// check prototypes
DLLLOCAL int glob(const char *pattern, int flags, glob_error_t errfunc, glob_t *buf);
DLLLOCAL int globfree(glob_t *buf);

#endif
