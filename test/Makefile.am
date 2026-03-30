## Process this file with automake to produce Makefile.in
##
AM_CPPFLAGS = $(COMMON_CPPFLAGS) @OPENTELEMETRY_C_WRAPPER_CPPFLAGS@ -DOTELC_PACKAGE_BUILD=`cat $(srcdir)/../src/.build-counter`
  AM_CFLAGS = @OPENTELEMETRY_C_WRAPPER_CFLAGS@
 AM_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@

ymldir   = $(docdir)
yml_DATA = otel-cfg.yml

if WANT_DEBUG
                  TEST_BIN_NAME = otel-c-wrapper-test_dbg
                 check_PROGRAMS = otel-c-wrapper-test_dbg test-tracer_dbg test-meter_dbg test-logger_dbg test-yaml_dbg
otel_c_wrapper_test_dbg_SOURCES = test.c opentelemetry.c
  otel_c_wrapper_test_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la -lm
otel_c_wrapper_test_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
        test_tracer_dbg_SOURCES = test-tracer.c test-util.c opentelemetry.c
          test_tracer_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la
        test_tracer_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
         test_meter_dbg_SOURCES = test-meter.c test-util.c
           test_meter_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la
         test_meter_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
        test_logger_dbg_SOURCES = test-logger.c test-util.c
          test_logger_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la
        test_logger_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
          test_yaml_dbg_SOURCES = test-yaml.cpp test-util.c
         test_yaml_dbg_CPPFLAGS = $(AM_CPPFLAGS) @OPENTELEMETRY_CPPFLAGS@ @LIBFYAML_CPPFLAGS@ @RAPIDYAML_CPPFLAGS@
            test_yaml_dbg_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper_dbg.la @ABSL_LIBS@
          test_yaml_dbg_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@

else

              TEST_BIN_NAME = otel-c-wrapper-test
             check_PROGRAMS = otel-c-wrapper-test test-tracer test-meter test-logger test-yaml
otel_c_wrapper_test_SOURCES = test.c opentelemetry.c
  otel_c_wrapper_test_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la -lm
otel_c_wrapper_test_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
        test_tracer_SOURCES = test-tracer.c test-util.c opentelemetry.c
          test_tracer_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la
        test_tracer_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
         test_meter_SOURCES = test-meter.c test-util.c
           test_meter_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la
         test_meter_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
        test_logger_SOURCES = test-logger.c test-util.c
          test_logger_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la
        test_logger_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
          test_yaml_SOURCES = test-yaml.cpp test-util.c
         test_yaml_CPPFLAGS = $(AM_CPPFLAGS) @OPENTELEMETRY_CPPFLAGS@ @LIBFYAML_CPPFLAGS@ @RAPIDYAML_CPPFLAGS@
            test_yaml_LDADD = @OPENTELEMETRY_C_WRAPPER_LIBS@ $(top_builddir)/src/libopentelemetry-c-wrapper.la
          test_yaml_LDFLAGS = @OPENTELEMETRY_C_WRAPPER_LDFLAGS@
endif

install-exec-local:
	@if test -f "$(TEST_BIN_NAME)$(EXEEXT)"; then \
	    $(MKDIR_P) "$(DESTDIR)$(bindir)"; \
	    $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install \
	        $(INSTALL_PROGRAM) "$(TEST_BIN_NAME)$(EXEEXT)" "$(DESTDIR)$(bindir)/$(TEST_BIN_NAME)$(EXEEXT)"; \
	fi

uninstall-local:
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=uninstall \
	    rm -f "$(DESTDIR)$(bindir)/$(TEST_BIN_NAME)$(EXEEXT)"

CLEANFILES = a.out
##
## Makefile.am ends here
