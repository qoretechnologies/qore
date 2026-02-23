/* -*- indent-tabs-mode: nil -*- */
/*
    RuntimeConfig.h

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

#ifndef _QORE_RUNTIMECONFIG_H
#define _QORE_RUNTIMECONFIG_H

#include <qore/common.h>
#include <qore/QoreParseOptions.h>
#include <cstdint>

class QoreProgram;
class QoreObject;
class AbstractStatement;
struct QoreProgramLocation;
struct ThreadLocalProgramData;
class QoreClosureBase;
class qore_class_private;
class QoreTypeInfo;
class QoreStackLocation;

//! Runtime configuration passed through the evaluation call chain.
class RuntimeConfig {
public:
    class Impl;

    DLLEXPORT RuntimeConfig();
    DLLEXPORT RuntimeConfig(const RuntimeConfig& other);
    DLLEXPORT RuntimeConfig(RuntimeConfig&& other) noexcept;
    DLLEXPORT RuntimeConfig& operator=(const RuntimeConfig& other);
    DLLEXPORT RuntimeConfig& operator=(RuntimeConfig&& other) noexcept;
    DLLEXPORT ~RuntimeConfig();

    DLLEXPORT QoreProgram* getProgram() const;
    DLLEXPORT void setProgram(QoreProgram* pgm);

    DLLEXPORT const QoreProgramLocation* getLocation() const;
    DLLEXPORT void setLocation(const QoreProgramLocation* loc);

    DLLEXPORT const AbstractStatement* getStatement() const;
    DLLEXPORT void setStatement(const AbstractStatement* stmt);

    DLLEXPORT QoreParseOptions getParseOptions() const;
    DLLEXPORT void setParseOptions(const QoreParseOptions& po);

    DLLEXPORT ThreadLocalProgramData* getThreadLocalProgramData() const;
    DLLEXPORT void setThreadLocalProgramData(ThreadLocalProgramData* tlpd);

    DLLEXPORT QoreObject* getObject() const;
    DLLEXPORT void setObject(QoreObject* obj);

    DLLEXPORT const qore_class_private* getClass() const;
    DLLEXPORT void setClass(const qore_class_private* cls);

    DLLEXPORT const QoreStackLocation* getStackLocation() const;
    DLLEXPORT void setStackLocation(const QoreStackLocation* stack_loc);

    DLLEXPORT const QoreTypeInfo* getReturnTypeInfo() const;
    DLLEXPORT void setReturnTypeInfo(const QoreTypeInfo* return_type_info);

    DLLEXPORT q_rt_flags_t getRuntimeFlags() const;
    DLLEXPORT void setRuntimeFlags(q_rt_flags_t flags);

    DLLEXPORT int getElement() const;
    DLLEXPORT void setElement(int element);

    DLLEXPORT const QoreClosureBase* getClosureEnv() const;
    DLLEXPORT void setClosureEnv(const QoreClosureBase* closure_env);

    DLLEXPORT bool hasThreadLocalData() const;
    DLLEXPORT bool isValid() const;

private:
    Impl* impl;
    bool owned;

    DLLEXPORT explicit RuntimeConfig(Impl* impl, bool owned);
    DLLEXPORT Impl& implRef();
    DLLEXPORT const Impl& implRef() const;

    friend RuntimeConfig rc_get_current();
    friend RuntimeConfig rc_get_parse_time();
    friend RuntimeConfig& rc_get_current_ref();
    friend RuntimeConfig& rc_get_tls_ref();
    friend void rc_sync_to_thread(const RuntimeConfig& rc);
    friend class RuntimeConfigHelper;
    friend class RuntimeConfigLocationHelper;
    friend class RuntimeConfigObjectHelper;
    friend class RuntimeConfigClosureHelper;
    friend class RuntimeConfigElementHelper;
    friend class RuntimeConfigStackHelper;
};

#endif // _QORE_RUNTIMECONFIG_H
