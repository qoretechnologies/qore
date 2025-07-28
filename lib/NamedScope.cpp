/*
  NamedScope.cpp

  Qore Programming Language

  Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

  NamedScopes are children of a program object.  there is a parse
  lock per program object to ensure that objects are added (or backed out)
  atomically per program object.  All the objects referenced here should
  be safe to read & copied at all times.  They will only be deleted when the
  program object is deleted (except the pending structures, which will be
  deleted any time there is a parse error, together with all other
  pending structures)

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

void NamedScope::fixBCCall() {
    if (!strlist) {
        strlist = new nslist_t;
        strlist->push_back(ostr);
    }
    // fix last string pointer
    std::string& str = (*strlist)[strlist->size() - 1];
    str.erase(0, 2);
}

NamedScope* NamedScope::copy() const {
   return new NamedScope(strdup(ostr));
}

void NamedScope::init() {
    const char* str = ostr;

    while (char* p = (char*)strstr(str, "::")) {
        if (!strlist) {
            strlist = new nslist_t;
        }
        strlist->push_back(std::string(str, (p - str)));
        str = p + 2;
    }
    if (strlist) {
        strlist->push_back(std::string(str));
    }
}
