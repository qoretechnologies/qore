moddir = $(userlibdir)

AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

ACLOCAL_AMFLAGS=-I m4

if COND_MINGWCC
MODULE_QPP=$(top_srcdir)/lib/qpp_native
else
MODULE_QPP=$(top_srcdir)/lib/qpp
endif

DOX=docs/mainpage.dox

all: ${SUBDIRS} dox

dox: ${DOX}

docs/mainpage.dox: docs/mainpage.dox.tmpl $(MODULE_QPP)
	$(MODULE_QPP) -t $< -o $@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

clean:
	${RM} ${DOX}
	$(MAKE) -C $(SUBDIRS) clean
