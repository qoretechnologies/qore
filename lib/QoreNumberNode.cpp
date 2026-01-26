/* -*- indent-tabs-mode: nil -*- */
/*
    QoreNumberNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/qore_string_private.h"
#include "qore/intern/qore_number_private.h"

void qore_number_private::getAsString(QoreString& str, bool round, int base) const {
    // first check for zero
    if (zero()) {
        str.concat("0");
        return;
    }

    mpfr_exp_t exp;

    char* buf = mpfr_get_str(0, &exp, base, 0, num, QORE_MPFR_RND);
    if (!buf) {
        numError(str);
        return;
    }
    ON_BLOCK_EXIT(mpfr_free_str, buf);

    //printd(5, "qore_number_private::getAsString(round: %d) this: %p buf: '%s'\n", round, this, buf);

    // if it's a regular number, then format accordingly
    if (number()) {
        int sgn = sign();
        size_t len = str.size() + (sgn < 0 ? 1 : 0);
        //printd(5, "qore_number_private::getAsString() this: %p '%s' exp " QLLD " len: " QLLD "\n", this, buf, exp, len);

        size_t dp = 0;

        str.concat(buf);
        // trim the trailing zeros off the end
        str.trim_trailing('0');
        if (exp <= 0) {
        exp = -exp;
        str.insert("0.", len);
        dp = len + 1;
        //printd(5, "qore_number_private::getAsString() this: %p str: '%s' exp: " QLLD " dp: " QLLD " len: " QLLD "\n", this, str.c_str(), exp, dp, len);
        if (exp)
            str.insertch('0', len + 2, exp);
        } else {
            // get remaining length of string (how many characters were added)
            size_t rlen = str.size() - len;

            //printd(5, "qore_number_private::getAsString() this: %p str: '%s' exp: " QLLD " rlen: " QLLD "\n", this, str.c_str(), exp, rlen);

            // assert that we have added at least 1 character
            assert(rlen > 0);
            if ((size_t)exp > rlen)
                str.insertch('0', str.size(), exp - rlen);
            else if ((size_t)exp < rlen) {
                str.insertch('.', len + exp, 1);
                dp = len + exp;
            }
        }
        // try to do some rounding (noise reduction with binary->decimal conversions)
        if (dp && round)
            applyRoundingHeuristic(str, dp, str.size());
    } else
        str.concat(buf);

    //printd(5, "qore_number_private::getAsString() this: %p returning '%s'\n", this, str.c_str());
}

void qore_number_private::applyRoundingHeuristic(QoreString& str, size_t dp, size_t last, int round_threshold_1,
        int round_threshold_2) {
    // the position of the last significant digit
    qore_offset_t pos = (qore_offset_t)dp;
    size_t i = dp;
    // the last digit found in the sequence
    char lc = 0;
    // 0 or 9 count
    unsigned cnt = 0;
    ssize_t has_e = str.find('e');
    // don't check the last character
    --last;
    // check all except the last digit
    while (i < last) {
        char c = str[i++];
        if (c == '0' || c == '9') {
            // continue the sequence
            if (c == lc) {
                ++cnt;
                continue;
            }

            // check for 2nd threshold
            if (cnt > (unsigned)round_threshold_2) {
                break;
            }

            // set last digit to digit found
            lc = c;
        } else {
            // check for 2nd threshold
            if (cnt > (unsigned)round_threshold_2) {
                break;
            }
            // no 0 or 9 digit found
            lc = 0;
        }

        // mark position of the last significant digit
        pos = i - 2;
        //printd(5, "qore_number_private::applyRoundingHeuristic('%s') set pos: %lld ('%c') dp: %lld\n", str.c_str(),
        //    pos, str[pos], dp);

        // reset count
        cnt = 0;
    }

    // round the number for display
    if (cnt > (unsigned)round_threshold_1) {
        //printd(5, "ROUND BEFORE: (pos: %d dp: %d cnt: %d has_e: %d lc: '%c') '%s'\n", pos, dp, cnt, (int)has_e, lc,
        //    str.c_str());
        // if rounding right after the decimal point, then remove the decimal point
        if (pos == (qore_offset_t)dp) {
            --pos;
        }
        // remove the excess digits
        if (has_e == -1) {
            str.terminate(pos + 1);
        } else {
            str.replace(pos + 1, has_e - pos - 1, nullptr);
            //printd(5, "qore_number_private::applyRoundingHeuristic() after replace str: '%s'\n", str.c_str());
        }

        // rounding down is easy; the truncation is enough
        if (lc == '9') {
            // round up
            roundUp(str, pos);
        }
        //printd(5, "ROUND AFTER: %s\n", str.c_str());
    }
}

int qore_number_private::roundUp(QoreString& str, qore_offset_t pos) {
    for (; pos >= 0; --pos) {
        char c = str[pos];
        if (c == '.') {
            continue;
        }
        if (!pos && c == '-') {
            break;
        }
        if (c < '9') {
            str.replaceChar(pos, c + 1);
            break;
        }
        str.replaceChar(pos, '0');
    }
    //printd(5, "qore_number_private::roundUp() str: '%s' pos: %lld\n", str.c_str(), pos);
    if (pos == -1 || (!pos && str[0] == '-')) {
        str.insertch('1', pos + 1, 1);
        //printd(5, "qore_number_private::roundUp() after insertch(): str: '%s'\n", str.c_str());
        return 1;
    }
    return 0;
}

int qore_number_private::formatNumberString(QoreString& num, const QoreString& fmt, ExceptionSink* xsink) {
    assert(!num.empty());
    assert(num.getEncoding() == fmt.getEncoding());
    // get the length of the format string in characters (not bytes)
    size_t fl = fmt.length();
    if (fmt.empty()) {
        printd(5, "qore_number_private::formatNumberString() invalid format string: '%s' for number: '%s'\n",
            fmt.c_str(), num.c_str());
        return 0;
    }

    // get thousands separator character
    QoreString tsep;
    if (tsep.concat(fmt, 0, 1, xsink)) {
        return -1;
    }
    if (tsep == "?") {
        tsep.clear();
    }

    // decimal separator
    QoreString dsep;
    // number of digits after the decimal separator
    int prec = 0;
    if (fl > 2) {
        if (dsep.concat(fmt, 1, 1, xsink)) {
            return -1;
        }
        // get byte offset of start of decimal precision number
        qore_offset_t i = fmt.getByteOffset(2, xsink);
        if (*xsink) {
            return -1;
        }
        assert(i >= 2);
        prec = atoi(fmt.c_str() + i);
    }

    return formatNumberStringIntern(num, prec, dsep, tsep, xsink);
}

int qore_number_private::formatNumberString(QoreString& num, int prec, const QoreString& dsep_str,
        const QoreString& tsep_str, ExceptionSink* xsink) {
    assert(!num.empty());
    TempEncodingHelper dsep(dsep_str, num.getEncoding(), xsink);
    if (*xsink)
        return -1;
    TempEncodingHelper tsep(tsep_str, num.getEncoding(), xsink);
    if (*xsink)
        return -1;

    return formatNumberStringIntern(num, prec, **dsep, **tsep, xsink);
}

int qore_number_private::doRound(QoreString& num, qore_offset_t& dp, int prec) {
    // must round before the decimal point - get the position of the number to round
    int dt = dp + prec;
    bool roundup = (num[dt] > '4');
    //printd(5, "qore_number_private::doRound() num: '%s' dp: " QLLD " prec: %d roundup: %d\n", num.c_str(), dp, prec,
    //  roundup);
    // if the position is not in the string, then return 0
    if (dt < 0 || (!roundup && !dt)) {
        num.set("0", num.getEncoding());
        return -1;
    }
    num.terminate(dp);
    for (int i = dt; i < dp; ++i)
        const_cast<char*>(num.c_str())[i] = '0';
    if (roundup && roundUp(num, dt - 1))
        ++dp;
    return 0;
}

int qore_number_private::formatNumberStringIntern(QoreString& num, int prec, const QoreString& dsep,
        const QoreString& tsep, ExceptionSink* xsink) {
    //printd(5, "qore_number_private::formatNumberStringIntern() num: '%s' prec: %d dsep: '%s' tsep: '%s'\n",
    //  num.c_str(), prec, dsep.c_str(), tsep.c_str());
    // non-zero flag: if any digits are non-zero
    bool nonzero = false;

    //printd(5, "qore_number_private::formatNumberString() tsep: '%s' dsep: '%s' prec: %d '%s'\n", tsep.c_str(),
    //  dsep.c_str(), prec, num.c_str());

    // start of digits before the decimal point
    qore_offset_t ds = num[0] == '-' ? 1 : 0;

    // find decimal point
    qore_offset_t dp = num.find('.');
    if (dp != -1) {
        // how many digits do we have now after the decimal point
        size_t d = num.strlen() - dp - 1;
        assert(d);
        if (prec >= 0) {
            if ((int)d < prec)
                num.addch('0', prec - d);
            else if ((int)d > prec) {
                if ((num[dp + prec + 1] > '4') && (roundUp(num, dp + prec)))
                    ++dp;
                if (!prec)
                    num.terminate(dp);
                else
                    num.terminate(dp + prec + 1);
            }
        }

        // scan for non-zero digits if negative
        if (ds) {
            for (const char* c = num.c_str(); *c; ++c) {
                if (*c > '0' && *c <= '9') {
                    nonzero = true;
                    break;
                }
            }
        }

        // trim trailing zeros if only significant digits should be included or the precision is negative
        if (prec < 0) {
            bool removed_decimal = false;

            num.trim_trailing('0');
            size_t len = num.size();
            if (num[len - 1] == '.') {
                num.terminate(len - 1);
                removed_decimal = true;
            }

            // perform pre-decimal rounding if necessary
            if (prec != QORE_NUM_ALL_DIGITS && doRound(num, dp, prec))
                return 0;

            if (removed_decimal)
                prec = 0;
        }
        // now substitute decimal point if necessary
        else if (prec > 0 && (dsep.strlen() != 1 || dsep[0] != '.'))
            num.replace(dp, 1, dsep.c_str());
    } else {
        dp = num.size();
        if ((prec < 0) && (prec != QORE_NUM_ALL_DIGITS) && doRound(num, dp, prec))
            return 0;

        // scan for non-zero digits if negative
        if (ds) {
            for (const char* c = num.c_str(); *c; ++c) {
                if (*c > '0' && *c <= '9') {
                    nonzero = true;
                    break;
                }
            }
        }

        if (prec > 0) {
            // add decimal point
            qore_string_private::get(num)->concat(&dsep);
            // add zeros for significant digits
            num.addch('0', prec);
        }
    }

    if (!tsep.empty()) {
        // now insert thousands separator
        // work backwards from the decimal point
        qore_offset_t i = dp - 3;
        while (i > ds) {
            num.replace(i, 0, tsep.c_str());
            i -= 3;
        }
    }

    //printd(0, "qore_number_private::formatNumberString() ok '%s'\n", num.c_str());

    // remove minus sign if negative -0(.0*)
    if (ds && !nonzero) {
        num.trim_leading('-');
    }

    //assert(false); xxx
    return 0;
}

QoreNumberNode::QoreNumberNode(struct qore_number_private* p) : SimpleValueQoreNode(NT_NUMBER), priv(p) {
}

QoreNumberNode::QoreNumberNode(const QoreValue n) : SimpleValueQoreNode(NT_NUMBER), priv(nullptr) {
    qore_type_t t = n.getType();
    if (t == NT_NUMBER) {
        priv = new qore_number_private(*(n.get<const QoreNumberNode>()->priv));
        return;
    }

    if (t == NT_STRING) {
        priv = new qore_number_private(n.get<const QoreStringNode>()->c_str());
        return;
    }

    if (t == NT_INT) {
        priv = new qore_number_private(n.getAsBigInt());
        return;
    }

    if (t != NT_BOOLEAN
        && t != NT_DATE
        && t != NT_NULL
        && t != NT_FLOAT) {
        priv = new qore_number_private(0ll);
        return;
    }

    priv = new qore_number_private(n.getAsFloat());
}

QoreNumberNode::QoreNumberNode(double f) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(f)) {
}

QoreNumberNode::QoreNumberNode(int64 i) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(i)) {
}

QoreNumberNode::QoreNumberNode(const char* str) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(str)) {
}

QoreNumberNode::QoreNumberNode(const char* str, unsigned prec) : SimpleValueQoreNode(NT_NUMBER),
        priv(new qore_number_private(str, prec)) {
}

QoreNumberNode::QoreNumberNode() : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(0ll)) {
}

QoreNumberNode::QoreNumberNode(const QoreNumberNode& old) : SimpleValueQoreNode(old),
        priv(new qore_number_private(*old.priv)) {
}

QoreNumberNode::~QoreNumberNode() {
   delete priv;
}

// get the value of the type in a string context (default implementation = del = false and returns NullString)
// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
// use the QoreStringValueHelper class (defined in QoreStringNode.h) instead of using this function directly
QoreString* QoreNumberNode::getStringRepresentation(bool& del) const {
    del = true;
    QoreString* str = new QoreString;
    priv->getAsString(*str);
    return str;
}

// concatenate string representation to a QoreString (no action for complex types = default implementation)
void QoreNumberNode::getStringRepresentation(QoreString& str) const {
    priv->getAsString(str);
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
DateTime *QoreNumberNode::getDateTimeRepresentation(bool& del) const {
    del = true;
    double f = priv->getAsFloat();
    return DateTime::makeAbsoluteLocal(currentTZ(), (int64)f, (int)((f - (float)((int)f)) * 1000000));
}

// assign date representation to a DateTime (no action for complex types = default implementation)
void QoreNumberNode::getDateTimeRepresentation(DateTime& dt) const {
    double f = priv->getAsFloat();
    dt.setLocalDate(currentTZ(), (int64)f, (int)((f - (float)((int)f)) * 1000000));
}

bool QoreNumberNode::getAsBoolImpl() const {
    return priv->getAsBool();
}

int QoreNumberNode::getAsIntImpl() const {
    return priv->getAsBigInt();
}

int64 QoreNumberNode::getAsBigIntImpl() const {
    return priv->getAsBigInt();
}

double QoreNumberNode::getAsFloatImpl() const {
    return priv->getAsFloat();
}

int QoreNumberNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    getStringRepresentation(str);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreNumberNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    return getStringRepresentation(del);
}

AbstractQoreNode* QoreNumberNode::realCopy() const {
    return new QoreNumberNode(*this);
}

// the type passed must always be equal to the current type
bool QoreNumberNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    if (v->getType() == NT_NUMBER)
        return equals(*reinterpret_cast<const QoreNumberNode*>(v));

    return equals(v->getAsFloat());
}

bool QoreNumberNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    if (v->getType() != NT_NUMBER)
        return false;
    const QoreNumberNode* n = reinterpret_cast<const QoreNumberNode*>(v);
    return equals(*n);
}

// returns the type name as a c string
const char* QoreNumberNode::getTypeName() const {
    return getStaticTypeName();
}

int QoreNumberNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = numberTypeInfo;
    return 0;
}

bool QoreNumberNode::zero() const {
    return priv->zero();
}

int QoreNumberNode::sign() const {
    return priv->sign();
}

// add the argument to this value and return the result
QoreNumberNode* QoreNumberNode::doPlus(const QoreNumberNode& right) const {
    return new QoreNumberNode(priv->doPlus(*right.priv));
}

// subtract the argument from this value and return the result
QoreNumberNode* QoreNumberNode::doMinus(const QoreNumberNode& n) const {
    return new QoreNumberNode(priv->doMinus(*n.priv));
}

// multiply the argument to this value and return the result
QoreNumberNode* QoreNumberNode::doMultiply(const QoreNumberNode& n) const {
    return new QoreNumberNode(priv->doMultiply(*n.priv));
}

// add the argument to this value and return the result (can throw a division-by-zero exception)
QoreNumberNode* QoreNumberNode::doDivideBy(const QoreNumberNode& n, ExceptionSink* xsink) const {
    qore_number_private* p = priv->doDivideBy(*n.priv, xsink);
    return p ? new QoreNumberNode(p) : 0;
}

// divide this value by the argument and return the result (can throw a division-by-zero exception)
QoreNumberNode* QoreNumberNode::doDivideBy(double d, ExceptionSink* xsink) const {
    qore_number_private n(d);
    qore_number_private* p = priv->doDivideBy(n, xsink);
    return p ? new QoreNumberNode(p) : 0;
}

// divide this value by the argument and return the result (can throw a division-by-zero exception)
QoreNumberNode* QoreNumberNode::doDivideBy(int64 i, ExceptionSink* xsink) const {
    qore_number_private n(i);
    qore_number_private* p = priv->doDivideBy(n, xsink);
    return p ? new QoreNumberNode(p) : 0;
}

QoreNumberNode* QoreNumberNode::negate() const {
    return new QoreNumberNode(priv->negate());
}

bool QoreNumberNode::lessThan(const QoreNumberNode& n) const {
    return priv->lessThan(*n.priv);
}

bool QoreNumberNode::lessThan(double n) const {
    return priv->lessThan(n);
}

bool QoreNumberNode::lessThan(int64 n) const {
    return priv->lessThan(n);
}

bool QoreNumberNode::lessThanOrEqual(const QoreNumberNode& n) const {
    return priv->lessThanOrEqual(*n.priv);
}

bool QoreNumberNode::lessThanOrEqual(double n) const {
    return priv->lessThanOrEqual(n);
}

bool QoreNumberNode::lessThanOrEqual(int64 n) const {
    return priv->lessThanOrEqual(n);
}

bool QoreNumberNode::greaterThan(const QoreNumberNode& n) const {
    return priv->greaterThan(*n.priv);
}

bool QoreNumberNode::greaterThan(double n) const {
    return priv->greaterThan(n);
}

bool QoreNumberNode::greaterThan(int64 n) const {
    return priv->greaterThan(n);
}

bool QoreNumberNode::greaterThanOrEqual(const QoreNumberNode& n) const {
    return priv->greaterThanOrEqual(*n.priv);
}

bool QoreNumberNode::greaterThanOrEqual(double n) const {
    return priv->greaterThanOrEqual(n);
}

bool QoreNumberNode::greaterThanOrEqual(int64 n) const {
    return priv->greaterThanOrEqual(n);
}

bool QoreNumberNode::equals(const QoreNumberNode& n) const {
    return priv->equals(*n.priv);
}

bool QoreNumberNode::equals(double n) const {
    return priv->equals(n);
}

bool QoreNumberNode::equals(int64 n) const {
    return priv->equals(n);
}

QoreNumberNode* QoreNumberNode::numberRefSelf() const {
    ref();
    return const_cast<QoreNumberNode*>(this);
}

void QoreNumberNode::toString(QoreString& str, int fmt) const {
    priv->toString(str, fmt);
}

unsigned QoreNumberNode::getPrec() const {
    return priv->getPrec();
}

QoreNumberNode* QoreNumberNode::toNumber(const QoreValue n) {
    qore_type_t t = n.getType();

    if (t == NT_NUMBER)
        return n.get<const QoreNumberNode>()->numberRefSelf();

    if (t == NT_FLOAT)
        return new QoreNumberNode(n.getAsFloat());

    if (t == NT_STRING)
        return new QoreNumberNode(n.get<const QoreStringNode>()->c_str());

    if (t == NT_INT)
        return new QoreNumberNode(n.getAsBigInt());

    return new QoreNumberNode(n.getAsFloat());
}

QoreNumberNode* QoreNumberNode::toNumber(const AbstractQoreNode* n) {
    QoreValue v(n);
    return toNumber(v);
}

bool QoreNumberNode::nan() const {
    return priv->nan();
}

bool QoreNumberNode::inf() const {
    return priv->inf();
}

bool QoreNumberNode::ordinary() const {
    return priv->number();
}

QoreNumberNodeHelper::QoreNumberNodeHelper(const QoreValue n) {
    if (n.hasNode() && get_node_type(n.getInternalNode()) == NT_NUMBER) {
        del = false;
        num = n.get<const QoreNumberNode>();
    } else {
        del = true;
        num = new QoreNumberNode(n);
    }
}

QoreNumberNodeHelper::~QoreNumberNodeHelper() {
    if (del)
        const_cast<QoreNumberNode*>(num)->deref();
}

QoreNumberNode* QoreNumberNodeHelper::getReferencedValue() {
    auto rv = const_cast<QoreNumberNode*>(num);
    num = 0;
    del = false;
    if (!del)
        rv->ref();
    return rv;
}
