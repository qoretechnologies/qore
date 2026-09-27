/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    yaml-scalar-util.h

    Shared scalar parsing utilities for YAML module

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_YAML_SCALAR_UTIL_H
#define _QORE_YAML_SCALAR_UTIL_H

#include <qore/Qore.h>
#include <yaml.h>

// Forward declarations
DLLLOCAL extern const char* QORE_YAML_DURATION_TAG;
DLLLOCAL extern const char* QORE_YAML_NUMBER_TAG;
DLLLOCAL extern const char* QORE_YAML_SQLNULL_TAG;

//! Parse a scalar value with an explicit YAML tag
/** @param val the scalar value as a string
    @param len the length of the value
    @param tag the YAML tag (e.g., YAML_INT_TAG, QORE_YAML_DURATION_TAG)
    @param xsink exception sink for error reporting
    @return the parsed value, or nothing on error
*/
DLLLOCAL QoreValue yaml_parse_tagged_scalar(const char* val, size_t len, const char* tag, ExceptionSink* xsink);

//! Parse a scalar value with type inference (no explicit tag)
/** @param val the scalar value as a string
    @param len the length of the value
    @param style the YAML scalar style (plain, quoted, etc.)
    @param favor_string if true, return string for ambiguous values
    @param xsink exception sink for error reporting
    @return the inferred value
*/
DLLLOCAL QoreValue yaml_parse_implicit_scalar(const char* val, size_t len, yaml_scalar_style_t style,
                                               bool favor_string, ExceptionSink* xsink);

//! Formats a finite float exactly and as readably as possible
/** The value is written with 15, 16, or 17 significant digits, whichever is the fewest that parse back to the same
    value; as \c %g drops trailing zeros, a value such as \c 99.99 is written as such.  An integral value gets a
    \c ".0" suffix, so that it is parsed back as a float

    @param out the string to set
    @param f the value; must be finite
*/
DLLLOCAL void yaml_format_finite_float(QoreString& out, double f);

//! Parse an untagged scalar with the YAML 1.2 core schema
/** Only plain scalars are resolved, and only to null, booleans, integers (decimal, \c 0o octal and \c 0x
    hexadecimal), and floats (including \c .inf and \c .nan); every other scalar is a string.  This is the
    type resolution of JSON-compatible YAML documents such as OpenAPI descriptions, where Qore's extended types
    (dates, durations, arbitrary-precision numbers, SQL null) would change values.

    @param val the scalar value
    @param len the length of the value
    @param style the scalar style
    @param xsink exception sink
    @return the parsed value
*/
DLLLOCAL QoreValue yaml_parse_core_schema_scalar(const char* val, size_t len, yaml_scalar_style_t style,
                                                 ExceptionSink* xsink);

//! Check if a value looks like an ISO 8601 absolute date/time
/** @param len the length of the value
    @param val the value string
    @param quoted true if the value was quoted in the YAML source
    @return true if the value appears to be an absolute date/time
*/
DLLLOCAL bool yaml_check_absolute_date(size_t len, const char* val, bool quoted = false);

//! Check if a value is an ISO 8601 duration (P[n]Y[n]M[n]DT[n]H[n]M[n]S) with at least one component
/** @param val the value string
    @param xsink exception sink for cancellation
    @return true if the value appears to be a duration
*/
DLLLOCAL bool yaml_check_duration(const char* val, ExceptionSink* xsink);

//! Parse an ISO 8601 absolute date/time value
/** @param val the date/time string
    @param len the length of the string
    @param xsink exception sink for error reporting
    @return the parsed DateTimeNode
*/
DLLLOCAL DateTimeNode* yaml_parse_absolute_date(const char* val, size_t len, ExceptionSink* xsink);

//! Parse an ISO 8601 duration value
/** @param val the duration string
    @return the parsed DateTimeNode (relative)
*/
DLLLOCAL DateTimeNode* yaml_parse_duration(const char* val);

//! Try to parse a value as a number (int, float, or arbitrary precision)
/** Uses locale-independent parsing.
    @param val the value string
    @param len the length of the string
    @param xsink exception sink for cancellation
    @param no_simple_numeric if true, don't return simple int/float, only number
    @return the parsed number, or nothing if not a valid number
*/
DLLLOCAL QoreValue yaml_try_parse_number(const char* val, size_t len, ExceptionSink* xsink,
                                            bool no_simple_numeric = false);

//! Parse a float value with locale-independent handling
/** Handles YAML .inf / .nan spellings and Qore @inf@ / @nan@ special values.
    Finite values must have a decimal mantissa and optional exponent with digits in each.
    @param val the value string
    @param len the length of the string
    @param xsink exception sink for invalid float values
    @return the parsed double value
*/
DLLLOCAL double yaml_parse_float(const char* val, size_t len, ExceptionSink* xsink);

//! Parse an arbitrary-precision number value
/** Handles precision suffix in format: value{precision}
    @param val the value string
    @param len the length of the string
    @param xsink exception sink for cancellation
    @return the parsed QoreNumberNode
*/
DLLLOCAL QoreNumberNode* yaml_parse_number(const char* val, size_t len, ExceptionSink* xsink);

#endif // _QORE_YAML_SCALAR_UTIL_H
