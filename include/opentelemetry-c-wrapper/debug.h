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
#ifndef OPENTELEMETRY_C_WRAPPER_DEBUG_H
#define OPENTELEMETRY_C_WRAPPER_DEBUG_H

__CPLUSPLUS_DECL_BEGIN

#if defined(DEBUG) || defined(DEBUG_OTEL)
#define  OTELC_DBG_LEVEL_DEFINES                                                                  \
	OTELC_DBG_LEVEL_DEF(LOG)     /* Low-level logging infrastructure messages. */             \
	OTELC_DBG_LEVEL_DEF(FUNC)    /* Function entry/exit and call-tracing messages. */         \
	OTELC_DBG_LEVEL_DEF(ERROR)   /* Error conditions requiring attention. */                  \
	OTELC_DBG_LEVEL_DEF(WARNING) /* Warning conditions that may indicate a problem. */        \
	OTELC_DBG_LEVEL_DEF(INFO)    /* High-level informational messages. */                     \
	OTELC_DBG_LEVEL_DEF(NOTICE)  /* Noteworthy but non-error conditions. */                   \
	OTELC_DBG_LEVEL_DEF(DEBUG)   /* General-purpose debugging messages. */                    \
	OTELC_DBG_LEVEL_DEF(MEM)     /* Memory allocation, deallocation, and leak diagnostics. */ \
	OTELC_DBG_LEVEL_DEF(OTEL)    /* OpenTelemetry C++ client library diagnostics. */          \
	OTELC_DBG_LEVEL_DEF(OTELC)   /* OpenTelemetry C wrapper layer diagnostics. */             \
	OTELC_DBG_LEVEL_DEF(WORKER)  /* Background worker thread activity and lifecycle. */

/***
 * NOTE: Each OTELC_DBG_LEVEL_* value is duplicated as OTELC_DBG_LEVEL__* and
 * OTELC_DBG_LEVEL___*, all sharing the same value.  This was done to allow the
 * use of OTELC_DBG_LEVEL_* in a macro that invokes another macro using the same
 * value for the debug level.  This is supported up to two levels of submacros.
 *
 * For example, see the definition of the OTELC_KV_DUMP() macro.
 */
#define OTELC_DBG_LEVEL_DEF(a)   OTELC_DBG_LEVEL_##a, OTELC_DBG_LEVEL__##a = OTELC_DBG_LEVEL_##a, OTELC_DBG_LEVEL___##a = OTELC_DBG_LEVEL_##a,
enum OTELC_DBG_LEVEL_enum {
        OTELC_DBG_LEVEL_DEFINES
	OTELC_DBG_LEVEL_MAX,
	OTELC_DBG_LEVEL_MASK = (UINT32_C(1) << OTELC_DBG_LEVEL_MAX) - 1
};
#undef OTELC_DBG_LEVEL_DEF

#  define OTELC_DBG_IFDEF(a,b)     a
#  define OTELC_DBG_INDENT_STEP    2
#  define OTELC_DBG_INDENT         otelc_dbg_indent, "                                        > "

#  if defined(USE_THREADS) || defined(USE_THREAD)
#    define OTELC_DBG_FMT(f)       "[%*d] %11.6f %.*s" f, otelc_dbg_tid_width, otelc_ext_thread_id(), otelc_runtime() / 1e6, OTELC_DBG_INDENT
#  else
#    define OTELC_DBG_FMT(f)       "%11.6f %.*s" f, otelc_runtime() / 1e6, OTELC_DBG_INDENT
#  endif

#  define OTELC_LOG(s,f, ...)      (void)fprintf((s), OTELC_DBG_FMT(f "\n"), ##__VA_ARGS__)
#  define OTELC_DBG(l,f, ...)                                     \
	do {                                                      \
		if (otelc_dbg_level & (1 << OTELC_DBG_LEVEL_##l)) \
			OTELC_LOG(stdout, f, ##__VA_ARGS__);      \
	} while (0)
#  define OTELC_DBG_STRUCT(l,f,F,p, ...)                        \
	do {                                                    \
		if ((p) == NULL)                                \
			OTELC_DBG(_##l, f " %p:{ }", (p));      \
		else                                            \
			OTELC_DBG(_##l, F, (p), ##__VA_ARGS__); \
	} while (0)
#  define OTELC_FUNC_EX(l,f, ...)                                        \
	const int dbg_ = otelc_dbg_level & (1 << OTELC_DBG_LEVEL_##l);   \
	do {                                                             \
		OTELC_DBG(_##l, "%s(" f ") {", __func__, ##__VA_ARGS__); \
		if (dbg_)                                                \
			otelc_dbg_indent += OTELC_DBG_INDENT_STEP;       \
	} while (0)
#  define OTELC_FUNC(f, ...)       OTELC_FUNC_EX(FUNC, f, ##__VA_ARGS__)
#  define OTELC_FUNCPP_EX(l,f,c, ...)                                           \
	const int dbg_ = otelc_dbg_level & (1 << OTELC_DBG_LEVEL_##l);          \
	do {                                                                    \
		OTELC_DBG(_##l, "%s::%s(" f ") {", c, __func__, ##__VA_ARGS__); \
		if (dbg_)                                                       \
			otelc_dbg_indent += OTELC_DBG_INDENT_STEP;              \
	} while (0)
#  define OTELC_FUNCPP(f,c, ...)   OTELC_FUNCPP_EX(FUNC, f, c, ##__VA_ARGS__)
#  define OTELC_FUNC_END(f, ...)                                   \
	do {                                                       \
		if (dbg_) {                                        \
			otelc_dbg_indent -= OTELC_DBG_INDENT_STEP; \
			OTELC_LOG(stdout, f, ##__VA_ARGS__);       \
		}                                                  \
	} while (0)
#  define OTELC_RETURN()           do { OTELC_FUNC_END("}"); return; } while (0)
#  define OTELC_RETURN_EX(a,t,f)   do { t r_ = (a); OTELC_FUNC_END("} = " f, r_); return r_; } while (0)
#  define OTELC_RETURN_PTR(a)      OTELC_RETURN_EX((a), typeof(a), "%p")
#  define OTELC_RETURN_ENUM(a)     OTELC_RETURN_EX((a), typeof(a), "%d")
#  define OTELC_RETURN_INT(a)      OTELC_RETURN_EX((a), int, "%d")


extern __thread int otelc_dbg_indent;
extern int          otelc_dbg_level;
extern int          otelc_dbg_tid_width;
extern bool         otelc_dbg_trigger_throw;

#else

#  define OTELC_DBG_IFDEF(a,b)     b
#  define OTELC_LOG(s,f, ...)      (void)fprintf((s), f "\n", ##__VA_ARGS__)
#  define OTELC_DBG(...)           while (0)
#  define OTELC_DBG_STRUCT(...)    while (0)
#  define OTELC_FUNC_EX(...)       while (0)
#  define OTELC_FUNC(...)          while (0)
#  define OTELC_FUNCPP_EX(...)     while (0)
#  define OTELC_FUNCPP(...)        while (0)
#  define OTELC_FUNC_END(...)      while (0)
#  define OTELC_RETURN()           return
#  define OTELC_RETURN_EX(a,t,f)   return a
#  define OTELC_RETURN_PTR(a)      return a
#  define OTELC_RETURN_ENUM(a)     return a
#  define OTELC_RETURN_INT(a)      return a
#endif /* DEBUG || DEBUG_OTEL */

#define OTELC_DBG_ARGS             OTELC_DBG_IFDEF(OTELC_ARGS(__func__, __LINE__, ), )

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_DEBUG_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
