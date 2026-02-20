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
#ifndef OPENTELEMETRY_C_WRAPPER_DEFINE_H
#define OPENTELEMETRY_C_WRAPPER_DEFINE_H

#ifdef __cplusplus
#  define __CPLUSPLUS_DECL_BEGIN   extern "C" {
#  define __CPLUSPLUS_DECL_END     }
#else
#  define __CPLUSPLUS_DECL_BEGIN
#  define __CPLUSPLUS_DECL_END
#endif

#ifdef __GNUC__
#  define OTELC_NONNULL(...)       __attribute__((nonnull(__VA_ARGS__)))
#  define OTELC_NONNULL_ALL        __attribute__((nonnull))
#else
#  define OTELC_NONNULL(...)
#  define OTELC_NONNULL_ALL
#endif

#define OTELC_SCOPE_VERSION        "1.2.0"
#define OTELC_SCOPE_SCHEMA_URL     "https://opentelemetry.io/schemas/1.2.0"

#define OTELC_SPAN_ID_SIZE         8
#define OTELC_TRACE_ID_SIZE        16

#define OTELC_RET_ERROR            -1
#define OTELC_RET_OK               0

#define OTELC_STRINGIFY_ARG(a)     #a
#define OTELC_STRINGIFY(a)         OTELC_STRINGIFY_ARG(a)

#define OTELC_TABLESIZE(a)         (sizeof(a) / sizeof((a)[0]))
#define OTELC_TABLESIZE_N(a,n)     (OTELC_TABLESIZE(a) - (n))
#define OTELC_TABLESIZE_1(a)       OTELC_TABLESIZE_N((a), 1)

/* Type-safe MIN/MAX using GCC statement expressions. */
#define OTELC_MIN(a,b)             ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a <= _b) ? _a : _b; })
#define OTELC_MAX(a,b)             ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a >= _b) ? _a : _b; })
#define OTELC_IN_RANGE(v,a,b)      ({ __typeof__(v) _v = (v); __typeof__(a) _a = (a); __typeof__(b) _b = (b); (_v >= _a) && (_v <= _b); })

/***
 * OTELC_OPS(ptr, func, ...)  - call ptr->ops->func(ptr, ...).
 * OTELC_OPSR(ptr, func, ...) - call ptr->ops->func(&ptr, ...).
 *
 * The "R" (reference) variant passes the address of the handle so the callee
 * can set it to NULL (e.g. destroy / end operations).
 */
#define OTELC_OPSR(p,f, ...)       ({ __typeof__(&(p)) _p = &(p); (*_p)->ops->f(_p, ##__VA_ARGS__); })
#define OTELC_OPS(p,f, ...)        ({ __typeof__(p) _p = (p); _p->ops->f(_p, ##__VA_ARGS__); })

#define OTELC_ARGS(a, ...)         a, ##__VA_ARGS__
#define OTELC_DPTR_ARGS(p)         (p), ((p) == NULL) ? NULL : *(p)
#define OTELC_STR_ARG(s)           ((s) == NULL) ? "(null)" : (s)
#define OTELC_TV_ARGS(p)           (p)->tv_sec, (p)->tv_nsec
#define OTELC_RUNTIME_MS()         ((otelc_runtime() + 500) / 1000)
#define OTELC_STR_IS_VALID(s)      (((s) != NULL) && (*(s) != '\0'))

/* Type for deferred functions. */
typedef void (*otelc_defer_func_t)(void);

/* Cleanup function called by GCC/Clang __attribute__((cleanup)). */
OTELC_NONNULL_ALL static inline void otelc_defer_run(otelc_defer_func_t *f)
{
	(*f)();
}

/* Helpers to create unique variable names */
#define OTELC_DEFER_JOIN(a,b)      a##b
#define OTELC_DEFER_NAME(a,b)      OTELC_DEFER_JOIN(a, b)

/*
 * OTELC_DEFER(cleanup_action)
 *
 * Executes the given cleanup_action when leaving the current scope.
 *
 * Example:
 *   int *ptr = malloc(sizeof(int));
 *   OTELC_DEFER(free(ptr));
 *
 *   FILE *fd = fopen("file.txt", "r");
 *   OTELC_DEFER(fclose(fd));
 *
 * Notes:
 * - cleanup_action is a GNU C statement expression; can contain multiple
 *   statements using braces.
 * - Cleanup runs in reverse order of declaration.
 */
#define OTELC_DEFER(cleanup_action)                                                                       \
	__attribute__((cleanup(otelc_defer_run))) otelc_defer_func_t OTELC_DEFER_NAME(defer_, __LINE__) = \
		(otelc_defer_func_t)({ void otelc_defer_func(void) { cleanup_action; } otelc_defer_func; })

#endif /* OPENTELEMETRY_C_WRAPPER_DEFINE_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
