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
#ifndef _OPENTELEMETRY_C_WRAPPER_DEBUG_H_
#define _OPENTELEMETRY_C_WRAPPER_DEBUG_H_

#if defined(DEBUG) || defined(DEBUG_OTEL)
#  define OTELCPP_FUNC_EX(l,f,c, ...)                                           \
	const int dbg_ = otelc_dbg_level & (1 << OTELC_DBG_LEVEL_##l);          \
	do {                                                                    \
		OTELC_DBG(_##l, "%s::%s(" f ") {", c, __func__, ##__VA_ARGS__); \
		if (dbg_)                                                       \
			otelc_dbg_indent += OTELC_DBG_INDENT_STEP;              \
	} while (0)
#  define OTELCPP_FUNC(f,c, ...)   OTELCPP_FUNC_EX(FUNC, f, c, ##__VA_ARGS__)
#  define OTELCPP_RETURN_OBJ(a)    do { __typeof__(a) r_ = (a); OTELC_FUNC_END("} = %p", &r_); return r_; } while (0)
#  define OTELCPP_RETURN_ENUM(a)   do { __typeof__(a) r_ = (a); OTELC_FUNC_END("} = %d", static_cast<int>(r_)); return r_; } while (0)

#else

#  define OTELCPP_FUNC_EX(...)     while (0)
#  define OTELCPP_FUNC(...)        while (0)
#  define OTELCPP_RETURN_OBJ(a)    return a
#  define OTELCPP_RETURN_ENUM(a)   return a
#endif /* DEBUG || DEBUG_OTEL */

#endif /* _OPENTELEMETRY_C_WRAPPER_DEBUG_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
