/*
  ExecArgList.cpp

  Qore Programming Language

  Copyright (C) 2003 - 2023 David Nichols

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
#include "qore/intern/ExecArgList.h"
#include "qore/intern/QoreAllocFailure.h"

#include <cstdlib>
#include <cstring>

#define ARG_BLOCK 10

ExecArgList::ExecArgList(const char* str) {
    // copy string as we will edit it in place
    QoreString tmp(str);

    char* start = (char*)tmp.c_str();
    char* p = start;
    int quote = 0;

    while (*p) {
        if (start == p && !quote && (*p == '\'' || *p == '\"')) {
            quote = *p;
            start = p + 1;
            continue;
        }
        ++p;
        if (quote && (*p == '\'' || *p == '\"') && *p == quote) {
            // move characters down to quote position
            memmove(p, p + 1, strlen(p));
            quote = 0;
            --p;
            continue;
        } else if (!quote && *p == ' ') {
            addArgIntern(getString(start, p - start));
            start = p + 1;
        }
    }
    if (*start) {
        addArgIntern(strdup(start));
    }
    // terminate list
    addArgIntern(nullptr);
}

void ExecArgList::addArg(const char* str) {
    addArgIntern(str ? strdup(str) : nullptr);
}

ExecArgList::~ExecArgList() {
    if (arg) {
        for (int i = 0; i < len; ++i) {
            if (arg[i]) {
                free(arg[i]);
            }
        }
        free(arg);
    }
}

char* ExecArgList::getFile() {
    if (len) {
        return arg[0];
    }
    return nullptr;
}

char** ExecArgList::getArgs() {
   return arg;
}

#ifdef DEBUG
void ExecArgList::showArgs() {
    printd(0, "ExecArgList %p len: %d\n", this, len);
    if (!len)
        return;

    QoreString str;

    str.sprintf("file: %s", arg[0]);

    for (int i = 1; i < len; ++i)
        str.sprintf(" [%d]: '%s'", i, arg[i]);

    printd(0, "  %s\n", str.c_str());
}
#endif

char* ExecArgList::getString(const char* start, int size) {
    char* str = (char*)malloc(sizeof(char) * (size + 1));
    strncpy(str, start, size);
    str[size] = '\0';
    //printd(5, "ExecArgList::getString() %d: %s\n", len, str);
    return str;
}

void ExecArgList::addArgIntern(char* str) {
    //printd(5, "ExecArgList::addArgIntern() '%s'\n", str);
    // resize args
    if (len == allocated) {
        // memory that cannot be allocated fails with std::bad_alloc and leaves the list unchanged (qore#7466)
        arg = static_cast<char**>(qore_realloc_or_throw(arg, sizeof(char*) * (allocated + ARG_BLOCK)));
        allocated += ARG_BLOCK;
    }
    arg[len] = str;
    ++len;
}
