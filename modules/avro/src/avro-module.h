/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file avro-module.h avro module header file */
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

#ifndef _QORE_AVRO_MODULE_H
#define _QORE_AVRO_MODULE_H

#include <qore/Qore.h>
#include <qore/QoreJsonApi.h>
#include <qore/QoreSandboxManager.h>
#include <qore/qore_thread.h>

//! maximum schema and datum nesting depth
/** Unlike JSON_MAX_NESTING_DEPTH this is enforced unconditionally, not only under sandboxing:
    a recursive Avro schema lets the *datum* drive recursion depth without limit, so a few bytes
    of crafted input could otherwise overflow the C stack.  See design/avro-module.md.
*/
#define AVRO_MAX_NESTING_DEPTH 256

//! iterations between cooperative-cancellation checks in unbounded decode/encode loops
#define AVRO_INTERRUPT_CHECK_INTERVAL 100

//! the size of an object container file sync marker in bytes
#define AVRO_SYNC_SIZE 16

// namespace for the module
extern QoreNamespace AvroNs;

//! returns the json module's C++ API, resolving and caching it on the first call
/** An Avro schema is a JSON document, so the schema parser and the container-file writer need a
    JSON codec.  It is resolved through the module C++ API mechanism rather than linked, so the
    \c json module is loaded on demand the first time a schema is parsed; see
    design/module-cpp-api.md.

    @param xsink Qore-language exceptions are raised here

    @return the json module's API struct, or nullptr if an exception was raised
*/
DLLLOCAL const QoreJsonApi* avro_get_json_api(ExceptionSink* xsink);

#endif // _QORE_AVRO_MODULE_H
