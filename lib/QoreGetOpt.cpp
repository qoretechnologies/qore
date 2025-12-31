/*
    QoreGetOpt.cpp

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
#include "qore/intern/QoreGetOpt.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

QoreGetOpt::~QoreGetOpt() {
   getopt_node_list_t::iterator i;
   while ((i = node_list.begin()) != node_list.end()) {
      QoreGetOptNode* n = *i;
      node_list.erase(i);
      delete n;
   }
   long_map.clear();
   short_map.clear();
}

QoreGetOptNode* QoreGetOpt::find(const char* opt) const {
   getopt_long_map_t::const_iterator i = long_map.find(opt);
   if (i != long_map.end())
      return i->second;

   return 0;
}

QoreGetOptNode* QoreGetOpt::find(char opt) const {
   getopt_short_map_t::const_iterator i = short_map.find(opt);
   if (i != short_map.end())
      return i->second;
   return 0;
}

int QoreGetOpt::add(const char* name, char short_opt, const char* long_opt, qore_type_t argtype, int option) {
   // check input for validity
   if (!name || !name[0])
      return QGO_ERR_NO_NAME;
   if (!short_opt && (!long_opt || !long_opt[0]))
      return QGO_ERR_NO_OPTION;

   //printf("QoreGetOpt::add(name: '%s', short: %03d ('%c'), long: '%s', argtype: %d, opt: %d)\n", name, short_opt, short_opt ? short_opt : '-', long_opt ? long_opt : "n/a", argtype, option);
   // look for duplicate entries
   if (short_opt && find(short_opt))
      return QGO_ERR_DUP_SHORT_OPT;
   if (long_opt && find(long_opt))
      return QGO_ERR_DUP_LONG_OPT;

   QoreGetOptNode* n = new QoreGetOptNode(name, short_opt, long_opt, argtype, option);
   if (short_opt)
      short_map[short_opt] = n;
   if (long_opt)
      long_map[n->long_opt.c_str()] = n;
   node_list.push_back(n);

   return 0;
}

static void addError(QoreHashNode* h, QoreStringNode* err) {
    //printd(5, "addError() adding: %s\n", err->getBuffer())
    hash_assignment_priv ha(*h, "_ERRORS_");
    QoreListNode* l = (*ha).get<QoreListNode>();
    if (!l) {
        l = new QoreListNode(autoTypeInfo);
        ha.assign(l, 0);
    }

    l->push(err, nullptr);
}

// private, static method
DateTimeNode* QoreGetOpt::parseDate(const char* val) {
   return new DateTimeNode(val);
}

void QoreGetOpt::doOption(class QoreGetOptNode* n, class QoreHashNode* h, const char* val) {
    hash_assignment_priv ha(*h, n->name);

    // get a value ready
    if (n->argtype == -1) {
        if (!(*ha).isNothing())
            return;
        ha.assign(true, 0);
        return;
    }

    // handle option values
    if (!val) {
        if (n->option & QGO_OPT_ADDITIVE) {
            if (n->argtype == NT_INT) {
                if ((*ha).isNothing()) {
                    ha.assign(1, 0);
                }
                else {
                    ha.assign((*ha).getAsBigInt() + 1, nullptr);
                }
            }
            else {
                if ((*ha).isNothing()) {
                    ha.assign(0.0, 0);
                }
                else {
                    ha.assign((*ha).getAsFloat() + 1, nullptr);
                }
            }
        }
        else if ((*ha).isNothing()) {
            ha.assign(true, nullptr);
        }
        return;
    }

    QoreValue v{};
    if (n->argtype == NT_STRING)
        v = new QoreStringNode(val);
    else if (n->argtype == NT_INT)
        v = strtoll(val, 0, 10);
    else if (n->argtype == NT_FLOAT)
        v = QoreValue(q_strtod(val));
    else if (n->argtype == NT_DATE)
        v = parseDate(val);
    else if (n->argtype == NT_BOOLEAN)
        v = (bool)strtol(val, 0, 10);
    else // default string
        v = new QoreStringNode(val);

    if (!(n->option & QGO_OPT_LIST_OR_ADD)) {
        ha.assign(v, nullptr);
        return;
    }

    if (n->option & QGO_OPT_LIST) {
        QoreListNode* l = (*ha).get<QoreListNode>();
        if (!l) {
            l = new QoreListNode(autoTypeInfo);
            ha.assign(l, nullptr);
        }
        //else printf("cv->getType()=%s\n", cv->getTypeName());
        l->push(v, nullptr);
        return;
    }

    // additive
    if (!(*ha).isNothing()) {
        if (n->argtype == NT_INT) {
            ha.om->val.set(ha.om->val.getAsBigInt() + v.getAsBigInt());
        }
        else { // float
            ha.om->val.set(ha.om->val.getAsFloat() + v.getAsFloat());
        }
        v.discard(nullptr);
        return;
    }

    ha.assign(v, nullptr);
}

char* QoreGetOpt::getNextArgument(QoreListNode* l, QoreHashNode* h, unsigned& i, const char* lopt, char sopt) {
    if (i < (l->size() - 1)) {
        i++;
        QoreValue v = l->retrieveEntry(i);
        QoreStringNode* n = v.getType() == NT_STRING ? v.get<QoreStringNode>() : nullptr;
        if (n) {
            return (char*)n->c_str();
        }
    }
    QoreStringNode* err = new QoreStringNode;
    if (lopt) {
        err->sprintf("long option '--%s' requires an argument", lopt);
    }
    else {
        err->sprintf("short option '-%c' requires an argument", sopt);
    }
    addError(h, err);
    return nullptr;
}

void QoreGetOpt::processLongArg(const char* arg, QoreListNode* l, class QoreHashNode* h, unsigned &i, bool modify) {
    const char* opt;
    char* val;

    // get a copy of the argument
    QoreString vstr(arg);
    arg = vstr.getBuffer();

    // see if there is an assignment character
    char* tok = (char* )strchr(arg, '=');
    if (tok) {
        (*tok) = '\0';
        opt = arg;
        val = tok + 1;
    }
    else {
        opt = arg;
        val = 0;
    }
    // find option
    QoreGetOptNode* w = find(opt);
    if (!w) {
        QoreStringNode* err = new QoreStringNodeMaker("unknown long option '--%s'", opt);

        addError(h, err);
        return;
    }
    bool do_modify = false;
    // if we need a value and there isn't one, then try to get the next argument in the list
    if (w->argtype && !val && (w->option & QGO_OPT_MANDATORY)) {
        val = (char* )getNextArgument(l, h, i, opt, '\0');
        if (val && modify)
            do_modify = true;
        if (!val)
            return;
    }
    doOption(w, h, val);
    if (do_modify) {
        qore_list_private::get(*l)->spliceSingle(--i).discard(nullptr);
    }
}

int QoreGetOpt::processShortArg(const char* arg, QoreListNode* l, class QoreHashNode* h, unsigned &i, int &j, bool modify) {
    char opt = (arg + j)[0];
    // find option
    QoreGetOptNode* w = find(opt);
    if (!w) {
        QoreStringNode* err = new QoreStringNodeMaker("unknown short option '-%c'", opt);
        addError(h, err);
        return 0;
    }
    bool do_modify = false;
    const char* val = 0;
    if (w->argtype != -1) {
        if ((j < (signed)(strlen(arg) - 1))
            && ((w->option & QGO_OPT_MANDATORY) || ((arg + j + 1)[0] == '='))) {
            val = arg + j + 1;
            if (*val == '=')
                val++;
            j = 0;
        }
        else if (w->option & QGO_OPT_MANDATORY) {
            if (!(val = getNextArgument(l, h, i, 0, opt)))
                return 0;
            if (modify)
                do_modify = true;
        }
    }
    doOption(w, h, val);
    if (do_modify) {
        qore_list_private::get(*l)->spliceSingle(--i).discard(nullptr);
    }
    //printd(5, "processShortArg(%c) val=%p %s returning %d\n", opt, val, val, !j);
    return !j;
}

QoreHashNode* QoreGetOpt::parse(QoreListNode* l, bool modify, ExceptionSink *xsink) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);

    for (unsigned i = 0; i < l->size(); ++i) {
        //printf("QoreGetOpt::parse() %d/%d\n", i, l->size());
        QoreValue n = l->retrieveEntry(i);
        if (n.getType() != NT_STRING)
            continue;

        QoreStringNode* str = n.get<QoreStringNode>();
        const char* arg = str->c_str();

        if (arg[0] == '-') {
            if (!arg[1])
                continue;
            if (arg[1] == '-') {
                if (!arg[2])
                    break;
                processLongArg(arg + 2, l, h, i, modify);
                if (modify) {
                    //printd(5, "parse() opt=%s size=%d\n", arg, l->size());
                    qore_list_private::get(*l)->spliceSingle(i--).discard(xsink);
                    //printd(5, "parse() popped entry, size=%d\n", l->size());
                }
                continue;
            }
            int len = strlen(arg);
            for (int j = 1; j < len; j++)
                if (processShortArg(arg, l, h, i, j, modify))
                    break;
            if (modify) {
                qore_list_private::get(*l)->spliceSingle(i--).discard(xsink);
            }
        }
    }
    //printd(5, "QoreGetOpt::parse() returning h=%p (size %d)\n", h, h->size());
    return h;
}
