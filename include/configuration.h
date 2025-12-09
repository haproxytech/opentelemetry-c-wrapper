/***
 * Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _OPENTELEMETRY_C_WRAPPER_CONFIGURATION_H_
#define _OPENTELEMETRY_C_WRAPPER_CONFIGURATION_H_

/***
 * The OTEL_CPP_USE_DEBUG_OUTPUT definition is to prevent this library from
 * being misconfigured if the header files from the opentelemetry-cpp package
 * have been changed (i.e. debug output has been added to them).  It is not
 * used anywhere in this source code and it doesn't matter if it remains
 * defined like this.
 */
#ifdef DEBUG
#  define OTEL_CPP_USE_DEBUG_OUTPUT
#  define OTELC_DBG_MEM
#endif

/***
 * Prevents the automatic definition of default OTELC_USE_* configuration
 * macros.  Define this macro when providing a fully custom configuration.
 *
 * OTELC_USE_RUNTIME_CONTEXT enables the OTel RuntimeContext mechanism.  When
 * defined, the active span is pushed into RuntimeContext via Scope (write)
 * and retrieved via GetCurrent() (read) for implicit parent span lookup,
 * carrier extraction, and metric exemplar correlation.
 */
#ifndef OTELC_USER_CONFIG
#  define OTELC_USE_THREAD_SHARED_HANDLE
#  undef  OTELC_USE_STATIC_HANDLE
#  define OTELC_USE_COMPOSITE_PROPAGATOR
#  define OTELC_USE_MULTIPLE_PROCESSORS
#  undef  OTELC_USE_RUNTIME_CONTEXT
#endif

/* This is not configurable. */
#define OTELC_USE_INTERNAL_INCLUDES

#ifdef OTELC_USE_THREAD_SHARED_HANDLE
#  define OTELC_USE_THREAD_SHARED_HANDLE_IFDEF(a,b)   a
#else
#  define OTELC_USE_THREAD_SHARED_HANDLE_IFDEF(a,b)   b
#endif
#ifdef OTELC_USE_STATIC_HANDLE
#  define OTELC_USE_STATIC_HANDLE_IFDEF(a,b)          a
#else
#  define OTELC_USE_STATIC_HANDLE_IFDEF(a,b)          b
#endif
#ifdef OTELC_USE_MULTIPLE_PROCESSORS
#  define OTELC_USE_MULTIPLE_PROCESSORS_IFDEF(a,b)    a
#else
#  define OTELC_USE_MULTIPLE_PROCESSORS_IFDEF(a,b)    b
#endif
#ifdef OTELC_USE_RUNTIME_CONTEXT
#  define OTELC_USE_RUNTIME_CONTEXT_IFDEF(a,b)        a
#else
#  define OTELC_USE_RUNTIME_CONTEXT_IFDEF(a,b)        b
#endif
#ifdef OTELC_USE_COMPOSITE_PROPAGATOR
#  define OTELC_USE_COMPOSITE_PROPAGATOR_IFDEF(a,b)   a
#else
#  define OTELC_USE_COMPOSITE_PROPAGATOR_IFDEF(a,b)   b
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_CONFIGURATION_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
