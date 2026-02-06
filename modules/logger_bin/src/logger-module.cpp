/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    logger-module.cpp

    Qore logger module

    Copyright (C) 2017 - 2026 Qore Technologies s.r.o.

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

#include "qore/Qore.h"

#include "qore_logger.h"

#include "QC_LoggerLevel.h"
#include "QC_LoggerPattern.h"
#include "QC_LoggerEvent.h"
#include "QC_LoggerFilter.h"
#include "QC_LoggerLayout.h"
#include "QC_LoggerLayoutPattern.h"
#include "QC_LoggerAppender.h"
#include "QC_LoggerAppenderQueue.h"
#include "QC_LoggerAppenderWithLayout.h"
#include "QC_LoggerAppenderFile.h"
#include "QC_LoggerInterface.h"
#include "QC_Logger.h"

#include <string.h>

static void logger_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void logger_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void logger_module_delete();

extern "C" DLLEXPORT void logger_bin_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "logger_bin";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Qore logger module";
    mod_info.author = "David Nichols <david.nichols@qoretechnologies.com>";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = logger_module_init;
    mod_info.ns_init = logger_module_ns_init;
    mod_info.del = logger_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
    mod_info.functional_domains = QDOM_EXTERNAL_INFO;
}

QoreNamespace LoggerNS("Qore::Logger");

#ifndef HOSTNAMEBUFSIZE
#define HOSTNAMEBUFSIZE 512
#endif

static void logger_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    QoreLoggerEvent::init();

    // set up Logger namespace
    QoreClass* cls = initLoggerPatternClass(LoggerNS);
    cls->addBuiltinConstant("ESCAPE_CHAR", new QoreStringNode(ESCAPE_STR), Public, stringTypeInfo);
    LoggerNS.addSystemClass(cls);

    preinitLoggerInterfaceClass();
    preinitLoggerClass();

    cls = initLoggerLevelClass(LoggerNS);
    //! The highest logger level
    cls->addBuiltinConstant("OFF", QLL_OFF, Public, bigIntTypeInfo);
    //! Logger level for fatal errors
    cls->addBuiltinConstant("FATAL", QLL_FATAL, Public, bigIntTypeInfo);
    //! Logger level for (non-fatal) errors
    cls->addBuiltinConstant("ERROR", QLL_ERROR, Public, bigIntTypeInfo);
    //! Logger level for warnings
    cls->addBuiltinConstant("WARN", QLL_WARN, Public, bigIntTypeInfo);
    //! Logger level for informational messages
    cls->addBuiltinConstant("INFO", QLL_INFO, Public, bigIntTypeInfo);
    //! Logger level for detail messages
    cls->addBuiltinConstant("DETAIL", QLL_DETAIL, Public, bigIntTypeInfo);
    //! Logger level for debugging messages
    cls->addBuiltinConstant("DEBUG", QLL_DEBUG, Public, bigIntTypeInfo);
    //! Logger level for trace messages
    cls->addBuiltinConstant("TRACE", QLL_TRACE, Public, bigIntTypeInfo);
    //! The lowest logger level
    cls->addBuiltinConstant("ALL", QLL_ALL, Public, bigIntTypeInfo);

    // initialize constants
    QoreLoggerLevel::init();
    {
        const QoreTypeInfo* llti = cls->getTypeInfo();
        //! The highest logger level
        cls->addBuiltinConstant("LevelOff", QoreLoggerLevel::LevelOff->objectRefSelf(), Public, llti);
        //! Logger level for fatal errors
        cls->addBuiltinConstant("LevelFatal", QoreLoggerLevel::LevelFatal->objectRefSelf(), Public, llti);
        //! Logger level for (non-fatal) errors
        cls->addBuiltinConstant("LevelError", QoreLoggerLevel::LevelError->objectRefSelf(), Public, llti);
        //! Logger level for warnings
        cls->addBuiltinConstant("LevelWarn", QoreLoggerLevel::LevelWarn->objectRefSelf(), Public, llti);
        //! Logger level for informational messages
        cls->addBuiltinConstant("LevelInfo", QoreLoggerLevel::LevelInfo->objectRefSelf(), Public, llti);
        //! Logger level for detail messages
        cls->addBuiltinConstant("LevelDetail", QoreLoggerLevel::LevelDetail->objectRefSelf(), Public, llti);
        //! Logger level for debugging messages
        cls->addBuiltinConstant("LevelDebug", QoreLoggerLevel::LevelDebug->objectRefSelf(), Public, llti);
        //! Logger level for trace messages
        cls->addBuiltinConstant("LevelTrace", QoreLoggerLevel::LevelTrace->objectRefSelf(), Public, llti);
        //! The lowest logger level
        cls->addBuiltinConstant("LevelAll", QoreLoggerLevel::LevelAll->objectRefSelf(), Public, llti);
    }
    LoggerNS.addSystemClass(cls);

    LoggerNS.addSystemClass(initLoggerEventClass(LoggerNS));
    LoggerNS.addSystemClass(initLoggerLayoutClass(LoggerNS));

    cls = initLoggerFilterClass(LoggerNS);
    cls->addBuiltinConstant("ACCEPT", ACCEPT, Public, bigIntTypeInfo);
    cls->addBuiltinConstant("NEUTRAL", NEUTRAL, Public, bigIntTypeInfo);
    cls->addBuiltinConstant("DENY", DENY, Public, bigIntTypeInfo);
    LoggerNS.addSystemClass(cls);

    char buf[HOSTNAMEBUFSIZE + 1];
    if (gethostname(buf, HOSTNAMEBUFSIZE)) {
        xsink.raiseException("GETHOSTNAME-ERROR", "gethostname() failed: %s", strerror(errno));
        return;
    }

    cls = initLoggerLayoutPatternClass(LoggerNS);
    cls->addBuiltinConstant("DEFAULT_PATTERN", new QoreStringNode(DEFAULT_PATTERN), Public, stringTypeInfo);
    cls->addBuiltinConstant("DEFAULT_DATE_FORMAT", new QoreStringNode(DEFAULT_DATE_FORMAT), Public,
        stringTypeInfo);
    QoreLoggerLayoutPattern::HostName = new QoreStringNode(buf);
    cls->addBuiltinConstant("HostName", QoreLoggerLayoutPattern::HostName->stringRefSelf(), Public, stringTypeInfo);
    LoggerNS.addSystemClass(cls);

    preinitLoggerAppenderClass();
    preinitLoggerAppenderQueueClass();

    cls = initLoggerAppenderClass(LoggerNS);
    cls->addBuiltinConstant("EVENT_OPEN", EVENT_OPEN, Public, bigIntTypeInfo);
    cls->addBuiltinConstant("EVENT_LOG", EVENT_LOG, Public, bigIntTypeInfo);
    cls->addBuiltinConstant("EVENT_CLOSE", EVENT_CLOSE, Public, bigIntTypeInfo);
    LoggerNS.addSystemClass(cls);

    LoggerNS.addSystemClass(initLoggerAppenderQueueClass(LoggerNS));
    LoggerNS.addSystemClass(initLoggerAppenderWithLayoutClass(LoggerNS));

    cls = initLoggerAppenderFileClass(LoggerNS);
    cls->addBuiltinConstant("EVENT_REOPEN", EVENT_REOPEN, Public, bigIntTypeInfo);
    cls->addBuiltinConstant("DEFAULT_OPEN_FLAGS", DEFAULT_OPEN_FLAGS, Public, bigIntTypeInfo);
    LoggerNS.addSystemClass(cls);

    LoggerNS.addSystemClass(initLoggerInterfaceClass(LoggerNS));
    LoggerNS.addSystemClass(initLoggerClass(LoggerNS));
}

static void logger_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(LoggerNS.copy());
}

static void logger_module_delete() {
    LoggerNS.clear(nullptr);
    QoreLoggerLevel::del();
}
