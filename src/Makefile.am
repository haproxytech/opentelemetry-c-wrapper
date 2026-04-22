## Process this file with automake to produce Makefile.in
##
##
AM_CPPFLAGS = $(COMMON_CPPFLAGS) @OPENTELEMETRY_CPPFLAGS@ @LIBFYAML_CPPFLAGS@ @RAPIDYAML_CPPFLAGS@ -DOTELC_PACKAGE_BUILD=`cat $(srcdir)/../src/.build-counter`
  AM_CFLAGS = @OPENTELEMETRY_CFLAGS@   @LIBFYAML_CFLAGS@
AM_CXXFLAGS = @OPENTELEMETRY_CXXFLAGS@                    @RAPIDYAML_CXXFLAGS@
 AM_LDFLAGS = @OPENTELEMETRY_LDFLAGS@  @LIBFYAML_LDFLAGS@ @RAPIDYAML_LDFLAGS@
    AM_LIBS = @OPENTELEMETRY_LIBS@     @LIBFYAML_LIBS@    @RAPIDYAML_LIBS@

if WANT_DEBUG
lib_LTLIBRARIES = libopentelemetry-c-wrapper_dbg.la
pkgconfig_DATA  = ../opentelemetry-c-wrapper_dbg.pc

libopentelemetry_c_wrapper_dbg_la_CPPFLAGS = $(AM_CPPFLAGS)
libopentelemetry_c_wrapper_dbg_la_CFLAGS   = $(AM_CFLAGS)
libopentelemetry_c_wrapper_dbg_la_CXXFLAGS = $(AM_CXXFLAGS)
libopentelemetry_c_wrapper_dbg_la_LDFLAGS  = $(AM_LDFLAGS) @OPENTELEMETRY_SYSLDFLAGS@ -version-info @LIB_VERSION@ -Wl,--version-script=$(srcdir)/export_dbg.map -Wl,-z,nodelete
libopentelemetry_c_wrapper_dbg_la_LIBADD   = $(AM_LIBS) @OPENTELEMETRY_SYSLIBS@
libopentelemetry_c_wrapper_dbg_la_SOURCES  = \
	dbg_malloc.cpp \
	exporter.cpp \
	logger.cpp \
	meter.cpp \
	processor.cpp \
	provider.cpp \
	resource.cpp \
	sampler.cpp \
	span.cpp \
	threads.cpp \
	tracer.cpp \
	util.cpp \
	yaml.cpp \
	version.cpp

else

lib_LTLIBRARIES = libopentelemetry-c-wrapper.la
pkgconfig_DATA  = ../opentelemetry-c-wrapper.pc

libopentelemetry_c_wrapper_la_CPPFLAGS = $(AM_CPPFLAGS)
libopentelemetry_c_wrapper_la_CFLAGS   = $(AM_CFLAGS)
libopentelemetry_c_wrapper_la_CXXFLAGS = $(AM_CXXFLAGS)
libopentelemetry_c_wrapper_la_LDFLAGS  = $(AM_LDFLAGS) @OPENTELEMETRY_SYSLDFLAGS@ -version-info @LIB_VERSION@ -Wl,--version-script=$(srcdir)/export.map -Wl,-z,nodelete
libopentelemetry_c_wrapper_la_LIBADD   = $(AM_LIBS) @OPENTELEMETRY_SYSLIBS@
libopentelemetry_c_wrapper_la_SOURCES  = \
	exporter.cpp \
	logger.cpp \
	meter.cpp \
	processor.cpp \
	provider.cpp \
	resource.cpp \
	sampler.cpp \
	span.cpp \
	threads.cpp \
	tracer.cpp \
	util.cpp \
	yaml.cpp \
	version.cpp
endif

pkginclude_HEADERS = \
	../include/opentelemetry-c-wrapper/dbg_malloc.h \
	../include/opentelemetry-c-wrapper/debug.h \
	../include/opentelemetry-c-wrapper/define.h \
	../include/opentelemetry-c-wrapper/include.h \
	../include/opentelemetry-c-wrapper/logger.h \
	../include/opentelemetry-c-wrapper/meter.h \
	../include/opentelemetry-c-wrapper/otel_cpp.h \
	../include/opentelemetry-c-wrapper/propagation.h \
	../include/opentelemetry-c-wrapper/span.h \
	../include/opentelemetry-c-wrapper/tracer.h \
	../include/opentelemetry-c-wrapper/util.h \
	../include/opentelemetry-c-wrapper/version.h


CLEANFILES = a.out

BUILT_SOURCES = build-counter

.PHONY: build-counter

build-counter:
	$(AM_V_GEN)$(srcdir)/../scripts/build-counter

clean-local:
##
## Makefile.am ends here
