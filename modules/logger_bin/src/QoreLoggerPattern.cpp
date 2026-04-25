/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerPattern.cpp LoggerPattern class definition */
/*
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

#include "qore_logger.h"
#include "QoreLoggerPattern.h"
#include "QoreLoggerLayoutPattern.h"
#include "QC_LoggerEvent.h"

#include <regex>

QoreRegexInterface QoreLoggerPattern::te(nullptr, TOKEN_EXTRACT_PATTERN, QRE_CASELESS);
QoreRegexInterface QoreLoggerPattern::t1(nullptr, TOKEN_PATTERN_1, QRE_CASELESS);
QoreRegexInterface QoreLoggerPattern::t2(nullptr, TOKEN_PATTERN_2, QRE_CASELESS);
QoreRegexInterface QoreLoggerPattern::t3(nullptr, TOKEN_PATTERN_3, QRE_CASELESS);
QoreRegexInterface QoreLoggerPattern::t4(nullptr, TOKEN_PATTERN_4, QRE_CASELESS);
QoreRegexSubstInterface QoreLoggerPattern::t5(nullptr, TOKEN_PATTERN_5, QRE_CASELESS);

int QoreLoggerPattern::setPattern(const QoreStringNode* pattern, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> value(pattern->convertEncoding(QCS_DEFAULT, xsink));
    if (*xsink) {
        return -1;
    }
    ReferenceHolder<QoreListNode> pp(new QoreListNode(autoTypeInfo), xsink);
    SimpleRefHolder<QoreStringNode> patt(value->copy());

    //printd(5, "setPattern() pattern: '%s' (%d)\n", patt->c_str(), (int)patt->size());

    // Helper: the Qore CoW convention requires that a QoreStringNode with
    // refcount > 1 is cloned before in-place mutation.  Earlier iterations of
    // this loop push QoreStringNodeView slices of `patt` into `pp` (via
    // substr()), which hold refs on `patt` — so by the time we reach the
    // second splice below, `patt` is usually non-unique.  Clone before
    // splicing so the views in `pp` continue to read the pre-splice bytes.
    auto ensure_patt_unique = [&]() {
        if (!patt->is_unique()) {
            patt = patt->copy();
        }
    };

    // list elements may be strings or hashes
    while (!patt->empty()) {
        ssize_t pos = 0;
        while (true) {
            ssize_t pos0 = patt->find(ESCAPE_CHAR, pos);
            if (pos0 < 0) {
                pos = patt->size();
            } else {
                pos = pos0;
                if (patt->c_str()[pos + 1] == ESCAPE_CHAR) {
                    // %% found
                    ensure_patt_unique();
                    patt->splice(pos, 1, xsink);
                    assert(!*xsink);
                    ++pos;
                    if ((size_t)pos < patt->size()) {
                        continue;
                    }
                }
            }
            break;
        }
        if (pos >= (ssize_t)patt->size()) {
            //printd(5, "setPattern() pushing: '%s' (%d)\n", patt->c_str(), (int)patt->size());
            pp->push(patt.release(), xsink);
            assert(!*xsink);
            break;
        }
        if (pos > 0) {
            SimpleRefHolder<QoreStringNode> v(patt->substr(0, pos, xsink));
            assert(!*xsink);
            //printd(5, "setPattern() v: '%s' (%d)\n", v->c_str(), v->size());
            pp->push(v.release(), xsink);
            assert(!*xsink);
        }
        ensure_patt_unique();
        patt->splice(0, pos + 1, xsink);

        std::string pstr(patt->c_str());
        ReferenceHolder<QoreListNode> l(te.extractSubstrings(**patt, xsink), xsink);
        if (*xsink) {
            return -1;
        }

        if (!l || l->size() < 2) {
            xsink->raiseException("LOGGER-ERROR", new QoreStringNodeMaker("Invalid logger pattern starting at %%%s",
                patt->c_str()));
            return -1;
        }

        //printd(5, "setPattern() token '%s' matches: %d\n", patt ? patt->c_str() : "", (int)l->size());

        ReferenceHolder<QoreHashNode> f(new QoreHashNode(autoTypeInfo), xsink);
        const QoreValue l0 = l->retrieveEntry(0);
        if (l0.getType() == NT_STRING && !l0.get<const QoreStringNode>()->empty()) {
            const QoreStringNode* l0str = l0.get<const QoreStringNode>();
            // check first pattern
            ReferenceHolder<QoreListNode> opt(t1.extractSubstrings(*l0str, xsink), xsink);
            if (opt && !opt->empty()) {
                assert(opt->size() == 3);
                const QoreValue opt0 = opt->retrieveEntry(0);
                bool lj = opt0.getType() == NT_STRING && *opt0.get<const QoreStringNode>() == "-";
                //printd(5, "getPattern() 0 lj: %d (%s)\n", lj, opt0 ? opt0.get<const QoreStringNode>()->c_str() : "");
                f->setKeyValue("leftJustify", lj, xsink);
                f->setKeyValue("minWidth", opt->retrieveEntry(1).getAsBigInt(), xsink);
                f->setKeyValue("maxWidth", opt->retrieveEntry(2).getAsBigInt(), xsink);
            } else {
                opt = t2.extractSubstrings(*l0str, xsink);
                if (opt && !opt->empty()) {
                    assert(opt->size() == 2);
                    const QoreValue opt0 = opt->retrieveEntry(0);
                    bool lj = opt0.getType() == NT_STRING && *opt0.get<const QoreStringNode>() == "-";
                    //printd(5, "getPattern() 1 lj: %d (%s)\n", lj, opt0.getType() == NT_STRING ?
                    //    opt0.get<const QoreStringNode>()->c_str() : "");
                    f->setKeyValue("leftJustify", lj, xsink);
                    f->setKeyValue("minWidth", opt->retrieveEntry(1).getAsBigInt(), xsink);
                } else {
                    opt = t3.extractSubstrings(*l0str, xsink);
                    if (opt && !opt->empty()) {
                        assert(opt->size() == 1);
                        f->setKeyValue("maxWidth", opt->retrieveEntry(0).getAsBigInt(), xsink);
                    } else {
                        xsink->raiseException("LOGGER-ERROR", new QoreStringNodeMaker("Invalid logger pattern "
                            "option starting at %%%s", patt->c_str()));
                        return -1;
                    }
                }
            }
        }
        const QoreValue l1 = l->retrieveEntry(1);
        assert(l1 && l1.getType() == NT_STRING);
        f->setKeyValue("key", l1.refSelf(), xsink);
        if (l->size() == 3) {
            const QoreValue l2 = l->retrieveEntry(2);
            if (l2) {
                assert(l2.getType() == NT_STRING);
                ReferenceHolder<QoreListNode> opt(t4.extractSubstrings(*l2.get<const QoreStringNode>(), xsink),
                    xsink);
                if (opt && !opt->empty()) {
                    QoreValue opt0 = opt->retrieveEntry(0);
                    assert(opt0);
                    assert(opt0.getType() == NT_STRING);
                    f->setKeyValue("option", opt0.refSelf(), xsink);
                    assert(!*xsink);
                } else {
                    f->setKeyValue("option", new QoreStringNode(), xsink);
                    assert(!*xsink);
                }
            }
        }
        patt = t5.subst(**patt, xsink);
        //printd(5, "setPattern() final '%s' (%d)\n", patt->c_str(), (int)patt->size());
        pp->push(f.release(), xsink);
        assert(!*xsink);
    }

    AutoLocker al(m);
    // write new values to object
    if (parsedPattern) {
        parsedPattern->deref(xsink);
    }
    parsedPattern = pp.release();
    origPattern = value.release();

    return 0;
}

QoreStringNode* QoreLoggerPattern::format(const QoreValue data, const QoreLoggerLayoutPattern* llp,
        ExceptionSink* xsink) const {
    SimpleRefHolder<QoreStringNode> res(new QoreStringNode);
    assert(parsedPattern);

    // check if data is a direct instance of LoggerEvent
    QoreObject* event = nullptr;
    ReferenceHolder<QoreLoggerEvent> ev(xsink);
    if (data.getType() == NT_OBJECT) {
        event = const_cast<QoreObject*>(data.get<QoreObject>());
        if (event->validInstanceOf(*QC_LOGGEREVENT)) {
            // only set the private data object if the class is not overridden
            if (QC_LOGGEREVENT->isEqual(*event->getClass())) {
                ev = event->tryGetReferencedPrivateData<QoreLoggerEvent>(CID_LOGGEREVENT, xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
    }

    return format(xsink, llp, data, event, *ev);
}

QoreStringNode* QoreLoggerPattern::format(ExceptionSink* xsink, const QoreLoggerLayoutPattern* llp,
        const QoreValue data, const QoreObject* event, QoreLoggerEvent* ev) const {
    SimpleRefHolder<QoreStringNode> res(new QoreStringNode);
    assert(parsedPattern);

    ConstListIterator i(parsedPattern);
    while (i.next()) {
        const QoreValue a = i.getValue();
        if (a.getType() == NT_STRING) {
            res->concat(a.get<const QoreStringNode>(), xsink);
            if (*xsink) {
                return nullptr;
            }
            continue;
        }
        assert(a.getType() == NT_HASH);
        const QoreHashNode* ah = a.get<const QoreHashNode>();
        const QoreValue key = ah->getKeyValue("key");
        assert(key.getType() == NT_STRING);
        const QoreStringNode* keystr = key.get<const QoreStringNode>();
        const QoreValue option = ah->getKeyValue("option");
        assert(!option || option.getType() == NT_STRING);
        const QoreStringNode* optstr = option ? option.get<const QoreStringNode>() : nullptr;
        ValueHolder val(callResolveField(llp, event, ev, data, keystr, optstr, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        bool fallback = false;
        if (!val) {
            // try one char key if key is longer and has no {}, i.e. fix non-intuitive case
            if (keystr->size() > 1 && !optstr) {
                ValueHolder k(keystr->substr(0, 1, xsink), xsink);
                if (*xsink) {
                    return nullptr;
                }
                assert(k->getType() == NT_STRING);
                val = callResolveField(llp, event, ev, data, k->get<const QoreStringNode>(), nullptr, xsink);
                fallback = (bool)val;
            }
            if (!val) {
                xsink->raiseException("LOGGER-ERROR", new QoreStringNodeMaker("Unknown pattern token \"%s\"",
                    keystr->c_str()));
                return nullptr;
            }
        }
        assert(val->getType() == NT_STRING);
        QoreValue v = ah->getKeyValue("maxWidth");
        if (v) {
            assert(v.getType() == NT_INT);
            val = val->get<const QoreStringNode>()->substr(0, v.getAsBigInt(), xsink);
        }
        v = ah->getKeyValue("minWidth");
        if (v) {
            assert(v.getType() == NT_INT);
            int64 minWidth = v.getAsBigInt();
            size_t i = val->get<const QoreStringNode>()->size();
            if (i < (size_t)minWidth) {
                // ensure unique value
                if (!val->getInternalNode()->is_unique()) {
                    val = val->get<const QoreStringNode>()->copy();
                }
                if (ah->getKeyValue("leftJustify").getAsBool()) {
                    val->get<QoreStringNode>()->insertch(' ', i, minWidth - i);
                } else {
                    val->get<QoreStringNode>()->insertch(' ', 0, minWidth - i);
                }
            }
        }
        res->concat(val->get<const QoreStringNode>(), xsink);
        if (fallback) {
            res->concat(keystr->c_str() + 1, keystr->size() - 1);
        }
    }
    return res.release();
}

QoreValue QoreLoggerPattern::callResolveField(const QoreLoggerLayoutPattern* llp, const QoreObject* event,
        QoreLoggerEvent* ev, const QoreValue& data, const QoreStringNode* key, const QoreStringNode* option,
        ExceptionSink* xsink) const {
    assert(key);
    if (llp && event && ev) {
        return llp->resolveField(const_cast<QoreObject*>(event), ev, key, option, xsink);
    }

    // call resolveField(data, a.key, a.option);
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(data.refSelf(), xsink);
    args->push(key->stringRefSelf(), xsink);
    args->push(option ? option->stringRefSelf() : nullptr, xsink);
    assert(!*xsink);
    return self->evalMethod("resolveField", *args, xsink);
}