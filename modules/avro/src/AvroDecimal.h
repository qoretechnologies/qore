/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroDecimal.h arbitrary-precision conversion for the Avro decimal logical type */
/*
    Qore avro module

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#ifndef _QORE_AVRO_AVRODECIMAL_H
#define _QORE_AVRO_AVRODECIMAL_H

#include "avro-module.h"

#include <vector>

//! the largest decimal precision the module accepts in a schema
/** The Avro specification does not cap precision, but conversion between base 256 and base 10 is
    quadratic in the number of digits, so an uncapped precision would let a schema dictate
    unbounded decode cost.  1000 digits is far beyond any real use (SQL tops out at 38 to 65) and
    bounds the work per value.
*/
#define AVRO_MAX_PRECISION 1000

//! returns the largest decimal precision representable in a two's-complement value of \a size bytes
DLLLOCAL int avro_decimal_max_precision(unsigned size);

//! converts a big-endian two's-complement unscaled integer to its exact decimal string
/** @param str the string to write the result to; it is not cleared first
    @param buf the unscaled integer, big-endian two's complement
    @param len the length of \a buf in bytes; 0 is not valid
    @param scale the number of digits to the right of the decimal point

    @return 0 for OK, -1 if an exception was raised
*/
DLLLOCAL int avro_decimal_to_string(QoreString& str, const unsigned char* buf, size_t len,
        int scale, ExceptionSink* xsink);

//! converts an exact decimal string to a big-endian two's-complement unscaled integer
/** Accepts an optional sign, digits with an optional decimal point, and an optional decimal
    exponent (\c e or \c E).  Raises an exception rather than rounding if the value cannot be
    represented exactly at \a scale, or if it needs more than \a precision significant digits.

    @param out receives the unscaled integer, big-endian two's complement, in the shortest
        representation that preserves the sign
    @param str the decimal string to convert
    @param precision the maximum number of significant digits allowed
    @param scale the number of digits to the right of the decimal point
    @param what a description of the value being converted, used in exception messages

    @return 0 for OK, -1 if an exception was raised
*/
DLLLOCAL int avro_decimal_from_string(std::vector<unsigned char>& out, const char* str,
        int precision, int scale, const char* what, ExceptionSink* xsink);

//! sign-extends \a buf to exactly \a size bytes in place
/** @return 0 for OK, -1 if an exception was raised because the value does not fit
*/
DLLLOCAL int avro_decimal_sign_extend(std::vector<unsigned char>& buf, unsigned size,
        const char* what, ExceptionSink* xsink);

#endif // _QORE_AVRO_AVRODECIMAL_H
