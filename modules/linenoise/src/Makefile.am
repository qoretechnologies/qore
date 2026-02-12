# issue #3069: make sure to install correctly when cross compiling
modexeclibdir=${libdir}/${mymodverdir}

MNAME=linenoise

if COND_MINGWCC
LINENOISE_QPP=$(top_srcdir)/lib/qpp_native
else
LINENOISE_QPP=$(top_srcdir)/lib/qpp
endif

dummy:
	echo "Build started!"

.qpp.cpp:
	$(LINENOISE_QPP) -V $<

GENERATED_SOURCES = QC_AbstractHistoryProvider.cpp \
    ql_linenoise.cpp

CLEANFILES = $(GENERATED_SOURCES)

LINENOISE_SOURCES = linenoise/ConvertUTF.cpp linenoise/linenoise.cpp linenoise/wcwidth.cpp \
    linenoise-module.cpp
nodist_linenoise_la_SOURCES = $(GENERATED_SOURCES)

modexeclib_LTLIBRARIES = linenoise.la
linenoise_la_SOURCES = $(LINENOISE_SOURCES)
linenoise_la_LDFLAGS = -module -avoid-version ${LINENOISE_LIBS} ${MODULE_LDFLAGS}

LINENOISE_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/modules/linenoise/src -I$(top_srcdir)/modules/linenoise/src/linenoise -DUSE_UTF8 $(QORE_LIB_CPPFLAGS)

AM_CPPFLAGS=$(QORE_CPPFLAGS) $(LINENOISE_CPPFLAGS)

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
