/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_TermIOS.h

  Qore Programming Language

  Copyright (C) 2003 - 2026 Qore Technologies

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

#ifndef _QORE_QC_TERMIOS_H
#define _QORE_QC_TERMIOS_H

DLLLOCAL extern QoreClass *QC_TERMIOS;
DLLEXPORT extern qore_classid_t CID_TERMIOS;
DLLLOCAL QoreClass *initTermIOSClass(QoreNamespace& ns);

#ifdef HAVE_TERMIOS_H

// cc_t typedef for use in qpp files (from termios.h)
typedef unsigned char cc_t;

// Forward declaration only - implementation is in TermIOS.cpp
// This prevents termios.h macros from polluting compilation units that include LLVM headers
class QoreTermIOS : public AbstractPrivateData {
public:
    DLLLOCAL QoreTermIOS();
    DLLLOCAL QoreTermIOS(const QoreTermIOS &cp);
    DLLLOCAL ~QoreTermIOS();
    DLLLOCAL bool is_equal(const QoreTermIOS *qtios);
    DLLLOCAL int get(int fd, ExceptionSink *xsink);
    DLLLOCAL int set(int fd, int action, ExceptionSink *xsink);
    DLLLOCAL void set_lflag(unsigned int val);
    DLLLOCAL void set_cflag(unsigned int val);
    DLLLOCAL void set_oflag(unsigned int val);
    DLLLOCAL void set_iflag(unsigned int val);
    DLLLOCAL unsigned int get_lflag() const;
    DLLLOCAL unsigned int get_cflag() const;
    DLLLOCAL unsigned int get_oflag() const;
    DLLLOCAL unsigned int get_iflag() const;
    DLLLOCAL unsigned char get_cc(int64 offset, ExceptionSink *xsink);
    DLLLOCAL int set_cc(int64 offset, unsigned char val, ExceptionSink *xsink);
    DLLLOCAL static int getWindowSize(int &rows, int &columns, ExceptionSink *xsink);

private:
    // Private implementation - opaque to avoid exposing termios.h here
    void* termios_ptr;
    friend class QoreTermIOSImpl;
};

#endif // HAVE_TERMIOS_H

#endif // _QORE_QC_TERMIOS_H
