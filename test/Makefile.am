## Process this file with automake to produce Makefile.in
##
AM_CPPFLAGS = $(COMMON_CPPFLAGS) @OPENTELEMETRY_C_WRAPPER_CPPFLAGS@ -DOTELC_PACKAGE_BUILD=`cat $(srcdir)/../src/.build-counter`
  AM_CFLAGS = @OPENTELEMETRY_C_WRAPPER_CFLAGS@
 AM_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@

ymldir   = $(docdir)
yml_DATA = otel-cfg.yml

if WANT_DEBUG
                   bin_PROGRAMS = otel-c-wrapper-test_dbg
otel_c_wrapper_test_dbg_SOURCES = test.c opentelemetry.c
  otel_c_wrapper_test_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la -lm
otel_c_wrapper_test_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@

else

               bin_PROGRAMS = otel-c-wrapper-test
otel_c_wrapper_test_SOURCES = test.c opentelemetry.c
  otel_c_wrapper_test_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la -lm
otel_c_wrapper_test_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
endif

CLEANFILES = a.out
##
## Makefile.am ends here
