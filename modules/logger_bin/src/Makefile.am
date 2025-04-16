# issue #3069: make sure to install correctly when cross compiling
modexeclibdir=${libdir}/${mymodverdir}

MNAME=logger_bin

if COND_MINGWCC
LOGGER_QPP=$(top_srcdir)/lib/qpp_native
else
LOGGER_QPP=$(top_srcdir)/lib/qpp
endif

dummy:
	echo "Build started!"

.qpp.cpp:
	$(LOGGER_QPP) -V $<

GENERATED_SOURCES = QC_LoggerPattern.cpp \
    QC_LoggerLevel.cpp \
    QC_LoggerEvent.cpp \
    QC_LoggerFilter.cpp \
    QC_LoggerLayout.cpp \
    QC_LoggerLayoutPattern.cpp \
    QC_LoggerAppender.cpp \
    QC_LoggerAppenderQueue.cpp \
    QC_LoggerAppenderWithLayout.cpp \
    QC_LoggerAppenderFile.cpp \
    QC_LoggerInterface.cpp \
    QC_Logger.cpp

CLEANFILES = $(GENERATED_SOURCES)
if COND_SINGLE_COMPILATION_UNIT
LOGGER_SOURCES = logger_scu.cpp
logger_scu.cpp: $(GENERATED_SOURCES)
else
LOGGER_SOURCES = logger-module.cpp \
    QoreLoggerLevel.cpp \
    QoreLoggerPattern.cpp \
    QoreLoggerLayoutPattern.cpp \
    QoreLoggerEvent.cpp \
    QoreLoggerAppender.cpp \
    QoreLoggerAppenderQueue.cpp \
    QoreLoggerAppenderWithLayout.cpp \
    QoreLoggerAppenderFile.cpp \
    QoreLogger.cpp
nodist_logger_bin_la_SOURCES = $(GENERATED_SOURCES)
endif

modexeclib_LTLIBRARIES = logger_bin.la
logger_bin_la_SOURCES = $(LOGGER_SOURCES)
logger_bin_la_LDFLAGS = -module -avoid-version ${LOGGER_LIBS} ${MODULE_LDFLAGS}

LOGGER_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/modules/logger_bin/src $(QORE_LIB_CPPFLAGS)

AM_CPPFLAGS=$(QORE_CPPFLAGS) $(LOGGER_CPPFLAGS)

if COND_PROFILE
AM_CXXFLAGS=-g -pg
else
if COND_DEBUG
AM_CXXFLAGS=-g
endif
endif

if COND_MINGWCC
RM_OTHER=$(DESTDIR)$(modexeclibdir)/$(MNAME).dll.a
endif

install-exec-hook:
	mv $(DESTDIR)$(modexeclibdir)/$(MNAME).$(MODULE_SUFFIX) $(DESTDIR)$(modexeclibdir)/$(MNAME)-api-$(MODULE_API).qmod
	$(RM) $(DESTDIR)$(modexeclibdir)/$(MNAME).la $(DESTDIR)$(modexeclibdir)/$(MNAME).a $(RM_OTHER)

clean-local:
	$(RM) -r $(GENERATED_SOURCES)
	$(RM) *.qtest

uninstall-local:
	$(RM) $(DESTDIR)$(modexeclibdir)/$(MNAME)-api-$(MODULE_API).qmod
