/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRIntrinsic.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    This source is released under the MIT license; see README-LICENSE.
*/

#include <qore/intern/QoreJITIncludes.h>
#include <qore/intern/QoreIRIntrinsic.h>

#include <atomic>
#include <cstdio>
#include <cstring>

//! the number of the resolutions of pseudo-method intrinsics by name, which QORE_IR_OPT_STATS reports at exit, so that
//! tests can check that a call does not resolve it again (#6709)
static std::atomic<uint64_t> qore_ir_pseudo_intrinsic_resolutions{0};

static void qore_ir_report_pseudo_intrinsic_resolutions() {
    fprintf(stderr, "IR-OPT: pseudo-intrinsic-resolutions=%llu\n",
        static_cast<unsigned long long>(qore_ir_pseudo_intrinsic_resolutions.load(std::memory_order_relaxed)));
}

static bool qore_ir_pseudo_intrinsic_stats() {
    if (!QORE_ENV_IS_SET("QORE_IR_OPT_STATS")) {
        return false;
    }
    atexit(qore_ir_report_pseudo_intrinsic_resolutions);
    return true;
}

QoreIRIntrinsic qore_ir_resolve_pseudo_intrinsic(const QoreMethod* method, const QoreClass* qc,
        const char* fallback_name) {
    static const bool stats = qore_ir_pseudo_intrinsic_stats();
    if (stats) {
        qore_ir_pseudo_intrinsic_resolutions.fetch_add(1, std::memory_order_relaxed);
    }
    const char* name = method ? method->getName() : fallback_name;
    if (!name) {
        return QoreIRIntrinsic::None;
    }
    const char* class_name = qc ? qc->getName() : nullptr;
    if (class_name && !strcmp(class_name, "<string>")) {
        if (!strcmp(name, "size")) {
            return QoreIRIntrinsic::Size;
        }
        if (!strcmp(name, "empty")) {
            return QoreIRIntrinsic::Empty;
        }
        if (!strcmp(name, "val")) {
            return QoreIRIntrinsic::Val;
        }
        if (!strcmp(name, "strlen")) {
            return QoreIRIntrinsic::StringStrlen;
        }
        if (!strcmp(name, "length")) {
            return QoreIRIntrinsic::StringLength;
        }
        if (!strcmp(name, "sizep")) {
            return QoreIRIntrinsic::StringSizeP;
        }
        if (!strcmp(name, "strp")) {
            return QoreIRIntrinsic::StringStrP;
        }
        if (!strcmp(name, "intp")) {
            return QoreIRIntrinsic::StringIntP;
        }
        if (!strcmp(name, "lwr")) {
            return QoreIRIntrinsic::StringLower;
        }
        if (!strcmp(name, "upr")) {
            return QoreIRIntrinsic::StringUpper;
        }
        if (!strcmp(name, "toInt")) {
            return QoreIRIntrinsic::StringToInt;
        }
        if (!strcmp(name, "startsWith")) {
            return QoreIRIntrinsic::StringStartsWith;
        }
        if (!strcmp(name, "endsWith")) {
            return QoreIRIntrinsic::StringEndsWith;
        }
        if (!strcmp(name, "contains")) {
            return QoreIRIntrinsic::StringContains;
        }
        if (!strcmp(name, "find")) {
            return QoreIRIntrinsic::StringFind;
        }
        if (!strcmp(name, "rfind")) {
            return QoreIRIntrinsic::StringRFind;
        }
        if (!strcmp(name, "substr")) {
            return QoreIRIntrinsic::StringSubstr;
        }
    } else if (class_name && !strcmp(class_name, "<list>")) {
        if (!strcmp(name, "first")) {
            return QoreIRIntrinsic::ListFirst;
        }
        if (!strcmp(name, "last")) {
            return QoreIRIntrinsic::ListLast;
        }
    }
    if (!strcmp(name, "typeCode")) {
        return QoreIRIntrinsic::TypeCode;
    }
    if (!strcmp(name, "type")) {
        return QoreIRIntrinsic::Type;
    }
    if (!strcmp(name, "toNumber")) {
        return QoreIRIntrinsic::ToNumber;
    }
    if (!strcmp(name, "size")) {
        return QoreIRIntrinsic::Size;
    }
    if (!strcmp(name, "empty")) {
        return QoreIRIntrinsic::Empty;
    }
    if (!strcmp(name, "val")) {
        return QoreIRIntrinsic::Val;
    }
    return QoreIRIntrinsic::None;
}
