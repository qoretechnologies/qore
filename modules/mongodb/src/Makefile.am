# issue #3069: make sure to install correctly when cross compiling
modexeclibdir=${libdir}/${mymodverdir}

MNAME=mongodb

if COND_MINGWCC
MONGODB_QPP=$(top_srcdir)/lib/qpp_native
else
MONGODB_QPP=$(top_srcdir)/lib/qpp
endif

dummy:
	echo "Build started!"

QC_MongoClient.cpp: QC_MongoClient.qpp
	$(MONGODB_QPP) -V $<

QC_MongoDatabase.cpp: QC_MongoDatabase.qpp
	$(MONGODB_QPP) -V $<

QC_MongoCollection.cpp: QC_MongoCollection.qpp
	$(MONGODB_QPP) -V $<

QC_MongoCursor.cpp: QC_MongoCursor.qpp
	$(MONGODB_QPP) -V $<

QC_ObjectId.cpp: QC_ObjectId.qpp
	$(MONGODB_QPP) -V $<

ql_mongodb.cpp: ql_mongodb.qpp
	$(MONGODB_QPP) -V $<

GENERATED_SOURCES = QC_MongoClient.cpp QC_MongoDatabase.cpp QC_MongoCollection.cpp \
	QC_MongoCursor.cpp QC_ObjectId.cpp ql_mongodb.cpp
CLEANFILES = $(GENERATED_SOURCES)

if COND_SINGLE_COMPILATION_UNIT
MONGODB_SOURCES = mongodb_scu.cpp
mongodb_scu.cpp: $(GENERATED_SOURCES)
else
MONGODB_SOURCES = mongodb-module.cpp bson_conversion.cpp MongoClient.cpp \
	MongoDatabase.cpp MongoCollection.cpp MongoCursor.cpp ObjectId.cpp
nodist_mongodb_la_SOURCES = $(GENERATED_SOURCES)
endif

modexeclib_LTLIBRARIES = mongodb.la
mongodb_la_SOURCES = $(MONGODB_SOURCES)
mongodb_la_LDFLAGS = -module -avoid-version ${MONGODB_LIBS} ${MODULE_LDFLAGS}

MONGODB_MODULE_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/modules/mongodb/src $(QORE_LIB_CPPFLAGS) @MONGODB_CPPFLAGS@

AM_CPPFLAGS=$(QORE_CPPFLAGS) $(MONGODB_MODULE_CPPFLAGS)

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
