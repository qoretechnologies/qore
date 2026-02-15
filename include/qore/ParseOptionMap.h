/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ParseOptionMap.h
 
  Qore Programming language
 
  Copyright (C) 2003 - 2026 David Nichols
 
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

#ifndef _QORE_PARSEOPTIONMAP_H
#define _QORE_PARSEOPTIONMAP_H

#include <qore/Restrictions.h>

typedef std::map<const char *, int64, ltstr> opt_map_t;
typedef std::map<int64, const char *> rev_opt_map_t;

//! provides access to parse option information
class ParseOptionMap {
   private:
      DLLLOCAL static opt_map_t map;
      DLLLOCAL static rev_opt_map_t rmap;

      // not implemented
      DLLLOCAL ParseOptionMap(const ParseOptionMap&);
      DLLLOCAL ParseOptionMap& operator=(const ParseOptionMap&);
      
   public:
      DLLLOCAL ParseOptionMap();
      DLLLOCAL static void static_init();

      //! find a parse option name from its code
      DLLEXPORT static const char *find_name(int code);

      //! find a parse option code from its name
      /** @deprecated do not use; uses the wrong return type; use find_code64() instead */
      DLLEXPORT static int find_code(const char *name);

      //! find a parse option code from its name
      DLLEXPORT static int64 find_code64(const char *name);

      //! print out all parse optionsto stdout
      DLLEXPORT static void list_options();

      DLLLOCAL static QoreHashNode* getCodeToStringMap();
      DLLLOCAL static QoreHashNode* getStringToCodeMap();
};

#endif
