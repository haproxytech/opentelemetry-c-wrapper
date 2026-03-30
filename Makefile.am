## Process this file with automake to produce Makefile.in
##
SUBDIRS         = src test
DIST_SUBDIRS    = src test
DISTCLEANFILES  = _* *~
CLEANFILES      = a.out core gmon.out
ACLOCAL_AMFLAGS = -I m4
doc_DATA        = AUTHORS COPYING ChangeLog INSTALL LICENSE MAINTAINERS NEWS README TODO VERSION

LIBTOOL_DEPS    = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

.PHONY: test
test:
	$(MAKE) check
##
## Makefile.am ends here
