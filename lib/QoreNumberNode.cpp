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
#include "qore/intern/QoreAsciiCompatStringHelper.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <memory>
#include <string>

// The significand contains a possible leading minus sign and decimal digits only.
// Work in a local string so a cancellation never appends a partial result to the caller's destination.
static int qore_format_round_trip_digits(QoreString& str, const std::string& significand, int64 exponent,
        bool scientific, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "formatting a round-trip number")) {
        return -1;
    }
    assert(!significand.empty());
    bool negative = significand[0] == '-';
    size_t start = negative ? 1 : 0;
    size_t end = significand.size();
    size_t count = 0;
    while (end > start + 1 && significand[end - 1] == '0') {
        if (!(++count % 100) && qore_check_cancel(xsink, "formatting a round-trip number")) {
            return -1;
        }
        --end;
    }
    assert(end > start);
    size_t digits = end - start;
    QoreString result;
    if (negative) {
        result.concat('-');
    }
    auto appendZeros = [&](int64 zeros) -> int {
        assert(zeros >= 0);
        while (zeros) {
            if (qore_check_cancel(xsink, "expanding a decimal exponent")) {
                return -1;
            }
            unsigned chunk = static_cast<unsigned>(std::min<int64>(zeros, 65536));
            result.addch('0', chunk);
            zeros -= chunk;
        }
        return 0;
    };
    if (scientific) {
        result.concat(significand[start]);
        if (digits > 1) {
            result.concat('.');
            result.concat(significand.data() + start + 1, digits - 1);
        }
        result.sprintf("e" QLLD, exponent - 1);
    } else if (exponent <= 0) {
        result.concat("0.");
        if (appendZeros(-exponent)) {
            return -1;
        }
        result.concat(significand.data() + start, digits);
    } else if (static_cast<uint64_t>(exponent) >= digits) {
        result.concat(significand.data() + start, digits);
        if (appendZeros(exponent - static_cast<int64>(digits))) {
            return -1;
        }
    } else {
        size_t point = static_cast<size_t>(exponent);
        result.concat(significand.data() + start, point);
        result.concat('.');
        result.concat(significand.data() + start + point, digits - point);
    }
    if (qore_check_cancel(xsink, "formatting a round-trip number")) {
        return -1;
    }
    str.concat(result.c_str(), result.size());
    return 0;
}

int qore_number_private::getFloatRoundTripString(QoreString& str, double value, bool scientific,
        ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "formatting a round-trip float")) {
        return -1;
    }
    if (!std::isfinite(value)) {
        str.concat(std::isnan(value) ? "NaN" : std::signbit(value) ? "-INF" : "INF");
        return 0;
    }
    if (!value) {
        str.concat(std::signbit(value) ? "-0" : "0");
        if (scientific) {
            str.concat("e0");
        }
        return 0;
    }
    // Scientific to_chars minimizes significant digits. Expanding this result gives the same
    // decimal value in plain notation; fixed to_chars can instead emit hundreds of integer digits.
    char buffer[64]; // double: sign, 17 digits, decimal point, e, exponent sign and at most 3 exponent digits
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
    if (converted.ec != std::errc()) {
        xsink->raiseException("NUMBER-CONVERSION-ERROR", "could not format a floating-point value");
        return -1;
    }
    std::string text(buffer, converted.ptr);
    size_t e = text.find('e');
    assert(e != std::string::npos);
    const char* exp_start = text.data() + e + 1;
    if (*exp_start == '+') {
        ++exp_start;
    }
    int exponent;
    auto parsed = std::from_chars(exp_start, text.data() + text.size(), exponent);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size()) {
        xsink->raiseException("NUMBER-CONVERSION-ERROR", "could not read a formatted decimal exponent");
        return -1;
    }
    text.resize(e);
    size_t point = text.find('.');
    if (point != std::string::npos) {
        text.erase(point, 1);
    }
    return qore_format_round_trip_digits(str, text, exponent + 1, scientific, xsink);
}

int qore_number_private::getRoundTripString(QoreString& str, bool scientific, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "formatting a round-trip number")) {
        return -1;
    }
    if (!mpfr_number_p(num)) {
        str.concat(mpfr_nan_p(num) ? "NaN" : mpfr_signbit(num) ? "-INF" : "INF");
        return 0;
    }
    if (mpfr_zero_p(num)) {
        str.concat(mpfr_signbit(num) ? "-0" : "0");
        if (scientific) {
            str.concat("e0");
        }
        return 0;
    }
    mpfr_exp_t exponent;
    using MpfrString = std::unique_ptr<char, decltype(&mpfr_free_str)>;
    MpfrString raw(mpfr_get_str(nullptr, &exponent, 10, 0, num, QORE_MPFR_RND), &mpfr_free_str);
    if (!raw) {
        xsink->raiseException("NUMBER-CONVERSION-ERROR", "could not format a number's significand");
        return -1;
    }
    std::string selected(raw.get());
    size_t low = 1;
    size_t high = selected.size() - (selected[0] == '-' ? 1 : 0);
    // MPFR's automatic digit count is a proven round-trip upper bound at this precision.
    // Decimal grids nest as precision increases: existence of a valid candidate is monotone.
    // Test both neighbors, since binary rounding intervals at powers of two are asymmetric.
    // A nearest-only test can miss the shortest valid decimal on the wider side of the interval.
    qore_number_private restored(mpfr_get_prec(num));
    auto selectDigits = [&](size_t digits, std::string& candidate, mpfr_exp_t& candidate_exp) -> bool {
        for (mpfr_rnd_t rounding : {MPFR_RNDN, MPFR_RNDD, MPFR_RNDU}) {
            if (qore_check_cancel(xsink, "finding a round-trip decimal significand")) {
                return false;
            }
            MpfrString text(mpfr_get_str(nullptr, &candidate_exp, 10, digits, num, rounding), &mpfr_free_str);
            if (!text) {
                xsink->raiseException("NUMBER-CONVERSION-ERROR", "could not format a number's significand");
                return false;
            }
            QoreString input;
            input.sprintf("%se" QLLD, text.get(), static_cast<int64>(candidate_exp) - static_cast<int64>(digits));
            int rc = mpfr_set_str(restored.num, input.c_str(), 10, QORE_MPFR_RND);
            if (rc) {
                xsink->raiseException("NUMBER-CONVERSION-ERROR", "could not parse a decimal significand");
                return false;
            }
            if (mpfr_equal_p(num, restored.num)) {
                candidate = text.get();
                return true;
            }
        }
        return false;
    };
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        std::string candidate;
        mpfr_exp_t candidate_exp;
        bool matches = selectDigits(mid, candidate, candidate_exp);
        if (*xsink) {
            return -1;
        }
        if (matches) {
            high = mid;
            selected = std::move(candidate);
            exponent = candidate_exp;
        } else {
            low = mid + 1;
        }
    }
    return qore_format_round_trip_digits(str, selected, static_cast<int64>(exponent), scientific, xsink);
}

void qore_number_private::getAsString(QoreString& str, bool round, int base) const {
    // first check for zero
    if (zero()) {
        str.concat("0");
        return;
    }

    mpfr_exp_t exp;

    // MPFR's automatic digit count is sufficient to round-trip at the source precision,
    // but can round away low integer digits when the exponent exceeds that precision.
    // Raw integral output must also retain its value in a higher-precision consumer.
    // The binary exponent bounds the number of digits in every supported base (>= 2).
    size_t digits = !round && mpfr_integer_p(num) ? static_cast<size_t>(mpfr_get_exp(num)) : 0;
    char* buf = mpfr_get_str(0, &exp, base, digits, num, QORE_MPFR_RND);
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
        if (dp && round) {
            applyRoundingHeuristic(str, dp, str.size(), QORE_MPFR_ROUND_THRESHOLD, QORE_MPFR_ROUND_THRESHOLD_2,
                len - (sgn < 0 ? 1 : 0));
        }
    } else
        str.concat(buf);

    //printd(5, "qore_number_private::getAsString() this: %p returning '%s'\n", this, str.c_str());
}

void qore_number_private::applyRoundingHeuristic(QoreString& str, size_t dp, size_t last, int round_threshold_1,
        int round_threshold_2, size_t num_start) {
    // the position of the last significant digit
    qore_offset_t pos = (qore_offset_t)dp;
    size_t i = dp;
    // when the integer part is zero, the zeros that follow the decimal point place the first significant digit
    // and are not conversion noise; start the scan at that digit, with the zero before it as the last digit kept
    if (dp && str[dp - 1] == '0' && (dp - 1 == num_start || (dp - 2 == num_start && str[num_start] == '-'))) {
        size_t first = dp + 1;
        while (first < last && str[first] == '0') {
            ++first;
        }
        if (first > dp + 1) {
            i = first;
            pos = (qore_offset_t)first - 1;
        }
    }
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
        QoreStringValueHelper str(n);
        QoreAsciiCompatStringHelper text(**str);
        priv = new qore_number_private(text.c_str());
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

// in the YAML formats, non-finite values must be rendered as ".nan", ".inf", and "-.inf"; the default rendering
// ("@NaN@", "@Inf@", "-@Inf@") cannot be parsed as YAML
// note that the sign is only queried for infinite values; mpfr_sgn() sets the erange flag when called on NaN
static bool concat_yaml_nonfinite_number(QoreString& str, const qore_number_private& n) {
    bool is_inf = n.inf();
    return q_concat_yaml_nonfinite(str, n.nan(), is_inf, is_inf && n.sign() < 0);
}

int QoreNumberNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    if (q_yaml_format(foff) && concat_yaml_nonfinite_number(str, *priv)) {
        return 0;
    }
    getStringRepresentation(str);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreNumberNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    if (q_yaml_format(foff)) {
        std::unique_ptr<QoreString> rv(new QoreString);
        if (concat_yaml_nonfinite_number(*rv, *priv)) {
            del = true;
            return rv.release();
        }
    }
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

int QoreNumberNode::toStringRoundTrip(QoreString& str, bool scientific, ExceptionSink* xsink) const {
    return priv->getRoundTripString(str, scientific, xsink);
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

    if (t == NT_STRING) {
        QoreStringValueHelper str(n);
        QoreAsciiCompatStringHelper text(**str);
        return new QoreNumberNode(text.c_str());
    }

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
