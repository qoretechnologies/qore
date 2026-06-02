/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_TermIOS.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QC_TermIOS.h"

#ifdef HAVE_TERMIOS_H

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Actual implementation class that uses termios.h types
class QoreTermIOSImpl {
public:
    struct termios ios;

    QoreTermIOSImpl() {
        memset(&ios, 0, sizeof(struct termios));
    }

    QoreTermIOSImpl(const QoreTermIOSImpl &cp) {
        memcpy(&ios, &cp.ios, sizeof(struct termios));
    }

    bool is_equal(const QoreTermIOSImpl *other) const {
        return !memcmp(&ios, &other->ios, sizeof(struct termios));
    }

    int check_offset(int64 offset, ExceptionSink *xsink) {
        if (offset < 0) {
            xsink->raiseException("TERMIOS-CC-ERROR", "cc offset (%lld) is < 0", offset);
            return -1;
        }

        if (offset > NCCS) {
            xsink->raiseException("TERMIOS-CC-ERROR", "cc offset (%lld) is > NCCS (%d)", offset, NCCS);
            return -1;
        }
        return 0;
    }

    int get(int fd, ExceptionSink *xsink) {
        int rc = tcgetattr(fd, &ios);
        if (rc) {
            xsink->raiseException("TERMIOS-GET-ERROR", q_strerror(errno));
            return rc;
        }
        return 0;
    }

    int set(int fd, int action, ExceptionSink *xsink) {
        // WARNING!: tcsetattr returns 0 if any changes were made, not if all
        //           changes were made
        int rc = tcsetattr(fd, action, &ios);
        if (rc) {
            xsink->raiseException("TERMIOS-SET-ERROR", q_strerror(errno));
            return rc;
        }
        return 0;
    }

    void set_lflag(unsigned int val) {
        ios.c_lflag = val;
    }

    void set_cflag(unsigned int val) {
        ios.c_cflag = val;
    }

    void set_oflag(unsigned int val) {
        ios.c_oflag = val;
    }

    void set_iflag(unsigned int val) {
        ios.c_iflag = val;
    }

    unsigned int get_lflag() const {
        return ios.c_lflag;
    }

    unsigned int get_cflag() const {
        return ios.c_cflag;
    }

    unsigned int get_oflag() const {
        return ios.c_oflag;
    }

    unsigned int get_iflag() const {
        return ios.c_iflag;
    }

    unsigned char get_cc(int64 offset, ExceptionSink *xsink) {
        if (check_offset(offset, xsink))
            return -1;
        return ios.c_cc[offset];
    }

    int set_cc(int64 offset, unsigned char val, ExceptionSink *xsink) {
        if (check_offset(offset, xsink))
            return -1;
        ios.c_cc[offset] = val;
        return 0;
    }

    static int getWindowSize(int &rows, int &columns, ExceptionSink *xsink) {
        struct winsize ws;

        int fd = open("/dev/tty", O_RDONLY);
        if (fd == -1) {
            xsink->raiseErrnoException("TERMIOS-GET-WINDOW-SIZE-ERROR", errno, "cannot open controlling terminal");
            return -1;
        }

        if (ioctl(fd, TIOCGWINSZ, &ws)) {
            xsink->raiseErrnoException("TERMIOS-GET-WINDOW-SIZE-ERROR", errno, "error reading window size");

            if (close(fd))
                xsink->raiseErrnoException("TERMIOS-GET-WINDOW-SIZE-ERROR", errno, "error closing controlling terminal");

            return -1;
        }

        if (close(fd)) {
            xsink->raiseErrnoException("TERMIOS-GET-WINDOW-SIZE-ERROR", errno, "error closing controlling terminal");
            return -1;
        }

        rows = ws.ws_row;
        columns = ws.ws_col;
        return 0;
    }
};

// Public interface implementations
QoreTermIOS::QoreTermIOS() : termios_ptr(new QoreTermIOSImpl()) {
}

QoreTermIOS::QoreTermIOS(const QoreTermIOS &cp) : termios_ptr(nullptr) {
    if (cp.termios_ptr) {
        termios_ptr = new QoreTermIOSImpl(*static_cast<QoreTermIOSImpl*>(cp.termios_ptr));
    }
}

QoreTermIOS::~QoreTermIOS() {
    delete static_cast<QoreTermIOSImpl*>(termios_ptr);
}

bool QoreTermIOS::is_equal(const QoreTermIOS *qtios) {
    if (!termios_ptr || !qtios || !qtios->termios_ptr) {
        return termios_ptr == qtios->termios_ptr;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->is_equal(static_cast<QoreTermIOSImpl*>(qtios->termios_ptr));
}

int QoreTermIOS::get(int fd, ExceptionSink *xsink) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get(fd, xsink);
}

int QoreTermIOS::set(int fd, int action, ExceptionSink *xsink) {
    if (!termios_ptr) {
        return -1;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->set(fd, action, xsink);
}

void QoreTermIOS::set_lflag(unsigned int val) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    static_cast<QoreTermIOSImpl*>(termios_ptr)->set_lflag(val);
}

void QoreTermIOS::set_cflag(unsigned int val) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    static_cast<QoreTermIOSImpl*>(termios_ptr)->set_cflag(val);
}

void QoreTermIOS::set_oflag(unsigned int val) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    static_cast<QoreTermIOSImpl*>(termios_ptr)->set_oflag(val);
}

void QoreTermIOS::set_iflag(unsigned int val) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    static_cast<QoreTermIOSImpl*>(termios_ptr)->set_iflag(val);
}

unsigned int QoreTermIOS::get_lflag() const {
    if (!termios_ptr) {
        return 0;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get_lflag();
}

unsigned int QoreTermIOS::get_cflag() const {
    if (!termios_ptr) {
        return 0;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get_cflag();
}

unsigned int QoreTermIOS::get_oflag() const {
    if (!termios_ptr) {
        return 0;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get_oflag();
}

unsigned int QoreTermIOS::get_iflag() const {
    if (!termios_ptr) {
        return 0;
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get_iflag();
}

unsigned char QoreTermIOS::get_cc(int64 offset, ExceptionSink *xsink) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->get_cc(offset, xsink);
}

int QoreTermIOS::set_cc(int64 offset, unsigned char val, ExceptionSink *xsink) {
    if (!termios_ptr) {
        termios_ptr = new QoreTermIOSImpl();
    }
    return static_cast<QoreTermIOSImpl*>(termios_ptr)->set_cc(offset, val, xsink);
}

int QoreTermIOS::getWindowSize(int &rows, int &columns, ExceptionSink *xsink) {
    return QoreTermIOSImpl::getWindowSize(rows, columns, xsink);
}

#endif // HAVE_TERMIOS_H
