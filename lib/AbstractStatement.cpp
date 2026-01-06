/*
    AbstractStatement.cpp

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

#include <qore/Qore.h>
#include "qore/intern/AbstractStatement.h"
#include "qore/intern/qore_program_private.h"

#include <typeinfo>

QoreBreakpointList_t::QoreBreakpointList_t() {
}

QoreBreakpointList_t::~QoreBreakpointList_t() {
    for (auto& i : *this) {
        i->deref();
    }
    clear();
}

AbstractStatement::AbstractStatement(qore_program_private_base* p) : loc(p->getLocation(-1, -1)) {
    pwo = p->pwo;
}

AbstractStatement::AbstractStatement(int sline, int eline)
        : loc(qore_program_private::get(*getProgram())->getLocation(sline, eline)) {
    QoreProgram* pgm = getProgram();
    assert(pgm);
    pwo = qore_program_private::getParseWarnOptions(pgm);
}

AbstractStatement::AbstractStatement(const QoreProgramLocation* loc) : loc(loc) {
    QoreProgram* pgm = getProgram();
    assert(pgm);
    pwo = qore_program_private::getParseWarnOptions(pgm);
}

AbstractStatement::~AbstractStatement() {
    if (breakpoints) {
        // unassign all breakpoint, it is probably not needed because statement are deleted with program
        std::list<QoreBreakpoint*>::iterator it = breakpoints->begin();
        while (it != breakpoints->end()) {
            QoreBreakpoint* bkpt = *it;
            it++;  // will be removed by unassignBreakpoint()
            bkpt->statementList.remove(this);
        }
        delete breakpoints;
    }
}

void AbstractStatement::finalizeBlock(int sline, int eline) {
    QoreProgramLocation tl(sline, eline);
    if (tl.getFile() == loc->getFile()
        && tl.getSource() == loc->getSource()
        && (sline != loc->start_line || eline != loc->end_line)) {
        loc = qore_program_private::get(*getProgram())->getLocation(*loc, sline, eline);
    }
}

int AbstractStatement::exec(QoreValue& return_value, ExceptionSink *xsink) {
    //QORE_TRACE("AbstractStatement::exec()");
    printd(1, "AbstractStatement::exec() this: %p file: %s:%d\n", this, loc->getFile(), loc->start_line);
    QoreProgramLocationHelper stack_loc(xsink, loc, this, pwo.parse_options);
    //pthread_testcancel();
    if (*xsink) {
        return 0;
    }
    return execImpl(return_value, xsink);
}

int AbstractStatement::parseInit(QoreParseContext& parse_context) {
    printd(2, "AbstractStatement::parseInit() this: %p type: %s file: %s line: %d\n", this, typeid(this).name(),
        loc->getFile(), loc->start_line);
    // set parse options and warning mask for this statement
    ParseWarnHelper pwh(pwo);

    return parseInitImpl(parse_context);
}

QoreBreakpoint* AbstractStatement::getBreakpoint() const {
    if (breakpointFlag) {
        for (std::list<QoreBreakpoint*>::iterator it = breakpoints->begin(); it != breakpoints->end(); ++it) {
            if ((*it)->checkBreak()) {
                return *it;
            }
        }
    }
    return 0;
}

void AbstractStatement::assignBreakpoint(QoreBreakpoint *bkpt) {
    if (!breakpoints) {
        breakpoints = new QoreBreakpointList_t();
        breakpoints->push_front(bkpt);
    } else {
        if (std::find(breakpoints->begin(), breakpoints->end(), bkpt) == breakpoints->end()) {
            breakpoints->push_front(bkpt);
        }
    }
    breakpointFlag = !breakpoints->empty();
}

void AbstractStatement::unassignBreakpoint(QoreBreakpoint *bkpt) {
    if (breakpoints) {
        breakpoints->remove(bkpt);
        breakpointFlag = !breakpoints->empty();
    }
}

void AbstractStatement::parseCommit(QoreProgram* pgm) {
    qore_program_private::registerStatement(pgm, this, true);
}
