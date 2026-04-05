/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_number_private.h

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

#ifndef _QORE_QORE_NUMBER_PRIVATE_H

#define _QORE_QORE_NUMBER_PRIVATE_H

#include <cmath>
#include <memory>
using namespace std;

// the number of consecutive trailing 0 or 9 digits that will be rounded in string output
#define QORE_MPFR_ROUND_THRESHOLD 9
// the number of consecutive trailing 0 or 9 digits that will be rounded in string output if there are 2 trailing non-0/9 digits
#define QORE_MPFR_ROUND_THRESHOLD_2 15

// the magic precesion number that indicates that all decimals should be included in the output string when formatting
#define QORE_NUM_ALL_DIGITS -999999

#define QORE_DEFAULT_PREC 128
#define QORE_MAX_PREC 8192
#ifndef HAVE_MPFR_RNDN
#define MPFR_RNDN GMP_RNDN
#define MPFR_RNDZ GMP_RNDZ
#endif
// round to nearest (roundTiesToEven in IEEE 754-2008)
#define QORE_MPFR_RND MPFR_RNDN
// round toward zero
#define QORE_MPFR_RNDZ MPFR_RNDZ
// MPFR_RNDA

#ifndef HAVE_MPFR_EXP_T
typedef mp_exp_t mpfr_exp_t;
#endif

#ifdef HAVE_MPFR_SPRINTF
#define QORE_MPFR_SPRINTF_ARG 'R'
#else
#define QORE_MPFR_SPRINTF_ARG 'L'
#endif

// some compilers (sun/oracle pro c notably) do not support arrays with a variable size
// if not, we can't use the stack for the temporary variable and have to use a dynamically-allocated one
// also MPFR_DECL_INIT is compiled incorrectly on g 5.2 on Solaris SPARC with all optimization levels
// for some unknown reason (https://github.com/qorelanguage/qore/issues/958)
#if defined(HAVE_LOCAL_VARIADIC_ARRAYS) && !(defined(SPARC) && defined(SOLARIS) && defined(__GNUC__))
#define MPFR_TMP_VAR(x, p) MPFR_DECL_INIT(x, (p))
#else
#define MPFR_TMP_VAR(x, p) qore_number_private tmp_x((mpfr_prec_t)p); mpfr_t& x = tmp_x.num
#endif

// for binary operations on MPFR data
typedef int (*q_mpfr_binary_func_t)(mpfr_t, const mpfr_t, const mpfr_t, mpfr_rnd_t);
// for unary operations on MPFR data
typedef int (*q_mpfr_unary_func_t)(mpfr_t, const mpfr_t, mpfr_rnd_t);
// for unary operations on MPFR data without a rounding argument
typedef int (*q_mpfr_unary_nr_func_t)(mpfr_t, const mpfr_t);

struct qore_number_private_intern {
    mpfr_t num;

    DLLLOCAL qore_number_private_intern() {
        mpfr_init2(num, QORE_DEFAULT_PREC);
    }

    DLLLOCAL qore_number_private_intern(mpfr_prec_t prec) {
        if (prec > QORE_MAX_PREC)
            prec = QORE_MAX_PREC;
        mpfr_init2(num, prec);
    }

    DLLLOCAL ~qore_number_private_intern() {
        mpfr_clear(num);
    }

    DLLLOCAL void checkPrec(q_mpfr_binary_func_t func, const mpfr_t r) {
        mpfr_prec_t prec;
        if (func == mpfr_mul || func == mpfr_div) {
            prec = mpfr_get_prec(num) + mpfr_get_prec(r);
        } else {
            prec = QORE_MAX(mpfr_get_prec(num), mpfr_get_prec(r)) + 1;
        }
        if (prec > mpfr_get_prec(num))
            mpfr_prec_round(num, prec, QORE_MPFR_RND);
    }

    DLLLOCAL void setPrec(mpfr_prec_t prec) {
        if (prec > QORE_MAX_PREC)
            prec = QORE_MAX_PREC;
        mpfr_prec_round(num, prec, QORE_MPFR_RND);
    }

    DLLLOCAL static void do_divide_by_zero(ExceptionSink* xsink) {
        xsink->raiseException("DIVISION-BY-ZERO", "division by zero error in numeric operator");
    }

    DLLLOCAL static void checkFlags(ExceptionSink* xsink) {
#ifdef HAVE_MPFR_DIVBY0
        if (mpfr_divby0_p()) {
            mpfr_clear_divby0();
            do_divide_by_zero(xsink);
        }
#endif
        if (mpfr_erangeflag_p()) {
            mpfr_clear_erangeflag();
            xsink->raiseException("INVALID-NUMERIC-OPERATION", "invalid numeric operation attempted");
        }
    }
};

struct qore_number_private : public qore_number_private_intern {
    DLLLOCAL explicit qore_number_private(mpfr_prec_t prec) : qore_number_private_intern(prec) {
    }

    DLLLOCAL qore_number_private(double f) {
        /* from the MPFR docs: http://www.mpfr.org/mpfr-current/mpfr.html
            Note: If you want to store a floating-point constant to a mpfr_t, you should use mpfr_set_str
            (or one of the MPFR constant functions, such as mpfr_const_pi for Pi) instead of mpfr_set_flt,
            mpfr_set_d, mpfr_set_ld or mpfr_set_decimal64. Otherwise the floating-point constant will be
            first converted into a reduced-precision (e.g., 53-bit) binary (or decimal, for
            mpfr_set_decimal64) number before MPFR can work with it.
        */

        QoreStringMaker str("%.17g", f);
        mpfr_set_str(num, str.getBuffer(), 10, QORE_MPFR_RND);
    }

    DLLLOCAL qore_number_private(int64 i) {
        mpfr_set_sj(num, i, QORE_MPFR_RND);
    }

    DLLLOCAL qore_number_private(const char* str) : qore_number_private_intern(QORE_MAX(QORE_DEFAULT_PREC, strlen(str)*5)) {
        // see if number has an exponent and increase the number's precision if necessary
        const char* p = strchrs(str, "eE");
        if (p) {
            int exp = abs(atoi(p + 1));
            mpfr_prec_t np = exp * 5;
            if (np > getPrec())
                setPrec(np);
        }
        if (!str[0])
            mpfr_set_sj(num, 0, QORE_MPFR_RND);
        else
            mpfr_set_str(num, str, 10, QORE_MPFR_RND);
    }

    DLLLOCAL qore_number_private(const char* str, unsigned prec) : qore_number_private_intern(QORE_MAX(QORE_DEFAULT_PREC, prec)) {
        mpfr_set_str(num, str, 10, QORE_MPFR_RND);
    }

    DLLLOCAL qore_number_private(const qore_number_private& old) : qore_number_private_intern(mpfr_get_prec(old.num)) {
        mpfr_set(num, old.num, QORE_MPFR_RND);
    }

    DLLLOCAL double getAsFloat() const {
        return mpfr_get_d(num, QORE_MPFR_RND);
    }

    DLLLOCAL int64 getAsBigInt() const {
        return mpfr_get_sj(num, QORE_MPFR_RNDZ);
    }

    DLLLOCAL bool getAsBool() const {
        return !zero();
    }

    DLLLOCAL bool zero() const {
        return (bool)mpfr_zero_p(num);
    }

    DLLLOCAL bool nan() const {
        return (bool)mpfr_nan_p(num);
    }

    DLLLOCAL bool inf() const {
        return (bool)mpfr_inf_p(num);
    }

    DLLLOCAL bool number() const {
        return (bool)mpfr_number_p(num);
    }

#ifdef HAVE_MPFR_REGULAR
    // regular and not zero
    DLLLOCAL bool regular() const {
        return (bool)mpfr_regular_p(num);
    }
#endif

    DLLLOCAL int sign() const {
        return mpfr_sgn(num);
    }

    DLLLOCAL void sprintf(QoreString& str, const char* fmt) const {
#ifdef HAVE_MPFR_SPRINTF
        //printd(5, "qore_number_private::sprintf() fmt: '%s'\n", fmt);
        int len = mpfr_snprintf(0, 0, fmt, num);
        if (!len)
            return;
        if (len < 0) {
            numError(str);
            return;
        }
        str.allocate(str.size() + len + 1);
        mpfr_sprintf((char*)(str.getBuffer() + str.size()), fmt, num);
        str.terminate(str.size() + len);
#else
        // if there is no mpfr_sprintf, then we convert to a long double and output the number
        long double ld = mpfr_get_ld(num, QORE_MPFR_RND);
        int len = ::snprintf(0, 0, fmt, ld);
        if (len <= 0)
            return;
        str.allocate(str.size() + len + 1);
        ::sprintf((char*)(str.getBuffer() + str.size()), fmt, ld);
        str.terminate(str.size() + len);
#endif
    }

    DLLLOCAL void getScientificString(QoreString& str, bool round = true) const {
#ifdef HAVE_MPFR_SPRINTF
        sprintf(str, "%Re");
#else
        sprintf(str, "%Le");
#endif

        if (round) {
            qore_offset_t i = str.find('.');
            if (i != -1) {
                qore_offset_t e = str.rfind('e');
                if (e != -1) {
                    applyRoundingHeuristic(str, i, e);
                }
            }
        }
    }

    DLLLOCAL void getAsString(QoreString& str, bool round = true, int base = 10) const;

    DLLLOCAL void toString(QoreString& str, int fmt = QORE_NF_DEFAULT) const {
        bool raw = !(fmt & QORE_NF_RAW);
        if (fmt & QORE_NF_SCIENTIFIC)
            getScientificString(str, raw);
        else
            getAsString(str, raw);
    }

    DLLLOCAL int format(QoreString& str, const QoreString& fmt, ExceptionSink* xsink) {
        getAsString(str);
        return formatNumberString(str, fmt, xsink);
    }

    DLLLOCAL int format(QoreString& str, int prec, const QoreString& dsep_str, const QoreString& tsep_str, ExceptionSink* xsink) {
        getAsString(str);
        return formatNumberString(str, prec, dsep_str, tsep_str, xsink);
    }

    DLLLOCAL bool lessThan(const qore_number_private& right) const {
        return mpfr_less_p(num, right.num);
    }

    DLLLOCAL bool lessThan(double right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num) || std::isnan(right)) // If any of the "numbers" is NaN.
            return false;
        mpfr_set_d(r, right, QORE_MPFR_RND);
        return mpfr_less_p(num, r);
    }

    DLLLOCAL bool lessThan(int64 right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num)) // If the number is NaN.
            return false;
        mpfr_set_sj(r, right, QORE_MPFR_RND);
        return mpfr_less_p(num, r);
    }

    DLLLOCAL bool lessThanOrEqual(const qore_number_private& right) const {
        return mpfr_lessequal_p(num, right.num);
    }

    DLLLOCAL bool lessThanOrEqual(double right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num) || std::isnan(right)) // If any of the "numbers" is NaN.
            return false;
        mpfr_set_d(r, right, QORE_MPFR_RND);
        return mpfr_lessequal_p(num, r);
    }

    DLLLOCAL bool lessThanOrEqual(int64 right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num)) // If the number is NaN.
            return false;
        mpfr_set_sj(r, right, QORE_MPFR_RND);
        return mpfr_lessequal_p(num, r);
    }

    DLLLOCAL bool greaterThan(const qore_number_private& right) const {
        return mpfr_greater_p(num, right.num);
    }

    DLLLOCAL bool greaterThan(double right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num) || std::isnan(right)) // If any of the "numbers" is NaN.
            return false;
        mpfr_set_d(r, right, QORE_MPFR_RND);
        return mpfr_greater_p(num, r);
    }

    DLLLOCAL bool greaterThan(int64 right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num)) // If the number is NaN.
            return false;
        mpfr_set_sj(r, right, QORE_MPFR_RND);
        return mpfr_greater_p(num, r);
    }

    DLLLOCAL bool greaterThanOrEqual(const qore_number_private& right) const {
        return mpfr_greaterequal_p(num, right.num);
    }

    DLLLOCAL bool greaterThanOrEqual(double right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num) || std::isnan(right)) // If any of the "numbers" is NaN.
            return false;
        mpfr_set_d(r, right, QORE_MPFR_RND);
        return mpfr_greaterequal_p(num, r);
    }

    DLLLOCAL bool greaterThanOrEqual(int64 right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num)) // If the number is NaN.
            return false;
        mpfr_set_sj(r, right, QORE_MPFR_RND);
        return mpfr_greaterequal_p(num, r);
    }

    DLLLOCAL bool equals(const qore_number_private& right) const {
        return mpfr_equal_p(num, right.num);
    }

    DLLLOCAL bool equals(double right) const {
        if (mpfr_nan_p(num) || std::isnan(right)) // If any of the "numbers" is NaN.
            return false;
        return 0 == mpfr_cmp_d(num, right);
    }

    DLLLOCAL bool equals(int64 right) const {
        MPFR_TMP_VAR(r, QORE_DEFAULT_PREC);
        if (mpfr_nan_p(num)) // If the number is NaN.
            return false;
        mpfr_set_sj(r, right, QORE_MPFR_RND);
        return mpfr_equal_p(num, r);
    }

    DLLLOCAL qore_number_private* doBinary(q_mpfr_binary_func_t func, const qore_number_private& r, ExceptionSink* xsink = 0) const {
        mpfr_prec_t prec;
        if (func == mpfr_pow) {
            prec = mpfr_get_prec(num) * QORE_MIN(QORE_MAX_PREC, r.getAsBigInt());
        } else if (func == mpfr_mul || func == mpfr_div) {
            prec = mpfr_get_prec(num) + mpfr_get_prec(r.num);
        } else {
            prec = QORE_MAX(mpfr_get_prec(num), mpfr_get_prec(r.num)) + 1;
        }
        std::unique_ptr<qore_number_private> p(new qore_number_private(prec));
        func(p->num, num, r.num, QORE_MPFR_RND);
        if (xsink)
            checkFlags(xsink);

        return (xsink && *xsink) ? 0 : p.release();
    }

    DLLLOCAL qore_number_private* doPlus(const qore_number_private& r) const {
        return doBinary(mpfr_add, r);
    }

    DLLLOCAL qore_number_private* doMinus(const qore_number_private& r) const {
        return doBinary(mpfr_sub, r);
    }

    DLLLOCAL qore_number_private* doMultiply(const qore_number_private& r) const {
        return doBinary(mpfr_mul, r);
    }

    DLLLOCAL qore_number_private* doDivideBy(const qore_number_private& r, ExceptionSink* xsink) const {
#ifndef HAVE_MPFR_DIVBY0
        if (r.zero()) {
            do_divide_by_zero(xsink);
            return 0;
        }
#endif
        return doBinary(mpfr_div, r, xsink);
    }

    DLLLOCAL qore_number_private* doUnary(q_mpfr_unary_func_t func, ExceptionSink* xsink = 0) const {
        qore_number_private* p = new qore_number_private(*this);
        func(p->num, num, QORE_MPFR_RND);
        if (xsink)
            checkFlags(xsink);

        return p;
    }

    DLLLOCAL void negateInPlace() {
        mpfr_neg(num, num, QORE_MPFR_RND);
    }

    DLLLOCAL qore_number_private* negate() const {
        return doUnary(mpfr_neg);
    }

    DLLLOCAL qore_number_private* absolute() const {
        return doUnary(mpfr_abs);
    }

    DLLLOCAL qore_number_private* doUnaryNR(q_mpfr_unary_nr_func_t func, ExceptionSink* xsink = 0) const {
        qore_number_private* p = new qore_number_private(*this);
        func(p->num, num);
        if (xsink)
            checkFlags(xsink);

        return p;
    }

    // for round functions: round(), ceil(), floor()
    DLLLOCAL qore_number_private* doRoundNR(q_mpfr_unary_nr_func_t func, int prec = 0, ExceptionSink* xsink = NULL) const {
        unique_ptr<qore_number_private> p0(new qore_number_private(*this));

        if (prec == 0) {
            func(p0 -> num, num);

            if (xsink)
                checkFlags(xsink);

            return p0.release();
        }

        qore_number_private* p2;

        if (prec > 0) {
            unique_ptr<qore_number_private> c(new qore_number_private(pow(10, prec)));
            unique_ptr<qore_number_private> p1(p0 -> doMultiply(*c));
            func(p1 -> num, p1 -> num);
            p2 = p1 -> doDivideBy(*c, xsink);
        } else {
            unique_ptr<qore_number_private> c(new qore_number_private(pow(10, -prec)));
            unique_ptr<qore_number_private> p1(p0 -> doDivideBy(*c, xsink));
            func(p1 -> num, p1 -> num);
            p2 = p1 -> doMultiply(*c);
        }

        if (xsink)
            checkFlags(xsink);

        return p2;
    }

    DLLLOCAL mpfr_prec_t getPrec() const {
        return mpfr_get_prec(num);
    }

    DLLLOCAL void inc() {
        MPFR_TMP_VAR(tmp, mpfr_get_prec(num));
        mpfr_set(tmp, num, QORE_MPFR_RND);
        mpfr_add_si(num, tmp, 1, QORE_MPFR_RND);
    }

    DLLLOCAL void dec() {
        MPFR_TMP_VAR(tmp, mpfr_get_prec(num));
        mpfr_set(tmp, num, QORE_MPFR_RND);
        mpfr_sub_si(num, tmp, 1, QORE_MPFR_RND);
    }

    DLLLOCAL void doBinaryInplace(q_mpfr_binary_func_t func, const qore_number_private& r, ExceptionSink* xsink = 0) {
        checkPrec(func, r.num);
        // some compilers (sun/oracle pro c++ notably) do not support arrays with a variable size
        // if not, we can't use the stack for the temporary variable and have to use a dynamically-allocated one
        MPFR_TMP_VAR(tmp, mpfr_get_prec(num));
        mpfr_set(tmp, num, QORE_MPFR_RND);
        func(num, tmp, r.num, QORE_MPFR_RND);
        if (xsink)
            checkFlags(xsink);
    }

    DLLLOCAL void plusEquals(const qore_number_private& r) {
        doBinaryInplace(mpfr_add, r);
    }

    DLLLOCAL void minusEquals(const qore_number_private& r) {
        doBinaryInplace(mpfr_sub, r);
    }

    DLLLOCAL void multiplyEquals(const qore_number_private& r) {
        doBinaryInplace(mpfr_mul, r);
    }

    DLLLOCAL void divideEquals(const qore_number_private& r) {
        assert(!r.zero());
        doBinaryInplace(mpfr_div, r);
    }

    DLLLOCAL static void negateInPlace(QoreNumberNode& n) {
        n.priv->negateInPlace();
    }

    DLLLOCAL static int doRound(QoreString& num, qore_offset_t& dp, int prec);

    DLLLOCAL static int formatNumberString(QoreString& num, const QoreString& fmt, ExceptionSink* xsink);

    DLLLOCAL static int formatNumberString(QoreString& num, int prec, const QoreString& dsep_str, const QoreString& tsep_str, ExceptionSink* xsink);

protected:
    // assumes dsep, tsep and num all have the same encoding
    DLLLOCAL static int formatNumberStringIntern(QoreString& num, int prec, const QoreString& dsep, const QoreString& tsep, ExceptionSink* xsink);

public:
    DLLLOCAL static void numError(QoreString& str) {
        str.concat("<number error>");
    }

    // try to remove noise from the binary -> decimal conversion process in insignificant digits
    DLLLOCAL static void applyRoundingHeuristic(QoreString& str, size_t dp, size_t last,
        int round_threshold_1 = QORE_MPFR_ROUND_THRESHOLD, int round_threshold_2 = QORE_MPFR_ROUND_THRESHOLD_2);

    // try to remove noise from the binary -> decimal conversion process in insignificant digits
    /** finds the decimal point and attempts to remove noise and round the number if found
    */
    DLLLOCAL static void applyRoundingHeuristicToString(QoreString& str,
                int round_threshold_1 = QORE_MPFR_ROUND_THRESHOLD,
        int round_threshold_2 = QORE_MPFR_ROUND_THRESHOLD_2) {
        qore_offset_t i = str.find('.');
        if (i != -1) {
            applyRoundingHeuristic(str, i, str.size(), round_threshold_1, round_threshold_2);
        }
    }

    // returns number of digits inserted
    DLLLOCAL static int roundUp(QoreString& str, qore_offset_t pos);

    // static accessor methods
    DLLLOCAL static void sprintf(const QoreNumberNode& n, QoreString& str, const char* fmt) {
        n.priv->sprintf(str, fmt);
    }

    DLLLOCAL static void inc(QoreNumberNode& n) {
        n.priv->inc();
    }

    DLLLOCAL static void dec(QoreNumberNode& n) {
        n.priv->dec();
    }

    DLLLOCAL static void plusEquals(QoreNumberNode& n, const QoreNumberNode& r) {
        n.priv->plusEquals(*r.priv);
    }

    DLLLOCAL static void minusEquals(QoreNumberNode& n, const QoreNumberNode& r) {
        n.priv->minusEquals(*r.priv);
    }

    DLLLOCAL static void multiplyEquals(QoreNumberNode& n, const QoreNumberNode& r) {
        n.priv->multiplyEquals(*r.priv);
    }

    DLLLOCAL static void divideEquals(QoreNumberNode& n, const QoreNumberNode& r) {
        n.priv->divideEquals(*r.priv);
    }

    DLLLOCAL static QoreNumberNode* doUnary(const QoreNumberNode& n, q_mpfr_unary_func_t func, ExceptionSink* xsink = 0) {
        qore_number_private* p = n.priv->doUnary(func, xsink);
        return p ? new QoreNumberNode(p) : 0;
    }

    DLLLOCAL static QoreNumberNode* doBinary(const QoreNumberNode& n, q_mpfr_binary_func_t func, const QoreNumberNode& r, ExceptionSink* xsink = 0) {
        qore_number_private* p = n.priv->doBinary(func, *r.priv, xsink);
        return p ? new QoreNumberNode(p) : 0;
    }

    DLLLOCAL static QoreNumberNode* doUnaryNR(const QoreNumberNode& n, q_mpfr_unary_nr_func_t func, ExceptionSink* xsink = 0) {
        qore_number_private* p = n.priv->doUnaryNR(func, xsink);
        return p ? new QoreNumberNode(p) : 0;
    }

    DLLLOCAL static QoreNumberNode* doRoundNR(const QoreNumberNode& n, q_mpfr_unary_nr_func_t func, int prec = 0, ExceptionSink* xsink = 0) {
        qore_number_private* p = n.priv->doRoundNR(func, prec, xsink);
        return p ? new QoreNumberNode(p) : 0;
    }

    DLLLOCAL static QoreNumberNode* getNaNumber() {
        return new QoreNumberNode(new qore_number_private("@NaN@"));
    }

    DLLLOCAL static QoreNumberNode* getInfinity() {
        return new QoreNumberNode(new qore_number_private("@Inf@"));
    }

    DLLLOCAL static QoreNumberNode* getPi() {
        qore_number_private* p = new qore_number_private(0ll);
        mpfr_const_pi(p->num, QORE_MPFR_RND);
#ifdef HAVE_MPFR_BUILDOPT_TLS_P
        // avoid leaving MPFR cache allocations behind after one-time PI initialization
        mpfr_free_cache2(static_cast<mpfr_free_cache_t>(MPFR_FREE_LOCAL_CACHE | MPFR_FREE_GLOBAL_CACHE));
        mpfr_free_pool();
        mpfr_mp_memory_cleanup();
#endif
        return new QoreNumberNode(p);
    }

    DLLLOCAL static qore_number_private* get(const QoreNumberNode& n) {
        return n.priv;
    }

    DLLLOCAL static QoreStringNode* toBase(double f, int base, ExceptionSink* xsink) {
        std::unique_ptr<qore_number_private> n(new qore_number_private(f));
        return qore_number_private::toBase(n.get(), base, xsink);
    }

    DLLLOCAL static QoreStringNode* toBase(const QoreNumberNode& n, int base, ExceptionSink* xsink) {
        return qore_number_private::toBase(n.priv, base, xsink);
    }

    DLLLOCAL static QoreStringNode* toBase(qore_number_private* n, int base, ExceptionSink* xsink) {
        if (base < 2 || base > 36) {
            xsink -> raiseException("INVALID-BASE", "base %d is invalid; base must be 2 - 36 inclusive", base);
            return 0;
        }

        QoreString qs;
        n -> getAsString(qs, 1, base);
        qs.toupr();
        return new QoreStringNode(qs);
    }
};

#endif
